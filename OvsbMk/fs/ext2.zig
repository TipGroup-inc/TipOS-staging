// moe moe kyun <3
// moe moe kyun <3
// Ext2 read-only driver for TipOS — Linux native fs (GPL v2)

// ~~ Externs ~~ chamando as funções do kernel ~ socorro!
// ata_read_sector: lê 512 bytes do disco (LBA addressing, pq sim)
// serial_puts/puthex: debug via serial (eu gosto de ver o que acontece)
extern fn ata_read_sector(lba: u32, buffer: [*]u8) i32;
extern fn serial_putc(c: u8) void;
extern fn ata_write_sector(lba: u32, buffer: [*]u8) i32;
extern fn serial_puts(str: [*:0]const u8) void;
extern fn serial_puthex(val: u32) void;

// ~~ Ext2Superblock ~~
// Superbloco ext2 — contém metadados vitais do filesystem.
// Tamanho fixo de 1024 bytes, começa no offset 1024 do disco.
// Se o magic não for 0xEF53, não é ext2 e você tá no lugar errado~ ☆
const Ext2Superblock = extern struct {
    s_inodes_count: u32,
    s_blocks_count: u32,
    s_r_blocks_count: u32,
    s_free_blocks_count: u32,
    s_free_inodes_count: u32,
    s_first_data_block: u32,
    s_log_block_size: u32,
    s_log_frag_size: u32,
    s_blocks_per_group: u32,
    s_frags_per_group: u32,
    s_inodes_per_group: u32,
    s_mtime: u32,
    s_wtime: u32,
    s_mnt_count: u16,
    s_max_mnt_count: u16,
    s_magic: u16,
    s_state: u16,
    s_errors: u16,
    s_minor_rev_level: u16,
    s_lastcheck: u32,
    s_checkinterval: u32,
    s_creator_os: u32,
    s_rev_level: u32,
    s_def_resuid: u16,
    s_def_resgid: u16,
};

// ~~ Ext2BlockGroupDesc ~~
// Descritor de grupo de blocos — cada grupo tem seu bitmap de blocos,
// bitmap de inodes, e tabela de inodes. Organizamos o disco em grupos
// pra não morrer de procurar inode no meio do nada~
const Ext2BlockGroupDesc = extern struct {
    bg_block_bitmap: u32,
    bg_inode_bitmap: u32,
    bg_inode_table: u32,
    bg_free_blocks_count: u16,
    bg_free_inodes_count: u16,
    bg_used_dirs_count: u16,
    bg_pad: u16,
    bg_reserved: [12]u8,
};

// ~~ Ext2Inode ~~
// Inode ext2 — o coração do filesystem.
// Guarda metadados de arquivos/diretórios: tamanho, timestamps,
// ponteiros pros blocos de dados (12 diretos + indireto + duplo + triplo).
// Nós só implementamos 12 diretos + 1 single-indirect pq sou preguiçosa~
const Ext2Inode = extern struct {
    i_mode: u16,
    i_uid: u16,
    i_size_low: u32,
    i_atime: u32,
    i_ctime: u32,
    i_mtime: u32,
    i_dtime: u32,
    i_gid: u16,
    i_links_count: u16,
    i_blocks: u32,
    i_flags: u32,
    i_osd1: u32,
    i_block: [15]u32,
    i_generation: u32,
    i_file_acl: u32,
    i_dir_acl: u32,
    i_faddr: u32,
    i_osd2: [12]u8,
};

// ~~ Ext2DirEntry ~~
// Entrada de diretório ext2 — um nome e um inode, simples assim~
// O nome vem logo depois do header (8 bytes) e tem `name_len` bytes.
// `rec_len` inclui padding, porque alinhamento é importante (disse ninguém)
const Ext2DirEntry = extern struct {
    inode: u32,
    rec_len: u16,
    name_len: u8,     // ~~ comprimento do nome (NÃO é u16! byte 7 é file_type~)
    file_type: u8,
    name: [0]u8,
};

// ~~ Constantes mágicas ~~
// EXT2_MAGIC = 0xEF53 — identifica o filesystem como ext2
// S_IFMT/S_IFDIR/S_IFREG — macros de tipo do mode do inode
// EXT2_ROOT_INO = 2 — o inode raiz é sempre o 2 (sabia não?~)
// EXT2_NDIR_BLOCKS = 12 — blocos diretos no inode
const EXT2_MAGIC: u16 = 0xEF53;
const EXT2_S_IFMT: u16 = 0xF000;
const EXT2_S_IFDIR: u16 = 0x4000;
const EXT2_S_IFREG: u16 = 0x8000;
const EXT2_ROOT_INO: u32 = 2;
const EXT2_NDIR_BLOCKS: usize = 12;

