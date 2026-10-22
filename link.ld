/* SPDX-License-Identifier: Apache-2.0 */
OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY { RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 1M }

SECTIONS {
  .text : { *(.text.init) *(.text .text.*) } > RAM
  .rodata : ALIGN(4) { *(.rodata .rodata.* .srodata .srodata.*) } > RAM
  .data : ALIGN(4) {
    *(.data .data.*)
    __global_pointer$ = . + 0x800;
    *(.sdata .sdata.*)
  } > RAM
  .bss (NOLOAD) : ALIGN(4) {
    __bss_start = .;
    *(.sbss .sbss.*) *(.bss .bss.*) *(COMMON)
    . = ALIGN(4);
    __bss_end = .;
  } > RAM
  . = ALIGN(16);
  . += 16K;
  __stack_top = .;
  /DISCARD/ : { *(.comment) *(.eh_frame) *(.riscv.attributes) }
}
