OUTPUT_ARCH("aarch64")
ENTRY(_start)

SECTIONS {
    . = 0x40000000;
    PROVIDE(vmm_start = .);

    .text.boot : {
      KEEP(*(.text.boot))
    }

    . = ALIGN(16);

    /*
    /DISCARD/ : {
      *(.text.subnode)
    }
    */
    .text : {
      *(.text) *(.text.*)
    }

    .rodata : {
      *(.rodata) 

      . = ALIGN(16);
      __pocv2_msg_size_data_start = .;
      *(.rodata.pocv2_msg)
      __pocv2_msg_size_data_end = .;
      . = ALIGN(16);
      __pocv2_msg_handler_data_start = .;
      *(.rodata.pocv2_msg.common)
      *(.rodata.pocv2_msg.node0)
      __pocv2_msg_handler_data_end = .;
    }

    .data : {
      *(.data) *(.data.*)
    }

    . = ALIGN(4096);
    .boot_pagetable : {
      __boot_pgt_l1 = .;
      . += 4096;
      __boot_pgt_l2 = .;
      . += 4096;
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
