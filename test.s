
.syntax unified
.cpu cortex-m0plus
.thumb
.section .text
_start:
    ldr r0, =0x10000100
    ldr r1, =0xe000ed08
    str r0, [r1]
    ldmia r0!, {r1, r2}
    msr msp, r1
    bx r2
    .align 2
    .word 0x10000100
    .word 0xe000ed08