// ~~ Estado global do driver ~~
// sb: superbloco lido do disco
// block_size: tamanho do bloco (1024 << s_log_block_size)
// sectors_per_block: quantos setores de 512 cabem num bloco
// bgdt_block: bloco onde começa a tabela de descritores de grupo
// mounted: flag de "já montou, pode usar"
var sb: Ext2Superblock = undefined;
var block_size: u32 = 1024;
var sectors_per_block: u32 = 2;
var bgdt_block: u32 = 0;
var mounted: bool = false;

// ~~ Block cache ~~
// Cache de bloco único de 4KB (sim, sou básica~).
// Só cacheia um bloco por vez, se pedir outro, lê de novo.
// cached_dirty indica se precisa escrever de volta (só leitura, então nunca)
var block_buf: [4096]u8 = undefined;
var cached_block: u32 = 0xFFFFFFFF;
var cached_dirty: bool = false;

// ~~ sector_to_lba ~~
// Converte número de bloco + offset dentro do bloco pra LBA (setor 512B).
// Fica esperto: block != LBA, tem que multiplicar por sectors_per_block~
fn sector_to_lba(block: u32, offset_in_block: u32) u32 {
    return (block * sectors_per_block) + offset_in_block;
}

// ~~ read_block ~~
// Lê um bloco do disco pro cache (ou retorna o cache se já tiver).
// Se block_size > 4096, desiste (cache pequenininho~).
// Retorna ponteiro pro buffer ou null se o ATA falhar.
fn read_block(block: u32) ?[*]u8 {
    if (block == cached_block and !cached_dirty)
        return &block_buf;

    if (block_size > 4096)
        return null;

    const start_lba = sector_to_lba(block, 0);
    var si: u32 = 0;
    while (si < sectors_per_block) : (si += 1) {
        if (ata_read_sector(start_lba + si, @as([*]u8, &block_buf) + si * 512) < 0)
            return null;
    }

    cached_block = block;
    cached_dirty = false;
    return &block_buf;
}

// ~~ inode_to_bg / inode_to_idx ~~
// Inodes são numerados de 1 a N. Cada grupo tem s_inodes_per_group inodes.
// inode_to_bg: descobre qual grupo o inode pertence (divisão inteira~)
// inode_to_idx: descobre o índice DENTRO do grupo (resto da divisão)
// (inode - 1) porque inode 1 é o primeiro, mas array index é 0 >_<
fn inode_to_bg(inode: u32) u32 {
    return (inode - 1) / sb.s_inodes_per_group;
}

fn inode_to_idx(inode: u32) u32 {
    return (inode - 1) % sb.s_inodes_per_group;
}

// ~~ read_inode ~~
// Lê um inode do disco! Primeiro pega o descritor do grupo,
// depois localiza a tabela de inodes, e finalmente o inode específico.
// Retorna true se conseguiu, false se o disco falhou (ou você fez algo errado~)
fn read_inode(inum: u32, inode: *Ext2Inode) bool {
    const bg = inode_to_bg(inum);
    const idx = inode_to_idx(inum);

    // Read block group descriptor
    const bg_desc_size: u32 = 32;
    const descs_per_block = block_size / bg_desc_size;
    const bg_desc_block = bgdt_block + bg / descs_per_block;
    const bg_desc_off = (bg % descs_per_block) * bg_desc_size;

    const bg_data = read_block(bg_desc_block) orelse return false;
    const bg_desc = @as(*const Ext2BlockGroupDesc, @ptrCast(@alignCast(bg_data + bg_desc_off)));

    const inode_table = bg_desc.bg_inode_table;
    const inode_size: u32 = 128;
    const inodes_per_block = block_size / inode_size;
    const inode_block = inode_table + idx / inodes_per_block;
    const inode_off = (idx % inodes_per_block) * inode_size;

    const inode_data = read_block(inode_block) orelse return false;
    const inode_ptr = @as(*const Ext2Inode, @ptrCast(@alignCast(inode_data + inode_off)));
    inode.* = inode_ptr.*;
    return true;
}

// ~~ read_block_data ~~
// Lê o bloco de dados `block_idx` do inode (0-indexado).
// Suporta 12 blocos diretos + single-indirect (não me pede double indirect
// pq eu vou fingir que não ouvi~)
// Copia o bloco pro buffer `buf` (block_size bytes).
// ~~ phys_of ~ bloco físico do bloco lógico (direto+simples+duplo) ~~
// Buraco esparso (ponteiros 0) devolve is_hole=true — bloco lógico é ZEROS~
const PER_BLOCK: u32 = 256; // 1024/4 ponteiros por bloco indireto~

