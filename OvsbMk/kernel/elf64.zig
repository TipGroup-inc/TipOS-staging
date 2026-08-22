// moe moe kyun <3
// moe moe kyun <3
// ELF64 loader for TipOS — Linux ELF binary support (GPL v2)

// ~~ Elf64_Ehdr ~~
// Cabeçalho ELF64 — 64 bytes mágicos que identificam e descrevem o binário.
// e_ident[0..3] = "\x7fELF", e_entry = ponto de entrada, e_phoff = offset
// da tabela de program headers (é o que importa pra gente~)
const Elf64_Ehdr = extern struct {
    e_ident: [16]u8,
    e_type: u16,
    e_machine: u16,
    e_version: u32,
    e_entry: u64,
    e_phoff: u64,
    e_shoff: u64,
    e_flags: u32,
    e_ehsize: u16,
    e_phentsize: u16,
    e_phnum: u16,
    e_shentsize: u16,
    e_shnum: u16,
    e_shstrndx: u16,
};

// ~~ Elf64_Phdr ~~
// Program Header ELF64 — descreve um segmento a ser carregado na memória.
// p_type=PT_LOAD é o que nos interessa: manda carregar da foto pro mapa~
// p_vaddr é onde colocar, p_filesz é o tamanho no arquivo, p_memsz é o
// tamanho na memória (se memsz > filesz, preenche com zero~)
const Elf64_Phdr = extern struct {
    p_type: u32,
    p_flags: u32,
    p_offset: u64,
    p_vaddr: u64,
    p_paddr: u64,
    p_filesz: u64,
    p_memsz: u64,
    p_align: u64,
};

// ~~ Constantes ELF ~~ amor à primeira vista~
// PT_LOAD = 1: segmento carregável (o que interessa pra gente)
// PT_DYNAMIC = 2: tabela dinâmica (onde ficam as relocações)
// EM_X86_64 = 62: arquitetura x86-64 (senão não roda, baka~)
// ET_EXEC = 2: executável normal
// ET_DYN = 3: biblioteca dinâmica (PIE, dá pra carregar também~)
const PT_LOAD: u32 = 1;
const PT_DYNAMIC: u32 = 2;
const EM_X86_64: u16 = 62;
const ET_EXEC: u16 = 2;
const ET_DYN: u16 = 3;

// ~~ Relocações x86-64 ~~
// DT_RELA = 7: endereço da tabela de relocações
// DT_RELASZ = 8: tamanho da tabela em bytes
// DT_RELAENT = 9: tamanho de cada entrada (24 bytes)
// R_X86_64_RELATIVE = 8: soma a base de carga no endereço alvo~
const DT_RELA: u64 = 7;
const DT_RELASZ: u64 = 8;
const DT_RELAENT: u64 = 9;
const R_X86_64_RELATIVE: u32 = 8;

// ~~ Elf64_Dyn ~~
// Entrada da tabela dinâmica: par (tag, valor)~
const Elf64_Dyn = extern struct {
    d_tag: u64,
    d_val: u64,
};

// ~~ Elf64_Rela ~~
// Entrada de relocação (formato RELA, com addend)~
const Elf64_Rela = extern struct {
    r_offset: u64,
    r_info: u64,
    r_addend: i64,
};

// ~~ PIE_BASE ~~
// Base de carga de binários PIE (ET_DYN): 1GB, acima de qualquer
// mapeamento do kernel. O kernel mora em VA baixa (código 0x100000,
// buddy 0xA00000 até o fim da RAM) — um PIE com segmento em va=0x0
// sombrearia o código do próprio kernel no pml4 do usuário, e o
// context switch executaria bytes do programa no lugar do kernel
// (triple fault). O Xorg é PIE e sofreu disso~ (7655 relocações!)
const PIE_BASE: u64 = 0x4000_0000;

export fn elf64_pie_base() u64 {
    return PIE_BASE;
}

