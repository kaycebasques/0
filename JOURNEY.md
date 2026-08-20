# The Embedded Development Journey: Building Bare-Metal RP2040 Firmware with Bazel & Pigweed

This document provides a detailed, step-by-step technical chronicle of how we built, debugged, and verified a bare-metal C application for the **Raspberry Pi Pico (RP2040 microcontroller)** from scratch using **Bazel** and **Google Pigweed's hermetic toolchain**, verified using the **Wokwi VS Code Simulator**.

---

## 1. Project Initialization & Architecture Simplification

### Initial Setup
We began with a bare-metal C project targeting the **RP2350 (Raspberry Pi Pico 2)**. To ensure maximum clarity for engineers learning embedded development, we stripped away unnecessary driver abstractions, UART headers, and standard C library runtimes (`libc`, `crt0.o`).

### Target Hardware Switch: RP2350 -> RP2040
To support live visual simulation using the **Wokwi VS Code extension**, we transitioned the project target platform from RP2350 (ARM Cortex-M33 / `armv8-m`) to **RP2040 (ARM Cortex-M0+ / `armv6-m`)**, which is natively supported by Wokwi (`wokwi-pi-pico`).

---

## 2. Technical Challenges & Root Cause Breakthroughs

### Challenge 1: The RP2040 ROM Bootloader & `.boot2` Sector
* **Symptom**: Wokwi simulator rejected the binary and dropped into USB BOOTSEL mode.
* **Root Cause**: The RP2040 internal ROM bootloader runs from SRAM on power-up and requires a 256-byte Stage 2 Boot Sector (`.boot2`) at `0x10000000`. The last 4 bytes of this sector must contain a valid **CRC32-MPEG2 checksum** of the preceding 252 bytes.
* **Resolution**: Embedded the official Winbond `boot2_w25q080` SPI Flash Boot Stage 2 driver payload ending with checksum `0x7a4eb274` (`0xde, 0x6a, 0xa3, 0x9e`).

---

### Challenge 2: Cross-Machine Bazel Hermeticity & Toolchain Resolution
* **Symptom**: Building on a secondary machine (`kayce1`) failed with `Unable to find a CC toolchain using toolchain resolution`.
* **Root Cause**: Running `bazelisk build //:app` without an explicit `--config` flag attempted compilation for the host machine (`x86_64`). Because local host C++ compiler detection was disabled (`BAZEL_DO_NOT_DETECT_CPP_TOOLCHAIN=1`), Bazel threw a toolchain resolution error.
* **Resolution**: 
  1. Created `.bazelversion` pinning Bazel `8.7.0`.
  2. Set default `--platforms=//targets:rp2040` in `.bazelrc` so `bazelisk build //:app` works out of the box without requiring flags.

---

### Challenge 3: UF2 Generator Memory Address Alignment (`LMA` vs `VMA`)
* **Symptom**: Simulator ran at < 1% speed or threw `Invalid magic value` errors.
* **Root Cause 1 (Symlink Resolution)**: `bazel-bin` is a symlink to `~/.cache/bazel/...`. Wokwi's extension webview server could not resolve paths outside the workspace root directory.
* **Root Cause 2 (Flash LMA vs RAM VMA)**: In ELF binaries, initialized data sections have two addresses:
  - **VMA (`p_vaddr`)**: Virtual Memory Address in RAM (`0x200000c0`).
  - **LMA (`p_paddr`)**: Physical Load Memory Address in FLASH (`0x10002898`).
  Our custom Python UF2 generator originally used `p_vaddr` (RAM) instead of `p_paddr` (FLASH). The `.data` section was flashed into RAM, leaving FLASH memory empty. On startup, the C runtime zeroed RAM, overwriting data pointers and triggering HardFault loops.
* **Resolution**: 
  1. Updated the Python UF2 generator rule in `src/BUILD.bazel` to map loadable segments using Physical Load Address (`p_paddr`).
  2. Created `./build.sh` to copy `app.elf` and `app.uf2` into `build/`.
  3. Configured `wokwi.toml` with `firmware = "build/app.elf"` to load ELF binaries directly into virtual memory.

---

### Challenge 4: Parallel Reference Implementation (`//examples/wokwi`)
To establish a working reference baseline, we built **`//examples/wokwi`** using the official **Pico SDK (`pico-sdk` v2.0.0)** via the **Bazel Central Registry (BCR)**:
- Resolved third-party macro warnings by adding `common --per_file_copt=external/.*@-w` to `.bazelrc`.
- Verified working Pico SDK blinky using `gpio_init()`, `gpio_set_dir()`, and `gpio_put()`.

