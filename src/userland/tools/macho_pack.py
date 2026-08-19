# moe moe kyun <3
# Empacota flat binary em um executável Mach-O 64-bit minimal.

import struct
import sys

MH_MAGIC_64 = 0xFEEDFACF
CPU_TYPE_X86_64 = 0x01000007
CPU_SUBTYPE_X86_64 = 0x80000003
MH_EXECUTE = 2
LC_SEGMENT_64 = 0x19
LC_MAIN = 0x80000028
BSS_SIZE = 0x80000  # 512KB para heap + variaveis globais

def pack(code, entry_offset=0):
    segname = b'__TEXT\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'

    total_cmds_size = 72 + 24
    code_off = 32 + total_cmds_size

    header = struct.pack('<IIIIIIII',
        MH_MAGIC_64,
        CPU_TYPE_X86_64,
        CPU_SUBTYPE_X86_64,
        MH_EXECUTE,
        2,
        total_cmds_size,
        0x200085,
        0)

    total_size = len(code) + BSS_SIZE

    seg = struct.pack('<II', LC_SEGMENT_64, 72)
    seg += struct.pack('<16s', segname)
    seg += struct.pack('<QQ', 0x10000000, total_size)
    seg += struct.pack('<QQ', code_off, total_size)
    seg += struct.pack('<II', 7, 5)
    seg += struct.pack('<II', 0, 0)

    main_cmd = struct.pack('<II', LC_MAIN, 24)
    main_cmd += struct.pack('<QQ', entry_offset, 0)

    data = header + seg + main_cmd + code + b'\x00' * BSS_SIZE
    return data, len(data)

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("uso: macho_pack.py <input.bin> <output.macho>")
        sys.exit(1)
    with open(sys.argv[1], 'rb') as f:
        code = f.read()
    macho, size = pack(code)
    with open(sys.argv[2], 'wb') as f:
        f.write(macho)
    print(f"ok: {size} bytes")
