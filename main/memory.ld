OUTPUT_ARCH("aarch64")
ENTRY(_start)

SECTIONS {
    . = 0x40000000;

    .text.boot : {
      KEEP(*(.text.boot))
    }

    . = ALIGN(4096);

    /DISCARD/ : {
      *(.text.subnode)
    }
    .text : {
      *(.text) *(.text.*)
    }

    .rodata : {
      *(.rodata) 

      . = ALIGN(16);
      __pocv2_msg_data_start = .;
      *(.rodata.pocv2_msg)
      __pocv2_msg_data_end = .;

      *(.rodata.*)
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

    . = ALIGN(4096);

    PROVIDE(vmm_end = .);

    /DISCARD/ : {
      *(.comment)
    }
}

__bss_size = (__bss_end - __bss_start) >> 3;
