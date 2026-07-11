SECTIONS {
    . = 0x2000000;

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