// ~~ Externs ~~ funções do kernel que a gente chama na amizade~
// mmap_user: aloca páginas no espaço do usuário (quem nunca?)
// map_2mb_in_pml4: mapeia 2MB na tabela de páginas (huge pages FTW~)
// serial_puts/puthex: debug pela serial (amo ver números~)
extern fn mmap_user(addr: ?*anyopaque, length: usize, prot: i32, flags: i32) ?*anyopaque;
extern fn map_2mb_in_pml4(pml4_pa: u64, virt: u64, phys: u64) i32;
extern fn serial_puts(str: [*:0]const u8) void;
extern fn serial_puthex(val: u32) void;

// ~~ map_2mb_ensure ~~
// Mapeia 2MB, alocando as tabelas intermediárias (PD) se faltarem.
// O clone_identity_tables só cria as tabelas pro range da RAM (0-512MB);
// o PIE em 0x40000000 cai no pdpt_idx=1 (1GB+), que não existe — a gente
// aloca um PD novo e encaixa lá~ (quem manda é a PD entry: 0x87 = huge)
fn map_2mb_ensure(pml4_pa: u64, virt: u64, phys: u64) i32 {
    const pml4_idx: usize = @intCast((virt >> 39) & 0x1FF);
    const pdpt_idx: usize = @intCast((virt >> 30) & 0x1FF);
    const pd_idx: usize = @intCast((virt >> 21) & 0x1FF);

    const pml4 = @as([*]u64, @ptrFromInt(@as(usize, @intCast(pml4_pa))));
    if ((pml4[pml4_idx] & 1) == 0) return -1;

    const pdpt = @as([*]u64, @ptrFromInt(@as(usize, @intCast(pml4[pml4_idx] & ~@as(u64, 0xFFF)))));
    if ((pdpt[pdpt_idx] & 1) == 0) {
        const raw_pd = mmap_user(null, 4096, 3, 0) orelse return -1;
        const pd = @as([*]u64, @ptrCast(@alignCast(raw_pd)));
        var z: usize = 0;
        while (z < 512) : (z += 1) pd[z] = 0;
        pdpt[pdpt_idx] = @intFromPtr(raw_pd) | 0x7; // P|RW|US (user acessa!)
    }
    const pd = @as([*]u64, @ptrFromInt(@as(usize, @intCast(pdpt[pdpt_idx] & ~@as(u64, 0xFFF)))));
    pd[pd_idx] = (phys & ~@as(u64, 0x1FFFFF)) | 0x87; // P|RW|US|PS
    return 0;
}

// ~~ is_elf64 ~~
// Verifica os 4 bytes mágicos: 0x7F, 'E', 'L', 'F'.
// Se não for, não adianta forçar — não é ELF, pode parar~
fn is_elf64(data: [*]const u8) bool {
    return data[0] == 0x7F and data[1] == 'E' and data[2] == 'L' and data[3] == 'F';
}

// ~~ elf_hdr ~~
// Cast do ponteiro de bytes pra Elf64_Ehdr (cast sujo mas necessário~)
fn elf_hdr(data: [*]const u8) *const Elf64_Ehdr {
    return @ptrCast(@alignCast(data));
}

// ~~ elf_phdr ~~
// Cast do ponteiro de bytes num offset específico pra Elf64_Phdr.
// e_phoff dá onde começa a tabela, aí a gente itera com e_phentsize~ ☆
fn elf_phdr(data: [*]const u8, offset: u64) *const Elf64_Phdr {
    return @ptrCast(@alignCast(data + offset));
}