fn phys_of(inode: *const Ext2Inode, block_idx: u32, is_hole: *bool) ?u32 {
    is_hole.* = false;

    if (block_idx < EXT2_NDIR_BLOCKS) {
        const p = inode.i_block[block_idx];
        if (p == 0) is_hole.* = true;
        return p;
    }
    var rem = block_idx - EXT2_NDIR_BLOCKS;

    if (rem < PER_BLOCK) { // single indirect (i_block[12])
        const ib = inode.i_block[12];
        if (ib == 0) { is_hole.* = true; return 0; }
        const data = read_block(ib) orelse return null;
        const p = (@as([*]const u32, @ptrCast(@alignCast(data))))[rem];
        if (p == 0) is_hole.* = true;
        return p;
    }
    rem -= PER_BLOCK;

    if (rem < PER_BLOCK * PER_BLOCK) { // double indirect (i_block[13])
        const ib = inode.i_block[13];
        if (ib == 0) { is_hole.* = true; return 0; }
        const l1data = read_block(ib) orelse return null;
        const l1 = (@as([*]const u32, @ptrCast(@alignCast(l1data))))[rem / PER_BLOCK];
        if (l1 == 0) { is_hole.* = true; return 0; }
        const l2data = read_block(l1) orelse return null;
        const p = (@as([*]const u32, @ptrCast(@alignCast(l2data))))[rem % PER_BLOCK];
        if (p == 0) is_hole.* = true;
        return p;
    }
    return null; // triple indirect — depois a gente chora~
}

fn read_block_data(inode: *const Ext2Inode, block_idx: u32, buf: [*]u8) bool {
    var hole = false;
    const phys_block = phys_of(inode, block_idx, &hole) orelse {
        serial_puts("x2rd: falha idx=");
        serial_puthex(block_idx);
        serial_puts("\n");
        return false;
    };

    if (hole or phys_block == 0) {
        // ~~ buraco esparso = bloco lógico todo ZERO~~
        var zi: usize = 0;
        while (zi < block_size) : (zi += 1) buf[zi] = 0;
        return true;
    }

    const block_data = read_block(phys_block) orelse {
        serial_puts("x2rd: ata fail phys=");
        serial_puthex(phys_block);
        serial_puts("\n");
        return false;
    };
    var i: usize = 0;
    while (i < block_size) : (i += 1) buf[i] = block_data[i];
    return true;
}

// ~~ match_name_dir ~~
// Percorre as entradas de um diretório e procura por `name`.
// Retorna o inode se achar, 0 se não achar (ou se não for diretório~)
// Varre as entradas sequencialmente — tipo um `find` na marra~ ☆
fn match_name_dir(dir_inode: u32, name: [*:0]const u8) u32 {
    var inode: Ext2Inode = undefined;
    if (!read_inode(dir_inode, &inode)) {
        serial_puts("x2dbg: read_inode falhou ino=");
        serial_puthex(dir_inode);
        serial_puts("\n");
        return 0;
    }
    if ((inode.i_mode & EXT2_S_IFMT) != EXT2_S_IFDIR) {
        serial_puts("x2dbg: nao-dir ino=");
        serial_puthex(dir_inode);
        serial_puts(" mode=");
        serial_puthex(inode.i_mode);
        serial_puts("\n");
        return 0;
    }
    serial_puts("x2dbg: procurando '");
    serial_puts(name);
    serial_puts("' em ino=");
    serial_puthex(dir_inode);
    serial_puts(" sz=");
    serial_puthex(inode.i_size_low);
    serial_puts("\n");

    const size = inode.i_size_low;
    var offset: u32 = 0;
    var block_idx: u32 = 0;
    var block_data: [4096]u8 = undefined;

    while (offset < size) {
        if (offset % block_size == 0) {
            if (!read_block_data(&inode, block_idx, @as([*]u8, &block_data))) return 0;
            block_idx += 1;
        }

        const de = @as(*const Ext2DirEntry, @ptrCast(@alignCast(&block_data[@as(usize,@intCast(offset % block_size))])));
        if (de.inode == 0 or de.rec_len == 0) break;

        // Name follows the 8-byte header (inode + rec_len + name_len)
        const de_name = @as([*]const u8, @ptrCast(de)) + 8;

        if (de.name_len > 0) {
            serial_puts("x2dbg:   entrada '");
            { var k2: usize = 0;
              while (k2 < de.name_len) : (k2 += 1)
                  serial_putc(block_data[@as(usize,@intCast(offset % block_size)) + 8 + k2]); }
            serial_puts("'\n");
            var matched = true;
            var i: u16 = 0;
            while (i < de.name_len) : (i += 1) {
                var nc = name[i];
                var dc = de_name[i];
                // ~~ case-insensitive: usuário digita /BIN, disco tem /bin~~
                if (nc >= 'a' and nc <= 'z') nc -= 0x20;
                if (dc >= 'a' and dc <= 'z') dc -= 0x20;
                if (nc == 0 or nc != dc) {
                    matched = false;
                    break;
                }
            }
            if (matched and name[de.name_len] == 0)
                return de.inode;
        }

        offset += de.rec_len;
    }

    return 0;
}

