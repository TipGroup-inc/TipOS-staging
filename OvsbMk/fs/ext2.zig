// moe moe kyun <3
// moe moe kyun <3
// Ext2 read-only driver for TipOS — Linux native fs (GPL v2)

// ~~ Externs ~~ chamando as funções do kernel ~ socorro!
// ata_read_sector: lê 512 bytes do disco (LBA addressing, pq sim)
// serial_puts/puthex: debug via serial (eu gosto de ver o que acontece)
extern fn ata_read_sector(lba: u32, buffer: [*]u8) i32;
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
    name_len: u16,
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
fn read_block_data(inode: *const Ext2Inode, block_idx: u32, buf: [*]u8) bool {
    var phys_block: u32 = 0;

    if (block_idx < EXT2_NDIR_BLOCKS) {
        phys_block = inode.i_block[block_idx];
    } else {
        // Simple single-indirect support
        const indirect_idx = block_idx - EXT2_NDIR_BLOCKS;
        const indirect_block = inode.i_block[EXT2_NDIR_BLOCKS];
        if (indirect_block == 0) return false;

        const ind_data = read_block(indirect_block) orelse return false;
        const entries = @as([*]const u32, @ptrCast(@alignCast(ind_data)));
        phys_block = entries[indirect_idx];
    }

    if (phys_block == 0) return false;

    const block_data = read_block(phys_block) orelse return false;
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
    if (!read_inode(dir_inode, &inode)) return 0;
    if ((inode.i_mode & EXT2_S_IFMT) != EXT2_S_IFDIR) return 0;

    const size = inode.i_size_low;
    var offset: u32 = 0;
    var block_idx: u32 = 0;
    var block_data: [4096]u8 = undefined;

    while (offset < size) {
        if (offset % block_size == 0) {
            if (!read_block_data(&inode, block_idx, @as([*]u8, &block_data))) return 0;
            block_idx += 1;
        }

        const de = @as(*const Ext2DirEntry, @ptrCast(@alignCast(&block_data[offset])));
        if (de.inode == 0 or de.rec_len == 0) break;

        // Name follows the 8-byte header (inode + rec_len + name_len)
        const de_name = @as([*]const u8, @ptrCast(de)) + 8;

        if (de.name_len > 0) {
            var matched = true;
            var i: u16 = 0;
            while (i < de.name_len) : (i += 1) {
                if (name[i] == 0 or name[i] != de_name[i]) {
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
    if (ata_read_sector(0, @as([*]u8, &temp)) < 0) {
        serial_puts("ext2: no disk\n");
        return -1;
    }
    if (ata_read_sector(1, @as([*]u8, &temp) + 512) < 0) {
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

    const inum = resolve_path(path);
    if (inum == 0) return -1;

    var inode: Ext2Inode = undefined;
    if (!read_inode(inum, &inode)) return -1;
    if ((inode.i_mode & EXT2_S_IFMT) != EXT2_S_IFREG) return -1;

    const file_size = inode.i_size_low;
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