---

### Challenge 5: Section Execution Permissions (`"ax"`)
* **Symptom**: `//examples/wokwi` ran cleanly, but bare-metal `//src` still failed to boot.
* **Reverse-Engineering Discovery**: Performing a side-by-side section attribute audit using `readelf -S` revealed:
  ```
  [//examples/wokwi] .boot2: PROGBITS  10000000  Size: 0x100  Flags: AX (Alloc + EXECUTABLE)
  [//src]            .boot2: PROGBITS  10000000  Size: 0x100  Flags: A  (Alloc ONLY)
  ```
  In C, declaring `const uint8_t boot2_raw[256]` puts data in a read-only section (`A`). Memory protection hardware refused to execute instructions from a non-executable section.
* **Resolution**: Created `src/boot2.S` using assembly syntax `.section .boot2, "ax"` to explicitly flag `.boot2` as Executable (`x`). Simulation speed immediately jumped to 80%.

---

### Challenge 6: Chip Generation Register Offset Mismatches (RP2040 vs RP2350)
* **Symptom**: Code executed at 80% speed, but onboard LED (GPIO 25) remained OFF.
* **Reverse-Engineering Discovery**: Auditing `sio.h` headers between chip generations revealed that Single-Cycle I/O (SIO) register offsets differ between microcontrollers:
  - **RP2350 (Pico 2)**: `SIO_GPIO_OE_SET` = `0x38`, `SIO_GPIO_OUT_SET` = `0x18`.
  - **RP2040 (Pico 1)**: `SIO_GPIO_OE_SET` = **`0x24`**, `SIO_GPIO_OUT_SET` = **`0x14`** (`0x18` on RP2040 is `SIO_GPIO_OUT_CLR`!).
  Writing `(1 << 25)` to `0x38` on RP2040 targeted the FIFO status register (`SIO_FIFO_ST`), leaving output direction disabled. Writing to `0x18` explicitly turned the LED **OFF**.
* **Resolution**: Updated `src/main.c` SIO register definitions:
  ```c
  #define SIO_GPIO_OUT_SET REG(SIO_BASE + 0x0014)
  #define SIO_GPIO_OE_SET  REG(SIO_BASE + 0x0024)
  ```
  The onboard LED immediately illuminated solid green! 🎉

---

## 3. Key Concepts Learned

| Concept | Explanation |
| :--- | :--- |
| **MMIO & `volatile`** | Peripherals are controlled by writing to physical memory addresses (`0x40000000`). `volatile` prevents compilers from optimizing away register writes. |
| **Atomic Hardware Aliases** | RP2040 peripherals provide base (`+0x0000`), XOR (`+0x1000`), SET (`+0x2000`), and CLR (`+0x3000`) memory aliases for single-cycle atomic bit operations without race conditions. |
| **LMA vs VMA** | Linkers separate where code lives in Flash (Load Memory Address / LMA) from where it runs in RAM (Virtual Memory Address / VMA). |
| **ELF Section Flags** | Code sections must be explicitly flagged as Executable (`AX` / `"ax"`) in assembly or linker scripts for processor execution. |

---

## 4. Final Directory Structure

```
.
├── BUILD.bazel          # Root target aliases (//:app)
├── MODULE.bazel         # Bzlmod dependencies (pico-sdk, pigweed)
├── .bazelrc             # Hermetic compiler flags & target platform overrides
├── .bazelversion        # Pinned Bazel version (8.7.0)
├── build.sh             # Helper build & file copy script
├── diagram.json         # Wokwi VS Code circuit configuration
├── wokwi.toml           # Wokwi firmware & ELF binary paths
├── JOURNEY.md           # Project chronological history & technical guide
├── src/
│   ├── BUILD.bazel      # Bare-metal cc_binary & UF2 genrule
│   ├── boot2.S          # Executable ("ax") Winbond W25Q080 bootloader
│   ├── linker.ld        # Bare-metal ARM Cortex-M0+ memory layout
│   └── main.c           # Bare-metal MMIO register initialization & LED driver
└── examples/
    └── wokwi/           # Pico SDK baseline reference application
        ├── BUILD.bazel
        ├── MODULE.bazel
        ├── build.sh
        ├── diagram.json
        ├── main.c       # Pico SDK stdlib implementation (gpio_init, gpio_put)
        └── wokwi.toml
```