// ~~ resolve_path ~~
// Resolve um caminho tipo "/home/user/file.txt" pro inode correspondente.
// Começa na raiz (inode 2) e vai descendo componente por componente,
// chamando match_name_dir em cada nível. Se falhar, retorna 0 (e você chora~)
fn resolve_path(path: [*:0]const u8) u32 {
    if (path[0] == 0) return EXT2_ROOT_INO;

    var inum = EXT2_ROOT_INO;
    var p: usize = 0;

    // Skip leading /
    while (path[p] == '/') : (p += 1) {}

    if (path[p] == 0) return EXT2_ROOT_INO;

    while (path[p] != 0) {
        const start = p;
        while (path[p] != 0 and path[p] != '/') : (p += 1) {}

        const saved = path[p];
        var component: [256]u8 = undefined;
        var ci: usize = 0;
        while (start + ci < p and ci < 255) : (ci += 1)
            component[ci] = path[start + ci];
        component[ci] = 0;

        inum = match_name_dir(inum, @ptrCast(&component));
        if (inum == 0) return 0;

        if (saved == '/') {
            p += 1; // skip /
            while (path[p] == '/') : (p += 1) {}
        }
    }

    return inum;
}

// ~~ ext2_init ~~
// Monta o filesystem ext2! Lê o superbloco do disco (offset 1024),
// verifica o magic 0xEF53 (se não bater, desiste~), calcula block_size
// e localiza a tabela de descritores de grupo.
// Retorna 0 se OK, -1 se deu ruim.
export fn ext2_init() i32 {
    // Read superblock from offset 1024 (block 0 if block_size=1024, block 0 sector 2)
    // Actually s_first_data_block is usually 0 for block_size=1024
    // Superblock is at byte offset 1024 = sector 2
    var temp: [1024]u8 = undefined;
    // ~~ Superbloco no byte 1024 = LBAs 2 e 3 (não 0/1, que é a área de boot!)~~
    if (ata_read_sector(2, @as([*]u8, &temp)) < 0) {
        serial_puts("ext2: no disk\n");
        return -1;
    }
    if (ata_read_sector(3, @as([*]u8, &temp) + 512) < 0) {
        serial_puts("ext2: read err\n");
        return -1;
    }

    const sb_raw = @as(*const Ext2Superblock, @ptrCast(@alignCast(&temp[0])));
    if (sb_raw.s_magic != EXT2_MAGIC) {
        serial_puts("ext2: bad magic\n");
        return -1;
    }

    sb = sb_raw.*;
    block_size = @as(u32, 1024) << @as(u5, @truncate(sb.s_log_block_size));
    sectors_per_block = block_size / 512;

    // Block group descriptor table starts at block 1 (for 1024 block size)
    // or block 0 (for larger block sizes)
    if (block_size == 1024) {
        bgdt_block = 2;
    } else {
        bgdt_block = 1;
    }

    serial_puts("ext2: mounted, blk=");
    serial_puthex(block_size);
    serial_puts(" inodes=");
    serial_puthex(sb.s_inodes_count);
    serial_puts("\n");

    mounted = true;
    return 0;
}

// ~~ ext2_read_file ~~
// Lê um arquivo inteiro (parcialmente, respeitando `size`) pro buffer.
// Resolve o path, pega o inode, e copia bloco por bloco.
// Retorna quantos bytes foram lidos, ou -1 se deu erro.
// Se `size` for menor que o arquivo, só lê `size` bytes (fofa~)
export fn ext2_read_file(path: [*:0]const u8, buffer: [*]u8, size: u32) i32 {
    if (!mounted) return -1;

    serial_puts("x2rd: path='");
    serial_puts(path);
    serial_puts("' sz=");
    serial_puthex(size);
    serial_puts("\n");

    const inum = resolve_path(path);
    if (inum == 0) return -1;

    var inode: Ext2Inode = undefined;
    if (!read_inode(inum, &inode)) return -1;
    if ((inode.i_mode & EXT2_S_IFMT) != EXT2_S_IFREG) {
        serial_puts("x2rd: nao-REG ino=");
        serial_puthex(inum);
        serial_puts("\n");
        return -1;
    }

    const file_size = inode.i_size_low;
    serial_puts("x2rd: ler ");
    serial_puthex(file_size);
    serial_puts("B b0=");
    serial_puthex(inode.i_block[0]);
    serial_puts("\n");
    const to_read = if (size < file_size) size else file_size;

    var offset: u32 = 0;
    var block_idx: u32 = 0;
    while (offset < to_read) {
        var block_data: [4096]u8 = undefined;
        if (!read_block_data(&inode, block_idx, @as([*]u8, &block_data))) return -1;

        const chunk = if (to_read - offset < block_size) to_read - offset else block_size;
        var i: u32 = 0;
        while (i < chunk) : (i += 1) buffer[offset + i] = block_data[i];
        offset += chunk;
        block_idx += 1;
    }

    return @as(i32, @intCast(to_read));
}

