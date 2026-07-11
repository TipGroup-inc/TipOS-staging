ENTRY(_start)

SECTIONS
{
    . = 1M;
    
    .multiboot : {
        KEEP(*(.multiboot))
    }
    
    .text : {
        *(.text)
    }
    
    .rodata : {
        *(.rodata)
    }
    
    .data : {
        *(.data)
    }
    
    .paging : {
        *(.paging)
    }
    
    .bss : {
        *(.bss)
        *(COMMON)
    }
}
