MEMORY { FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 30K   /* last 2 KB page keeps the baseline */
         RAM (rwx)  : ORIGIN = 0x20000000, LENGTH = 8K }
_estack = ORIGIN(RAM) + LENGTH(RAM);
SECTIONS {
  .text : { __app_start = .; KEEP(*(.isr_vector)) *(.text*) *(.rodata*) . = ALIGN(4); } > FLASH
  _sidata = LOADADDR(.data);
  .data : { _sdata = .; *(.data*) . = ALIGN(4); _edata = .; } > RAM AT > FLASH
  __app_end = LOADADDR(.data) + SIZEOF(.data);
  .bss (NOLOAD) : { _sbss = .; *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .; __stack_canary = .; . += 4; } > RAM
}
