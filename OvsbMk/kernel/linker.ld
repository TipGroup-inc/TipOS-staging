/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
 * arquivo: linker.ld ~ funcoes anotadas: 0
 */
/* ♥ Linker script ~ "Junte tudo, seu montador baka!" */
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

/* ♥ STACK ~ pilha do kernel 16KB */

/* ♥ linker.ld ~ arquivo fofinho do OvsbMk! kyun~ <3 */
