OUTPUT_ARCH("aarch64")
ENTRY(_start)

SECTIONS {
    . = 0x80000000;
    PROVIDE(vmm_start = .);

    __text_start = .;
    __idmap_start = .;

    .text.head : {
      KEEP(*(.text.head))
    }

    .text.boot : {
      KEEP(*(.text.boot))
    }

    . = ALIGN(4096);
    __idmap_end = .;

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
    __rodata_start = .;

    .rodata.device : {
      __dt_irqchip_device = .;
      KEEP(*(__dt_irqchip_device));
      KEEP(*(__dt_irqchip_sentinel));
      __dt_serial_device = .;
      KEEP(*(__dt_serial_device));
      KEEP(*(__dt_serial_sentinel));
    }

    .rodata : {
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

      *(.rodata.*)
    }

    __rodata_end = .;

    . = ALIGN(16);
    .data : {
      *(.data) *(.data.*)
    }

    . = ALIGN(4096);
    .boot_pagetable : {
      __boot_pgt_l1 = .;
      . += 4096;
      __idmap_pgt_l2_1 = .;
      . += 4096;
      __idmap_pgt_l2_2 = .;
      . += 4096;
      __idmap_pgt_l3_1 = .;
      . += 4096;
      __idmap_pgt_l3_2 = .;
      . += 4096;
      __boot_pgt_l2 = .;
      . += 4096;
      __boot_pgt_l3 = .;
      . += 4096 * (((vmm_end - vmm_start + 0x200000-1) & ~(0x200000-1)) >> 21);
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

ASSERT(vmm_start % 0x1000 == 0, "vmm misaligned")
ASSERT(__idmap_end - __idmap_start == 4096, "idmap too big")
