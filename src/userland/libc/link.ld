/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ userland ~ programinha de ring 3, longe do kernel!
 * arquivo: link.ld ~ funcoes anotadas: 0
 */
/* ~~ Script de linker do userland ~~ */
/* Cada programa é linkado com base em 0x10000000 (endereço virtual fixo~) */
/* Porque o loader Mach-O carrega tudo nesse endereço. */

SECTIONS {
    /* ~~ Base address = 128 MB ~~ */
    /* 0x10000000 é o endereço onde o kernel mapeia o userland. */
    /* Se mudar isso, muda o loader, muda o packer, muda tudo~ (então não mexe~) */
    . = 0x10000000;

    /* ~~ Seção .text (código + dados constantes) ~~ */
    /* .text.start: contém _start (crt0) — tem que ser o primeiro! */
    /* .text: resto do código */
    /* .rodata: strings, constantes (read-only) */
    /* .data: dados inicializados */
    /* .got / .got.plt: tabelas de linkage (pro dynamic linker, se existisse~) */
    /* .note.gnu.property: notas do GNU (o GCC insiste em colocar~) */
    .text : {
        *(.text.start)
        *(.text*)
        *(.rodata*)
        *(.data*)
        *(.got)
        *(.got.plt)
        *(.note.gnu.property)
    }

    /* ~~ BSS ~~ */
    /* Dados não inicializados (zero fill ~ economiza espaço no binário~) */
    /* _bss_start e _bss_end são usados pelo crt0 pra zerar a seção */
    _bss_start = .;
    .bss (NOLOAD) : { *(.bss*) }
    _bss_end = .;
}

/* ♥ link.ld ~ arquivo fofinho do OvsbMkM! kyun~ <3 */
