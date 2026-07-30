/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ userland ~ programinha de ring 3, longe do kernel!
 * arquivo: link.ld ~ funcoes anotadas: 0
 */
SECTIONS {
    . = 0x10000000;

    .text : {
        *(.text.start)
        *(.text*)
        *(.rodata*)
        *(.data*)
        *(.got)
        *(.got.plt)
        *(.note.gnu.property)
    }

    _bss_start = .;
    .bss (NOLOAD) : { *(.bss*) }
    _bss_end = .;
}

/* ♥ link.ld ~ arquivo fofinho do OvsbMkM! kyun~ <3 */
