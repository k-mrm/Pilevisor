OUTPUT_ARCH("aarch64")
ENTRY(_start)

SECTIONS {
    . = 0x40000000;

    .text.boot : {
      KEEP(*(.text.boot))
    }

    . = ALIGN(4096);

    .text : {
      *(.text) *(.text.*)
    }

    .rodata : {
      *(.rodata) *(.rodata.*)
    }

    .data : {
      *(.data) *(.data.*)
    }

    . = ALIGN(4096);
    .bss : {
      __bss_start = .;
      *(.bss .bss.*)
      __bss_end = .;
    }

    /DISCARD/ : {
      *(.comment)
    }
}

__bss_size = (__bss_end - __bss_start) >> 3;
