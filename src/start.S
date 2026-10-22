/* SPDX-License-Identifier: Apache-2.0 */
/* Reset entry: hart 0 runs, others park. Sets the stack, zeroes .bss and
 * jumps to main. QEMU loads the ELF straight into RAM, so .data is
 * already in place. */
    .section .text.init, "ax", @progbits
    .globl _start
_start:
    csrr  t0, mhartid
    bnez  t0, park
.option push
.option norelax
    la    gp, __global_pointer$
.option pop
    la    sp, __stack_top
    la    t0, __bss_start
    la    t1, __bss_end
1:  bgeu  t0, t1, 2f
    sw    zero, 0(t0)
    addi  t0, t0, 4
    j     1b
2:  call  main
    /* main returned: report its value as the exit status */
    call  qemu_exit
park:
    wfi
    j     park