// ~~ ext2_stat ~~
// Retorna o tamanho do arquivo via `size` (sem ler os dados~).
// Útil pra saber quanto alocar antes de ler. Retorna 0 se OK, -1 se falhou.
export fn ext2_stat(path: [*:0]const u8, size: *u32) i32 {
    if (!mounted) return -1;

    const inum = resolve_path(path);
    if (inum == 0) return -1;

    var inode: Ext2Inode = undefined;
    if (!read_inode(inum, &inode)) return -1;

    size.* = inode.i_size_low;
    return 0;
}

// ~~ ext2_list_dir ~~
// [TODO] Lista entradas de um diretório. Por enquanto é só um placeholder
// que retorna 0 sem fazer nada. Um dia eu implemento, prometo~ ☆
export fn ext2_list_dir(path: [*:0]const u8, entries: [*]u64, max_entries: i32) i32 {
    _ = entries;
    _ = max_entries;
    if (!mounted) return -1;
    const inum = resolve_path(path);
    if (inum == 0) return -1;
    return 0; // TODO: implement directory listing
}
// ~~~~~~~~~~ ESCRITA BÁSICA (create + extend) ~~~~~~~~~~
// MVP: cria arquivo regular vazio no dir pai e escreve por cima,
// estendendo com blocos novos (diretos+indiretos, sem triple~)

// ~~ test_and_set_bit num bitmap já em memória ~~
fn bitmap_set(buf: [*]u8, bit: u32) void {
    const byte = bit / 8;
    const off: u3 = @intCast(bit % 8);
    buf[byte] |= (@as(u8, 1) << off);
}

fn bitmap_test(buf: [*]u8, bit: u32) bool {
    const byte = bit / 8;
    const off: u3 = @intCast(bit % 8);
    return (buf[byte] & (@as(u8, 1) << off)) != 0;
}

// ~~ ext2_alloc_block ~ acha bloco livre no bitmap e marca ~~
// Retorna 0 se não achou. Atualiza o contador livre do grupo~
var alloc_block_buf: [4096]u8 = undefined;

fn ext2_alloc_block() i32 {
    if (!mounted) return 0;
    const groups = (sb.s_blocks_count + sb.s_blocks_per_group - 1) / sb.s_blocks_per_group;
    var bg: u32 = 0;
    while (bg < groups) : (bg += 1) {
        // descritor do grupo
        const descs_per_block = block_size / 32;
        const gd_block = bgdt_block + bg / descs_per_block;
        const gd_off = (bg % descs_per_block) * 32;
        const gd_data = read_block(gd_block) orelse return 0;
        // ~~ COPIA o descritor: read_block do bitmap vai sobrescrever o cache~~
        const gd: Ext2BlockGroupDesc = @as(*const Ext2BlockGroupDesc, @ptrCast(@alignCast(gd_data + gd_off))).*;
        if (gd.bg_free_blocks_count == 0) continue;

        const bmap_block = gd.bg_block_bitmap;
        // copia bitmap pra buffer mutável
        var bi: usize = 0;
        while (bi < block_size) : (bi += 1) {
            const src = read_block(bmap_block) orelse return 0;
            alloc_block_buf[bi] = src[bi];
        }

        // primeiro bloco procurável deste grupo
        const first = sb.s_first_data_block + bg * sb.s_blocks_per_group;
        const last = @min(first + sb.s_blocks_per_group, sb.s_blocks_count);
        var cand: u32 = first;
        while (cand < last) : (cand += 1) {
            const rel = cand - first;
            if (!bitmap_test(&alloc_block_buf, @intCast(rel))) {
                bitmap_set(&alloc_block_buf, @intCast(rel));
                _ = write_block_raw(bmap_block, &alloc_block_buf);
                // ~~ re-lê o bloco de GDs FRESCO e patcha só os 32 bytes~~
                const gd_fresh = read_block(gd_block) orelse return 0;
                var gdf: Ext2BlockGroupDesc = @as(*const Ext2BlockGroupDesc, @ptrCast(@alignCast(gd_fresh + gd_off))).*;
                if (gdf.bg_free_blocks_count > 0) gdf.bg_free_blocks_count -= 1;
                {
                    const bytes: [*]u8 = @ptrCast(&gdf);
                    @memcpy(gd_fresh[gd_off .. gd_off + 32], bytes[0..32]);
                }
                _ = write_block_raw(gd_block, gd_fresh[0..4096]);
                sb.s_free_blocks_count -= 1;
                write_superblock();
                return @intCast(cand);
            }
        }
    }
    return 0;
}

