# project

the goal of this project is to create a truly bare-metal application for
the raspberry pi pico 2

note: // represents the root dir of this project. the dir containing
this file (PROJECT.md) is the root dir

## examples

the //examples dir provides a bunch of other libraries and example projects
to help you build our bare-metal app correctly

you must never depend on anything in //examples. it is only there for information,
guidance, etc.

## datasheets

the //datasheets dir contains PDF datasheets for the pico 1, pico 2, rp2040
(MCU in the pico 1), and rp2350 (MCU in the pico 2)

## hardware

we will be using the version of the pico 2 WITHOUT WIFI. the wifi
version has different hardware configuration (e.g. the onboard LED is wired up
differently) so it's important to remember that we are using the non-wifi board

a lot of the code and information from the first generation pico 1 board
is still relevant and useful. but remember that we are developing against
the pico 2 board and therefore they may be differences

the MCU of the pico 2 is RP2350. it is quite different than the first generation
pico 1 MCU (RP2040). be careful

## deliverables

create //MODULE.bazel and build the project with bazel. see //examples/bazel
to understand the basic strucutre

pigweed provides a hermetic toolchain for the rp2350. you may use that. you
should depend on pigweed via MODULE.bazel, the same way that //examples/bazel depends on pigweed
you can access toolchain tools through bazelisk. see https://pigweed.dev/changelog/2026/03.html#access-toolchain-tools-through-bazelisk
do not depend on anything else from pigweed

create everything from scratch: linker script, hardware access, etc

put all app source code in //src unless there's a very good reason to put
it somewhere else

you do not need to set up flashing. we can use the manual drag-and-drop
workflow that the pico supports
