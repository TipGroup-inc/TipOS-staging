#ifndef MACH_O_H
#define MACH_O_H

#include <stdint.h>

// Magic numbers
#define MH_MAGIC    0xFEEDFACE  // 32-bit
#define MH_MAGIC_64 0xFEEDFACF  // 64-bit
#define MH_CIGAM    0xCEFAEDFE  // 32-bit (invertido)
#define MH_CIGAM_64 0xCFFAEDFE  // 64-bit (invertido)
#define FAT_MAGIC   0xCAFEBABE  // FAT binary
#define FAT_CIGAM   0xBEBAFECA  // FAT binary inverted

// Header 64-bit
typedef struct {
    uint32_t magic;         // Magic number
    uint32_t cputype;       // CPU type (0x01000007 = x86-64)
    uint32_t cpusubtype;    // CPU subtype (3 = x86-64)
    uint32_t filetype;      // Tipo de arquivo (2 = executável)
    uint32_t ncmds;         // Número de comandos de carga
    uint32_t sizeofcmds;    // Tamanho total dos comandos
    uint32_t flags;         // Flags
    uint32_t reserved;      // Reservado (64-bit)
} __attribute__((packed)) mach_header_64_t;

// Comando de carga
typedef struct {
    uint32_t cmd;           // Tipo do comando
    uint32_t cmdsize;       // Tamanho do comando
} __attribute__((packed)) load_command_t;

// LC_SEGMENT_64
#define LC_SEGMENT_64 0x19
#define LC_MAIN       0x80000028

typedef struct {
    uint32_t cmd;
    uint32_t cmdsize;
    char     segname[16];   // Nome do segmento
    uint64_t vmaddr;        // Endereço virtual
    uint64_t vmsize;        // Tamanho virtual
    uint64_t fileoff;        // Offset no arquivo
    uint64_t filesize;        // Tamanho no arquivo
    uint32_t maxprot;        // Proteção máxima
    uint32_t initprot;        // Proteção inicial
    uint32_t nsects;        // Número de seções
    uint32_t flags;         // Flags
} __attribute__((packed)) segment_command_64_t;

typedef struct {
    uint32_t cmd;
    uint32_t cmdsize;
    uint64_t entryoff;
    uint64_t stacksize;
} __attribute__((packed)) entry_point_command_t;

// Section 64
typedef struct {
    char     sectname[16];  // Nome da seção
    char     segname[16];   // Nome do segmento
    uint64_t addr;          // Endereço
    uint64_t size;          // Tamanho
    uint32_t offset;        // Offset no arquivo
    uint32_t align;         // Alinhamento
    uint32_t reloff;        // Offset de relocações
    uint32_t nreloc;        // Número de relocações
    uint32_t flags;         // Flags
    uint32_t reserved1;
    uint32_t reserved2;
    uint32_t reserved3;
} __attribute__((packed)) section_64_t;

// LC_UNIXTHREAD (ponto de entrada)
#define LC_UNIXTHREAD 0x05

typedef struct {
    uint32_t cmd;
    uint32_t cmdsize;
    uint32_t flavor;        // Tipo de estado (x86_64 = 4)
    uint32_t count;         // Número de registradores
    // Seguido por: struct x86_thread_state64_t
} __attribute__((packed)) unixthread_command_t;

void *mach_o_load(void *data, unsigned int len);

#endif
