OUTPUT_ARCH("aarch64")
ENTRY(_start)

SECTIONS {
    . = 0x80000;
    PROVIDE(vmm_start = .);

    __text_start = .;

    .text.head : {
      KEEP(*(.text.head))
    }

    .text.boot : {
      KEEP(*(.text.boot))
    }

    /*
    /DISCARD/ : {
      *(.text.node0)
    }
    */

    .text : {
      *(.text) *(.text.*)
    }
    __text_end = .;

    . = ALIGN(4096);
    .rodata : {
      __rodata_start = .;

      *(.rodata) 

      . = ALIGN(16);
      __msg_size_data_start = .;
      *(.rodata.msg)
      __msg_size_data_end = .;
      . = ALIGN(16);
      __msg_handler_data_start = .;
      *(.rodata.msg.common)
      *(.rodata.msg.subnode)
      __msg_handler_data_end = .;

      __rodata_end = .;
    }

    . = ALIGN(4096);
    .data.device : {
      __dt_irqchip_device = .;
      KEEP(*(__dt_irqchip_device));
      __dt_serial_device = .;
      KEEP(*(__dt_serial_device));
    }

    . = ALIGN(16);
    .data : {
      *(.data) *(.data.*)
    }

    . = ALIGN(4096);
    .boot_pagetable : {
      __boot_pgt_l1 = .;
      . += 4096;
      __boot_pgt_l2_1 = .;
      . += 4096;
      __boot_pgt_l2_2 = .;
      . += 4096;
      __boot_pgt_l3 = .;
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

    . = ALIGN(0x200000);
    /*
     *  reserve 2MB for vmm initialization
     */
    .boot_earlymem : {
      __earlymem_start = .;
      . += 0x200000;
      __earlymem_end = .;
    }

    /DISCARD/ : {
      *(.comment)
    }
}

__bss_size = (__bss_end - __bss_start) >> 3;
