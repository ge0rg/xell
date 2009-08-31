     ENTRY(_start)
     SECTIONS
     {
        . = 0x8000000001310000;
       .text : { *(.text) }
       .data : { *(.data) }
       .sdata : { *(.sdata) }
       .rodata : { *(.rodata)}

        . = 0x8000000001380000;

       bss_start = .;
       .bss : { *(.bss) }
       .sbss : { *(.sbss) }
       bss_end = .;
     }