// ~~ write_block_raw ~ grava o cache/buffer no disco ~~
fn write_block_raw(block: u32, buf: [*]const u8) i32 {
    const start_lba = sector_to_lba(block, 0);
    var si: u32 = 0;
    while (si < sectors_per_block) : (si += 1) {
        if (ata_write_sector(start_lba + si, @constCast(buf + si * 512)) < 0) return -1;
    }
    if (cached_block == block) cached_block = 0xFFFFFFFF; // invalida cache~
    return 0;
}



// ~~ decrementa contador de livres no descritor (blocks ou inodes) ~~
fn gd_free_dec(gd: *Ext2BlockGroupDesc, blocks: bool) void {
    if (blocks) {
        if (gd.bg_free_blocks_count > 0) gd.bg_free_blocks_count -= 1;
    } else {
        if (gd.bg_free_inodes_count > 0) gd.bg_free_inodes_count -= 1;
    }
}

fn write_superblock() void {
    var tmp: [1024]u8 = undefined;
    _ = ata_read_sector(2, &tmp);
    _ = ata_read_sector(3, tmp[512..]);
    const sbp = @as(*Ext2Superblock, @ptrCast(@alignCast(&tmp[0])));
    sbp.* = sb;
    _ = ata_write_sector(2, &tmp);
    _ = ata_write_sector(3, tmp[512..]);
}

// ~~~~~~~~~~ E2: alocação de inode + link + write_at ~~~~~~~~~~

var inode_bitmap_buf: [4096]u8 = undefined;

fn ext2_alloc_inode() i32 {
    const groups = (sb.s_inodes_count + sb.s_inodes_per_group - 1) / sb.s_inodes_per_group;
    var bg: u32 = 0;
    while (bg < groups) : (bg += 1) {
        const descs_per_block = block_size / 32;
        const gd_block = bgdt_block + bg / descs_per_block;
        const gd_off = (bg % descs_per_block) * 32;
        const gd_data = read_block(gd_block) orelse return 0;
        const gd: Ext2BlockGroupDesc = @as(*const Ext2BlockGroupDesc, @ptrCast(@alignCast(gd_data + gd_off))).*;
        if (gd.bg_free_inodes_count == 0) continue;

        var bi: usize = 0;
        while (bi < block_size) : (bi += 1) {
            const src2 = read_block(gd.bg_inode_bitmap) orelse return 0;
            inode_bitmap_buf[bi] = src2[bi];
        }

        const first = bg * sb.s_inodes_per_group + 1;
        var cand: u32 = 11; // 1..10 reservados
        while (cand < first + sb.s_inodes_per_group and cand <= sb.s_inodes_count) : (cand += 1) {
            const rel = cand - first;
            if (!bitmap_test(&inode_bitmap_buf, @intCast(rel))) {
                bitmap_set(&inode_bitmap_buf, @intCast(rel));
                _ = write_block_raw(gd.bg_inode_bitmap, &inode_bitmap_buf);
                const gd_fresh = read_block(gd_block) orelse return 0;
                var gdf: Ext2BlockGroupDesc = @as(*const Ext2BlockGroupDesc, @ptrCast(@alignCast(gd_fresh + gd_off))).*;
                if (gdf.bg_free_inodes_count > 0) gdf.bg_free_inodes_count -= 1;
                {
                    const bytes: [*]u8 = @ptrCast(&gdf);
                    @memcpy(gd_fresh[gd_off .. gd_off + 32], bytes[0..32]);
                }
                _ = write_block_raw(gd_block, gd_fresh[0..4096]);
                sb.s_free_inodes_count -= 1;
                write_superblock();
                return @intCast(cand);
            }
        }
    }
    return 0;
}

fn write_inode(inum: u32, inode: *const Ext2Inode) bool {
    const bg = inode_to_bg(inum);
    const idx = inode_to_idx(inum);
    const descs_per_block = block_size / 32;
    const gd_block = bgdt_block + bg / descs_per_block;
    const gd_off = (bg % descs_per_block) * 32;
    const gd_data = read_block(gd_block) orelse return false;
    const gd = @as(*const Ext2BlockGroupDesc, @ptrCast(@alignCast(gd_data + gd_off)));
    const table = gd.bg_inode_table;
    const per = block_size / 128;
    const blk = table + idx / per;
    const off = (idx % per) * 128;

    const dst = read_block(blk) orelse return false;
    const src: [*]const u8 = @ptrCast(inode);
    @memcpy(dst[off .. off + 128], src[0..128]);
    return write_block_raw(blk, dst[0..4096]) == 0;
}

