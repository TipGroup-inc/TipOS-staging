// Mini stub array to act as a 'Mach-O' test blob.
// Real Mach-O generation is complex; this blob is just a placeholder
// so mach_o_load can parse FAT/MH headers if provided.

unsigned char test_macho[] = {
    0xcf,0xfa,0xed,0xfe, // MH_MAGIC_64 (little endian)
    0x07,0x00,0x00,0x01, // cputype (placeholder)
    0,0,0,0, // cpusubtype
    0,0,0,0, // filetype
    0,0,0,0, // ncmds
    0,0,0,0, // sizeofcmds
    0,0,0,0, // flags
    0,0,0,0  // reserved
};
unsigned int test_macho_len = sizeof(test_macho);