// ~~ elf64_load ~~
// Carrega um ELF64 em modo identity mapping (mapeamento 1:1).
// Varre os program headers, copia segmentos PT_LOAD pras vaddrs certas,
// preenche BSS com zero, e retorna o entry point.
// Se algo der errado, retorna null (e você fica sem programa~)
export fn elf64_load(data: [*]const u8, len: u32) ?*anyopaque {
    if (len < @sizeOf(Elf64_Ehdr)) return null;
    if (!is_elf64(data)) return null;

    const ehdr = elf_hdr(data);
    if (ehdr.e_machine != EM_X86_64) return null;
    if (ehdr.e_type != ET_EXEC and ehdr.e_type != ET_DYN) return null;
    if (ehdr.e_phentsize != @sizeOf(Elf64_Phdr)) return null;
    if (ehdr.e_phnum == 0) return null;

    serial_puts("elf64: load identity\n");

    var max_va: u64 = 0;
    var i: u16 = 0;
    while (i < ehdr.e_phnum) : (i += 1) {
        const phdr_off = ehdr.e_phoff + i * ehdr.e_phentsize;
        if (phdr_off + @sizeOf(Elf64_Phdr) > len) return null;
        const phdr = elf_phdr(data, phdr_off);
        if (phdr.p_type != PT_LOAD) continue;

        const dst_va = phdr.p_vaddr;
        const dst = @as([*]u8, @ptrFromInt(dst_va));
        const src = data + phdr.p_offset;

        serial_puts("  LOAD va=");
        serial_puthex(@as(u32, @truncate(dst_va)));
        serial_puts(" sz=");
        serial_puthex(@as(u32, @truncate(phdr.p_filesz)));
        serial_puts("\n");

        var j: u64 = 0;
        while (j < phdr.p_filesz) : (j += 1) dst[j] = src[j];
        while (j < phdr.p_memsz) : (j += 1) dst[j] = 0;

        const extra_end = (dst_va + phdr.p_memsz + 0xFFF) & ~@as(u64, 0xFFF);
        var k: u64 = dst_va + phdr.p_memsz;
        while (k < extra_end) : (k += 1) @as([*]u8, @ptrFromInt(k))[0] = 0;

        if (dst_va + phdr.p_memsz > max_va)
            max_va = dst_va + phdr.p_memsz;
    }

    serial_puts("elf64: entry=");
    serial_puthex(@as(u32, @truncate(ehdr.e_entry)));
    serial_puts("\n");
    return @as(*anyopaque, @ptrFromInt(ehdr.e_entry));
}

