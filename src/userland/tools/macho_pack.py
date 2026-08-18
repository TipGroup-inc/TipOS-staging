# moe moe kyun <3
# moe moe kyun <3
# ♥ userland ~ programinha de ring 3, longe do kernel!
# arquivo: macho_pack.py ~ funcoes anotadas: 1
# ~*~ macho_pack.py ~*~
# Hihi, Pythonzinho ~ que fofo!
# Se rodar sem erro, pode comemorar~ >_<
# ~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~

# ~~ macho_pack.py ~~
# Empacota flat binary em um executável Mach-O 64-bit minimal.
# O kernel TipOS carrega Mach-O, então precisamos converter nossos
# flat binaries (extraídos via objcopy) pro formato que ele entende~
#
# Layout do Mach-O gerado:
#   [ Mach-O Header (32 bytes) ]
#   [ __TEXT Segment Command (72 bytes) ]
#   [ LC_MAIN Command (24 bytes) ]
#   [ Código bruto ]
#
# O __TEXT segment mapeia o código em 0x10000000 (mesmo base do link.ld~)
# LC_MAIN define o entry offset dentro do código (entry = 0x10000000 + offset)

import struct
import sys

# ~~ Constantes Mach-O ~~
# MH_MAGIC_64 = 0xFEEDFACF (little-endian, sim, ao contrário do que parece~)
# CPU_TYPE_X86_64 = 0x01000007 (arch x86_64, claro~)
# CPU_SUBTYPE_X86_64 = 0x80000003 (subtype com capabilitiy bit)
# MH_EXECUTE = 2 (tipo: executável padrão)
# LC_SEGMENT_64 = 0x19 (load command de segmento)
# LC_MAIN = 0x80000028 (load command que define entry point)
MH_MAGIC_64 = 0xFEEDFACF
CPU_TYPE_X86_64 = 0x01000007
CPU_SUBTYPE_X86_64 = 0x80000003
MH_EXECUTE = 2
LC_SEGMENT_64 = 0x19
LC_MAIN = 0x80000028

# ~~ pack ~~
# ~ cuidado que essa aqui morde ~
def pack(code, entry_offset=0):
    """Wrap flat binary code in minimal Mach-O executable.

    Layout: [header(32) + __TEXT_seg(72) + LC_MAIN(24) + code]
    - __TEXT: fileoff=code_off, vmaddr=0x10000000, filesize=len(code)
      -> kernel loads to 0x10000000 via mach_o_load
    - LC_MAIN entryoff = offset within code to _start
      -> entry = 0x10000000 + entryoff
    """
    segname = b'__TEXT\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'

    total_cmds_size = 72 + 24
    code_off = 32 + total_cmds_size

    header = struct.pack('<IIIIIIII',
        MH_MAGIC_64,
        CPU_TYPE_X86_64,
        CPU_SUBTYPE_X86_64,
        MH_EXECUTE,
        2,                    # ncmds
        total_cmds_size,      # sizeofcmds
        0x200085,             # flags
        0)                    # reserved

    seg = struct.pack('<II', LC_SEGMENT_64, 72)
    seg += struct.pack('<16s', segname)
    seg += struct.pack('<QQ', 0x10000000, len(code))   # vmaddr, vmsize
    seg += struct.pack('<QQ', code_off, len(code))     # fileoff, filesize
    seg += struct.pack('<II', 7, 5)                    # maxprot, initprot
    seg += struct.pack('<II', 0, 0)                    # nsects, flags

    main_cmd = struct.pack('<II', LC_MAIN, 24)
    main_cmd += struct.pack('<QQ', entry_offset, 0)

    data = header + seg + main_cmd + code
    return data, len(data)

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(f'uso: {sys.argv[0]} entrada.bin saida.macho [entry_offset]')
        sys.exit(1)
    entry_offset = int(sys.argv[3]) if len(sys.argv) > 3 else 0
    with open(sys.argv[1], 'rb') as f:
        code = f.read()
    macho, size = pack(code, entry_offset)
    with open(sys.argv[2], 'wb') as f:
        f.write(macho)
    print(f'ok: {size} bytes')


# ♥ macho_pack.py ~ arquivo fofinho do OvsbMkM! kyun~ <3