// ~~ garante o bloco indireto (single/double) e devolve o bloco lógico~~
fn ensure_indirect(inode: *Ext2Inode, slot: u32, dirty_inode: *bool) ?u32 {
    const cur = inode.i_block[slot];
    if (cur != 0) return cur;
    const p = ext2_alloc_block();
    if (p == 0) return null;
    inode.i_block[slot] = @intCast(p);
    dirty_inode.* = true;
    // zera o novo bloco de ponteiros
    var zi: usize = 0;
    while (zi < block_size) : (zi += 1) alloc_block_buf[zi] = 0;
    _ = write_block_raw(@intCast(p), &alloc_block_buf);
    return @intCast(p);
}

fn set_phys(inode: *Ext2Inode, block_idx: u32, p: u32, dirty: *bool) bool {
    if (block_idx < EXT2_NDIR_BLOCKS) {
        inode.i_block[block_idx] = p;
        dirty.* = true;
        return true;
    }
    var rem = block_idx - EXT2_NDIR_BLOCKS;
    if (rem < PER_BLOCK) {
        const ib = ensure_indirect(inode, 12, dirty) orelse return false;
        const data = read_block(ib) orelse return false;
        (@as([*]u32, @ptrCast(@alignCast(data))))[rem] = p;
        _ = write_block_raw(ib, data[0..4096]);
        return true;
    }
    rem -= PER_BLOCK;
    const l1idx = 13;
    const l1b = ensure_indirect(inode, l1idx, dirty) orelse return false;
    const l1data = read_block(l1b) orelse return false;
    const l1 = (@as([*]u32, @ptrCast(@alignCast(l1data))))[rem / PER_BLOCK];
    var l1v = l1;
    if (l1v == 0) {
        const np = ext2_alloc_block();
        if (np == 0) return false;
        l1v = @intCast(np);
        (@as([*]u32, @ptrCast(@alignCast(l1data))))[rem / PER_BLOCK] = l1v;
        _ = write_block_raw(l1b, l1data[0..4096]);
    }
    const l2data = read_block(l1v) orelse return false;
    (@as([*]u32, @ptrCast(@alignCast(l2data))))[rem % PER_BLOCK] = p;
    _ = write_block_raw(l1v, l2data[0..4096]);
    return true;
}

// ~~ escreve `size` bytes em `offset` (estendendo o arquivo se preciso) ~~
export fn ext2_write_at(path: [*:0]const u8, buffer: [*]u8, size: u32, offset: u32) i32 {
    if (!mounted) return -1;
    const inum = resolve_path(path);
    if (inum == 0) return -1;

    var inode: Ext2Inode = undefined;
    if (!read_inode(inum, &inode)) return -1;
    if ((inode.i_mode & EXT2_S_IFMT) != EXT2_S_IFREG) return -1;

    var dirty_inode = false;
    const end = offset + size;
    var written: u32 = 0;

    while (written < size) : (written += 0) {
        const lo = offset + written;
        const idx = lo / block_size;
        const inoff = lo % block_size;
        const chunk = @min(block_size - inoff, size - written);

        var hole = false;
        var phys = phys_of(&inode, idx, &hole) orelse return -1;
        var newly = false;
        if (phys == 0 or !hole) {
            if (phys == 0) {
                const np = ext2_alloc_block();
                if (np == 0) return -1;
                phys = @intCast(np);
                newly = true;
                if (!set_phys(&inode, idx, phys, &dirty_inode)) return -1;
            }
        }

        // read-modify-write do bloco
        var tmp: [4096]u8 = undefined;
        if (!hole) {
            const srcblk = read_block(phys) orelse return -1;
            var ci: usize = 0;
            while (ci < block_size) : (ci += 1) tmp[ci] = srcblk[ci];
        } else {
            var ci: usize = 0;
            while (ci < block_size) : (ci += 1) tmp[ci] = 0;
        }
        var k: u32 = 0;
        while (k < chunk) : (k += 1) tmp[inoff + k] = buffer[written + k];
        if (hole or !newly or true) _ = write_block_raw(phys, &tmp);

        if (newly) {
            inode.i_blocks += (block_size / 512);
            dirty_inode = true;
        }
        written += chunk;
    }

    if (end > inode.i_size_low) {
        inode.i_size_low = end;
        dirty_inode = true;
    }
    if (dirty_inode) _ = write_inode(inum, &inode);
    return @intCast(size);
}