// ~~ elf64_load_into_pml4 ~~
// Carrega um ELF64 usando uma PML4 específica (tabela de páginas separada).
// Ideal pra processos — cada um tem seu próprio espaço de endereçamento~
// Aloca páginas de 2MB (huge pages) via mmap_user e mapeia na PML4.
// Copia os dados do ELF e zera o resto. Retorna o entry point.
export fn elf64_load_into_pml4(data: [*]const u8, len: u32, pml4: u64) ?*anyopaque {
    if (len < @sizeOf(Elf64_Ehdr)) return null;
    if (!is_elf64(data)) return null;

    const ehdr = elf_hdr(data);
    if (ehdr.e_machine != EM_X86_64) return null;
    if (ehdr.e_type != ET_EXEC and ehdr.e_type != ET_DYN) return null;
    if (ehdr.e_phentsize != @sizeOf(Elf64_Phdr)) return null;
    if (ehdr.e_phnum == 0) return null;

    // ~~ PIE: carrega em base alta pra não sombrear o kernel ~~
    const is_pie = ehdr.e_type == ET_DYN;
    const base: u64 = if (is_pie) PIE_BASE else 0;
    if (is_pie) {
        serial_puts("elf64: PIE base=");
        serial_puthex(@as(u32, @truncate(base)));
        serial_puts("\n");
    }

    serial_puts("elf64: load into pml4\n");

    // Track already-mapped 2MB chunks so overlapping PT_LOAD segments
    // don't allocate+zero each other's data. Each entry = (chunk_idx << 32) | phys.
    // chunk_idx = va >> 21 (índice do chunk de 2MB). Antes era va >> 32,
    // que colidia entre segmentos no mesmo bloco de 4GB (ex: XORG)~
    var mapped: [32]u64 = [_]u64{0} ** 32;
    var mapped_count: u32 = 0;

    var i: u16 = 0;
    while (i < ehdr.e_phnum) : (i += 1) {
        const phdr_off = ehdr.e_phoff + i * ehdr.e_phentsize;
        if (phdr_off + @sizeOf(Elf64_Phdr) > len) return null;
        const phdr = elf_phdr(data, phdr_off);
        if (phdr.p_type != PT_LOAD) continue;

        const dst_va = base + phdr.p_vaddr;
        const seg_end = dst_va + phdr.p_memsz;
        const aligned_start = dst_va & ~@as(u64, 0x1FFFFF);
        const aligned_end = (seg_end + 0x1FFFFF) & ~@as(u64, 0x1FFFFF);

        serial_puts("  LOAD va=");
        serial_puthex(@as(u32, @truncate(dst_va)));
        serial_puts("..");
        serial_puthex(@as(u32, @truncate(seg_end)));
        serial_puts("\n");

        var va: u64 = aligned_start;
        while (va < aligned_end) : (va += 0x200000) {
            // ~~ Ja mapeou esse chunk de 2MB antes? ~~
            // mapped[] guarda: upper 32 bits = chunk_idx (va >> 21),
            // lower 32 bits = phys (endereco fisico).
            // Se o VA da proxima carga cai no mesmo chunk,
            // reusa o mesmo bloco fisico~ (sem OR corrupto!)
            const tag = va >> 21;
            var phys: ?[*]u8 = null;
            var mi: u32 = 0;
            while (mi < mapped_count) : (mi += 1) {
                if ((mapped[mi] >> 32) == tag) {
                    phys = @ptrFromInt(@as(usize, @intCast(mapped[mi] & 0xFFFF_FFFF)));
                    break;
                }
            }

            // ~~ Resolve o phys: reusa chunk já mapeado ou aloca novo ~~
            var p: [*]u8 = undefined;
            if (phys) |existing| {
                p = existing;
            } else {
                const raw = mmap_user(null, 0x200000, 3, 0) orelse {
                    serial_puts("elf64: alloc fail\n");
                    return null;
                };
                if (map_2mb_ensure(pml4, va, @intFromPtr(raw)) < 0) {
                    serial_puts("elf64: map fail\n");
                    return null;
                }

                p = @as([*]u8, @ptrCast(raw));
                var z: u64 = 0;
                while (z < 0x200000) : (z += 1) p[z] = 0;

                // ~~ Salva no mapped[]: (chunk_idx << 32) | PA_lo ~~
                // Nada de OR bit a bit! O upper 32 bits é do chunk,
                // o lower 32 bits é do PA. Separadinhos~
                // (igual casal que dorme em cama de casal mas cada
                //  um com seu cobertor~ cada um no seu quadrado!)
                mapped[mapped_count] = (tag << 32) | (@as(u64, @intFromPtr(raw)) & 0xFFFF_FFFF);
                mapped_count += 1;
            }

            // ~~ Copia o trecho do arquivo que cai NESTE chunk ~~
            // Segmentos maiores que 2MB (ex: XORG, 3.2MB) estouravam o
            // primeiro chunk e o resto ficava zerado — cada chunk de 2MB
            // recebe só a fatia que lhe pertence~
            // (tipo rachar o miojo: cada um leva a própria metade)
            const dst_off: u64 = if (dst_va > va) (dst_va - va) else 0;
            const chunk_off: u64 = if (dst_va > va) 0 else (va - dst_va);
            if (chunk_off < phdr.p_filesz) {
                const room = 0x200000 - dst_off;
                const n = @min(phdr.p_filesz - chunk_off, room);
                const src = data + phdr.p_offset + chunk_off;
                var j: u64 = 0;
                while (j < n) : (j += 1) p[dst_off + j] = src[j];
            }
        }
    }

    // ~~ Relocações R_X86_64_RELATIVE ~~
    // NÃO aplicamos aqui: o musl static-pie (1.2.4+) se auto-reloca
    // no _start_c usando o AT_BASE do auxv (que a gente empilha no
    // setup_linux_user_stack). Se aplicássemos, seria double-reloc~
    if (is_pie) {
        serial_puts("elf64: relocs by user (_start_c) via AT_BASE\n");
    }

    const entry = if (is_pie) base + ehdr.e_entry else ehdr.e_entry;
    serial_puts("elf64: entry=");
    serial_puthex(@as(u32, @truncate(entry)));
    serial_puts("\n");
    return @as(*anyopaque, @ptrFromInt(entry));
}