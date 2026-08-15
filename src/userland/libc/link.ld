/* userland linker script */
/* Base address = 128 MB (0x10000000) */
/* Onde o kernel mapeia o userland */

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