// ~~ cria arquivo regular vazio (sem suporte a dirs ainda~) ~~
export fn ext2_create_file(path: [*:0]const u8) i32 {
    if (!mounted) return -1;
    // já existe? sucesso~
    const exists = resolve_path(path);
    if (exists != 0) return 0;

    // separa pai/nome
    var tmp: [512]u8 = undefined;
    var ti: usize = 0;
    while (path[ti] != 0 and ti < 511) : (ti += 1) tmp[ti] = path[ti];
    tmp[ti] = 0;
    var slash: ?usize = null;
    { var k: usize = 0; while (k < ti) : (k += 1) if (tmp[k] == '/') { slash = k; }; }
    if (slash == null) return -1;
    tmp[slash.?] = 0;
    const name: [*:0]const u8 = @ptrCast(tmp[slash.? + 1..].ptr);

    const parent = resolve_path(if (slash.? == 0) "/" else @ptrCast(&tmp));
    if (parent == 0) return -1;

    const newino = ext2_alloc_inode();
    if (newino == 0) return -1;

    var node: Ext2Inode = undefined;
    {
        const bytes: [*]u8 = @ptrCast(&node);
        var zi: usize = 0;
        while (zi < @sizeOf(Ext2Inode)) : (zi += 1) bytes[zi] = 0;
    }
    node.i_mode = 0o100644; // S_IFREG | rw-r--r--
    node.i_links_count = 1;
    node.i_block = [_]u32{0} ** 15;
    if (!write_inode(@intCast(newino), &node)) return -1;

    if (!dir_link(parent, @intCast(newino), name)) return -1;
    return 0;
}

// ~~ linka entrada no fim/slack do diretório pai~~
fn dir_link(parent: u32, child: u32, name: [*:0]const u8) bool {
    var plen: usize = 0;
    while (name[plen] != 0) : (plen += 1) {}
    const need = @as(u32, @intCast((8 + plen + 3) & ~@as(u32, 3)));

    var pinode: Ext2Inode = undefined;
    if (!read_inode(parent, &pinode)) return false;
    if ((pinode.i_mode & EXT2_S_IFMT) != EXT2_S_IFDIR) return false;

    const fsize = pinode.i_size_low;
    var offset: u32 = 0;
    var block_idx: u32 = 0;
    var block_data: [4096]u8 = undefined;
    while (offset < fsize) {
        if (offset % block_size == 0) {
            if (!read_block_data(&pinode, block_idx, &block_data)) return false;
            block_idx += 1;
        }
        const inoff = offset % block_size;
        const de = @as(*Ext2DirEntry, @ptrCast(@alignCast(block_data[inoff..].ptr)));
        if (de.inode == 0 or de.rec_len == 0) break;

        const minimal = @as(u32, @intCast((8 + de.name_len + 3) & ~@as(u32, 3)));
        const slack = de.rec_len - minimal;
        if (slack >= need) {
            // divide: encolhe o atual e bota o novo no slack~
            const newoff = inoff + minimal;
            const nde = @as(*Ext2DirEntry, @ptrCast(@alignCast(block_data[newoff..].ptr)));
            nde.inode = child;
            nde.rec_len = @intCast(slack);
            nde.name_len = @intCast(plen);
            nde.file_type = 1;
            var j: usize = 0;
            while (j < plen) : (j += 1) block_data[newoff + 8 + j] = name[j];
            de.rec_len = @intCast(minimal);
            return write_block_raw(last_blk_of(offset), &block_data) == 0;
        }
        offset += de.rec_len;
    }

    // sem slack: estende com um bloco novo cheio de uma entrada só~
    const nb = ext2_alloc_block();
    if (nb == 0) return false;
    var zi: usize = 0;
    while (zi < block_size) : (zi += 1) alloc_block_buf[zi] = 0;
    const nde = @as(*Ext2DirEntry, @ptrCast(@alignCast(&alloc_block_buf)));
    nde.inode = child;
    nde.rec_len = @intCast(block_size);
    nde.name_len = @intCast(plen);
    nde.file_type = 1;
    var j: usize = 0;
    while (j < plen) : (j += 1) alloc_block_buf[8 + j] = name[j];

    const nblocks = (fsize + block_size - 1) / block_size;
    var dirty = false;
    if (!set_phys(&pinode, nblocks, @intCast(nb), &dirty)) return false;
    pinode.i_size_low = (nblocks + 1) * block_size;
    pinode.i_blocks += block_size / 512;
    if (!write_inode(parent, &pinode)) return false;
    _ = write_block_raw(@intCast(nb), &alloc_block_buf);
    return true;
}

// ~~ último bloco usado pelo arquivo (pra escrever o tail block)~~
fn last_blk_of(offset: u32) u32 {
    return (offset / block_size) * sectors_per_block;
}
