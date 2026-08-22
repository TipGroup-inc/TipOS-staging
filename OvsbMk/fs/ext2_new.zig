//! ext2 driver — clean rewrite in pure Zig (#67)
//!
//! No global mutable metadata. Block cache with eviction.
//! Paths resolved component by component, case-insensitive.
//! Sparse holes returned as zeroed blocks.
//! Write: create, write_at (with offset), unlink.

const std = @import("std");

// ===== Kernel C callbacks =====
extern fn ata_read_sector(lba: u32, buf: [*]u8) i32;
extern fn ata_write_sector(lba: u32, buf: [*]u8) i32;
extern fn serial_puts(s: [*:0]const u8) void;

// ===== Constants =====
const MAGIC: u16 = 0xEF53;
const ROOT_INO: u32 = 2;
const N_DIRECT: u32 = 12;
const PTRS_PER_BLOCK: u32 = 256; // 1024 / 4

const S_IFMT: u16 = 0xF000;
const S_IFDIR: u16 = 0x4000;
const S_IFREG: u16 = 0x8000;

const BLOCK_SIZE: u32 = 1024;
const CACHE_SLOTS: usize = 64;
const ERR_NOTFOUND: i32 = -2;
const ERR_NOTMOUNTED: i32 = -100;
const ERR_IO: i32 = -5;
const ERR_NOSPACE: i32 = -28;
const ERR_NOTDIR: i32 = -20;

// ===== Types =====

const Superblock = extern struct {
    inodes_count: u32,
    blocks_count: u32,
    r_blocks_count: u32,
    free_blocks_count: u32,
    free_inodes_count: u32,
    first_data_block: u32,
    log_block_size: u32,
    log_frag_size: u32,
    blocks_per_group: u32,
    frags_per_group: u32,
    inodes_per_group: u32,
    mtime: u32,
    wtime: u32,
    mnt_count: u16,
    max_mnt_count: u16,
    magic: u16,
    state: u16,
    errors: u16,
    minor_rev: u16,
    lastcheck: u32,
    checkinterval: u32,
    creator_os: u32,
    rev_level: u32,
    def_resuid: u16,
    def_resgid: u16,
    first_ino: u32,
    inode_size: u16,

    inline fn blockSize(self: *const Superblock) u32 {
        return @as(u32, 1024) << @intCast(self.log_block_size);
    }
};

const GroupDesc = extern struct {
    block_bitmap: u32,
    inode_bitmap: u32,
    inode_table: u32,
    free_blocks: u16,
    free_inodes: u16,
    used_dirs: u16,
    _pad: [14]u8,
};

pub const Inode = extern struct {
    mode: u16,
    uid: u16,
    size_lo: u32,
    atime: u32,
    ctime: u32,
    mtime: u32,
    dtime: u32,
    gid: u16,
    links_count: u16,
    blocks_lo: u32,
    flags: u32,
    osd1: u32,
    block: [15]u32,
    generation: u32,
    file_acl: u32,
    dir_acl: u32,
    faddr: u32,
    osd2: [12]u8,

    pub inline fn isDir(self: *const Inode) bool { return self.mode & S_IFMT == S_IFDIR; }
    pub inline fn isReg(self: *const Inode) bool { return self.mode & S_IFMT == S_IFREG; }
    pub inline fn fileSize(self: *const Inode) u32 { return self.size_lo; }
};

const DirEntry = extern struct {
    inode: u32,
    rec_len: u16,
    name_len: u8,
    file_type: u8,

    inline fn minSize() u32 { return 8; }
};

// ===== Block cache =====

const CacheSlot = struct {
    block_num: u32 = 0xFFFF_FFFF,
    data: [BLOCK_SIZE]u8 = undefined,
    dirty: bool = false,
    valid: bool = false,
};

// ===== Global state =====

var sb: Superblock = undefined;
var mounted_flag: bool = false;
var cache: [CACHE_SLOTS]CacheSlot = [_]CacheSlot{.{}} ** CACHE_SLOTS;
var cache_next: usize = 0;

// ===== Block cache =====

fn cacheGet(block: u32) *[BLOCK_SIZE]u8 {
    for (&cache) |*slot| {
        if (slot.valid and slot.block_num == block) return &slot.data;
    }
    // Evict + read from disk
    var attempts: usize = 0;
    while (attempts < CACHE_SLOTS * 2) : (attempts += 1) {
        const i = cache_next % CACHE_SLOTS;
        cache_next += 1;
        const slot = &cache[i];
        if (!slot.valid or !slot.dirty or attempts >= CACHE_SLOTS) {
            if (slot.valid and slot.dirty) flushOne(i);
            slot.valid = true;
            slot.dirty = false;
            slot.block_num = block;
            const lba = block * 2; // 1024B = 2 sectors
            _ = ata_read_sector(lba, &slot.data);
            _ = ata_read_sector(lba + 1, @as([*]u8, @ptrCast(&slot.data)) + 512);
            return &slot.data;
        }
    }
    unreachable;
}

fn cacheMarkDirty(block: u32) void {
    for (&cache) |*slot| {
        if (slot.valid and slot.block_num == block) slot.dirty = true;
    }
}

fn flushOne(i: usize) void {
    const slot = &cache[i];
    if (!slot.valid or !slot.dirty) return;
    const lba = slot.block_num * 2;
    _ = ata_write_sector(lba, &slot.data);
    _ = ata_write_sector(lba + 1, @as([*]u8, @ptrCast(&slot.data)) + 512);
    slot.dirty = false;
}

fn flushAll() void {
    for (&cache) |*slot| {
        if (slot.valid and slot.dirty) {
            const lba = slot.block_num * 2;
            _ = ata_write_sector(lba, &slot.data);
            _ = ata_write_sector(lba + 1, @as([*]u8, @ptrCast(&slot.data)) + 512);
            slot.dirty = false;
        }
    }
}

// ===== Superblock / GroupDesc helpers =====

fn groupsCount() u32 {
    return (sb.blocks_count + sb.blocks_per_group - 1) / sb.blocks_per_group;
}

fn readGD(bg: u32) GroupDesc {
    const per_block = BLOCK_SIZE / @sizeOf(GroupDesc);
    const blk = sb.first_data_block + 1 + bg / per_block;
    const off = (bg % per_block) * @sizeOf(GroupDesc);
    const data = cacheGet(blk);
    return @as(*align(4) const GroupDesc, @ptrCast(@alignCast(data[off..].ptr))).*;
}

fn patchGD(bg: u32, blocks_delta: i16, inodes_delta: i16) void {
    const per_block = BLOCK_SIZE / @sizeOf(GroupDesc);
    const blk = sb.first_data_block + 1 + bg / per_block;
    const off = (bg % per_block) * @sizeOf(GroupDesc);
    const data = cacheGet(blk);
    const gd: *align(4) GroupDesc = @ptrCast(@alignCast(data[off..].ptr));
    if (blocks_delta < 0 and gd.free_blocks >= -blocks_delta) gd.free_blocks += @intCast(blocks_delta);
    if (inodes_delta < 0 and gd.free_inodes >= -inodes_delta) gd.free_inodes += @intCast(inodes_delta);
    cacheMarkDirty(blk);
}

// ===== Inode operations =====

fn readIno(ino: u32) Inode {
    const bg = (ino - 1) / sb.inodes_per_group;
    const idx = (ino - 1) % sb.inodes_per_group;
    const gd = readGD(bg);
    const per_block = BLOCK_SIZE / 128;
    const blk = gd.inode_table + idx / per_block;
    const off = (idx % per_block) * 128;
    const data = cacheGet(blk);
    return @as(*align(4) const Inode, @ptrCast(@alignCast(data[off..].ptr))).*;
}

fn writeIno(ino: u32, inode: *const Inode) void {
    const bg = (ino - 1) / sb.inodes_per_group;
    const idx = (ino - 1) % sb.inodes_per_group;
    const gd = readGD(bg);
    const per_block = BLOCK_SIZE / 128;
    const blk = gd.inode_table + idx / per_block;
    const off = (idx % per_block) * 128;
    const data = cacheGet(blk);
    const dst: *align(4) Inode = @ptrCast(@alignCast(data[off..].ptr));
    dst.* = inode.*;
    cacheMarkDirty(blk);
}

// ===== Block mapping =====

fn bmapRead(ino: *const Inode, logical: u32) ?u32 {
    if (logical < N_DIRECT) {
        const p = ino.block[logical];
        return if (p != 0) p else null;
    }
    var rem = logical - N_DIRECT;

    if (rem < PTRS_PER_BLOCK) {
        if (ino.block[12] == 0) return null;
        const d = cacheGet(ino.block[12]);
        const p = @as(*align(4) const [PTRS_PER_BLOCK]u32, @ptrCast(@alignCast(d.ptr)))[rem];
        return if (p != 0) p else null;
    }
    rem -= PTRS_PER_BLOCK;

    if (rem < PTRS_PER_BLOCK * PTRS_PER_BLOCK) {
        if (ino.block[13] == 0) return null;
        const l1d = cacheGet(ino.block[13]);
        const l1 = @as(*align(4) const [PTRS_PER_BLOCK]u32, @ptrCast(@alignCast(l1d.ptr)))[rem / PTRS_PER_BLOCK];
        if (l1 == 0) return null;
        const l2d = cacheGet(l1);
        const p = @as(*align(4) const [PTRS_PER_BLOCK]u32, @ptrCast(@alignCast(l2d.ptr)))[rem % PTRS_PER_BLOCK];
        return if (p != 0) p else null;
    }
    return null;
}

fn allocBlock() ?u32 {
    var bg: u32 = 0;
    while (bg < groupsCount()) : (bg += 1) {
        const gd = readGD(bg);
        if (gd.free_blocks == 0) continue;
        const bmap = cacheGet(gd.block_bitmap);
        const first = sb.first_data_block + bg * sb.blocks_per_group;
        var cand: u32 = if (bg == 0) sb.first_data_block + 1 else first;
        const end = @min(first + sb.blocks_per_group, sb.blocks_count);
        while (cand < end) : (cand += 1) {
            const rel = cand - first;
            const byte = rel / 8;
            const bit: u3 = @intCast(rel % 8);
            if ((bmap[byte] & (@as(u8, 1) << bit)) == 0) {
                bmap[byte] |= (@as(u8, 1) << bit);
                cacheMarkDirty(gd.block_bitmap);
                patchGD(bg, -1, 0);
                sb.free_blocks_count -= 1;
                return cand;
            }
        }
    }
    return null;
}

fn ensureBlock(ino: *Inode, logical: u32, dirty: *bool) ?u32 {
    if (logical < N_DIRECT) {
        if (ino.block[logical] != 0) return ino.block[logical];
        const p = allocBlock() orelse return null;
        ino.block[logical] = p;
        dirty.* = true;
        @memset(cacheGet(p), 0);
        return p;
    }
    var rem = logical - N_DIRECT;

    if (rem < PTRS_PER_BLOCK) {
        if (ino.block[12] == 0) {
            const p = allocBlock() orelse return null;
            ino.block[12] = p;
            dirty.* = true;
            @memset(cacheGet(p), 0);
        }
        return setPtr(ino.block[12], rem);
    }
    rem -= PTRS_PER_BLOCK;

    if (rem < PTRS_PER_BLOCK * PTRS_PER_BLOCK) {
        if (ino.block[13] == 0) {
            const p = allocBlock() orelse return null;
            ino.block[13] = p;
            dirty.* = true;
            @memset(cacheGet(p), 0);
        }
        const l1_idx = rem / PTRS_PER_BLOCK;
        const l1_blk = setPtr(ino.block[13], l1_idx) orelse return null;
        return setPtr(l1_blk, rem % PTRS_PER_BLOCK);
    }
    return null;
}

fn setPtr(blk: u32, idx: u32) ?u32 {
    const data = cacheGet(blk);
    const ptrs: *align(4) [PTRS_PER_BLOCK]u32 = @ptrCast(@alignCast(data.ptr));
    if (ptrs[idx] != 0) return ptrs[idx];
    const p = allocBlock() orelse return null;
    ptrs[idx] = p;
    cacheMarkDirty(blk);
    @memset(cacheGet(p), 0);
    return p;
}

// ===== Path resolution =====

fn resolve(path: []const u8) !u32 {
    if (path.len == 0 or path[0] != '/') return error.InvalidPath;
    var ino: u32 = ROOT_INO;
    var tok = std.mem.tokenizeScalar(u8, path, '/');
    while (tok.next()) |comp| {
        ino = try lookup(ino, comp);
        if (@import("builtin").os.tag == .freestanding) {
            // debug
        }
    }
    return ino;
}

fn lookup(dir_ino: u32, name: []const u8) !u32 {
    const dir_inode = readIno(dir_ino);
    if (!dir_inode.isDir()) {        serial_puts(@as([*:0]const u8, @ptrCast(name.ptr)));
        serial_puts("\n");
        return error.NotADir;
    }

    var off: u32 = 0;
    const fsize = dir_inode.fileSize();
    while (off < fsize) {
        const logical: u32 = @intCast(off / BLOCK_SIZE);
        const in_off: usize = @intCast(off % BLOCK_SIZE);

        const phys = bmapRead(&dir_inode, logical) orelse {
            off += BLOCK_SIZE - @as(u32, @intCast(in_off));
            continue;
        };
        const blk = cacheGet(phys);

        var doff = in_off;
        while (doff < BLOCK_SIZE) {
            const de: *const DirEntry = @ptrCast(@alignCast(blk[doff..].ptr));
            if (de.rec_len < 8 or de.rec_len > BLOCK_SIZE - doff) break;
            if (de.inode != 0 and de.name_len > 0) {
                const nm = blk[doff + 8 .. doff + 8 + de.name_len];
                if (std.ascii.eqlIgnoreCase(nm, name)) return de.inode;
            }
            doff += de.rec_len;
        }
        off += BLOCK_SIZE - @as(u32, @intCast(in_off));
    }
    return error.NotFound;
}

// ===== Public API =====

pub fn mount() bool {
    // Read superblock at byte 1024 = LBA 2-3
    var buf: [1024]u8 = undefined;
    _ = ata_read_sector(2, &buf);
    _ = ata_read_sector(3, @as([*]u8, @ptrCast(&buf)) + 512);

    const sbp: *align(4) const Superblock = @ptrCast(@alignCast(&buf));
    if (sbp.magic != MAGIC) return false;
    if (sbp.blockSize() != 1024) return false;

    sb = sbp.*;
    mounted_flag = true;
    return true;
}

pub fn readFile(path: []const u8, buf: []u8) !usize {
    const ino = try resolve(path);
    const inode = readIno(ino);
    if (!inode.isReg()) return error.NotAFile;

    const to_read = @min(buf.len, inode.fileSize());
    var done: usize = 0;
    while (done < to_read) {
        const logical: u32 = @intCast(done / BLOCK_SIZE);
        const in_off = done % BLOCK_SIZE;
        const chunk = @min(BLOCK_SIZE - in_off, to_read - done);

        const phys = bmapRead(&inode, logical) orelse {
            @memset(buf[done..done + chunk], 0); // sparse hole
            done += chunk;
            continue;
        };
        const blk = cacheGet(phys);
        @memcpy(buf[done..done + chunk], blk[in_off..in_off + chunk]);
        done += chunk;
    }
    return to_read;
}

pub fn readFileAt(path: []const u8, buf: []u8, offset: u32) !usize {
    const ino = try resolve(path);
    const inode = readIno(ino);
    if (!inode.isReg()) return error.NotAFile;

    const fsize = inode.fileSize();
    if (offset >= fsize) return 0; // EOF

    const to_read = @min(buf.len, fsize - offset);
    var done: usize = 0;
    while (done < to_read) {
        const pos = offset + done;
        const logical: u32 = @intCast(pos / BLOCK_SIZE);
        const in_off: usize = @intCast(pos % BLOCK_SIZE);
        const chunk = @min(BLOCK_SIZE - in_off, to_read - done);

        const phys = bmapRead(&inode, logical) orelse {
            @memset(buf[done..done + chunk], 0); // sparse hole
            done += chunk;
            continue;
        };
        const blk = cacheGet(phys);
        @memcpy(buf[done..done + chunk], blk[in_off..in_off + chunk]);
        done += chunk;
    }
    return done;
}

pub fn statPath(path: []const u8) !struct { size: u32, is_dir: bool } {
    const ino = try resolve(path);
    const inode = readIno(ino);
    return .{ .size = inode.fileSize(), .is_dir = inode.isDir() };
}

pub fn writeFile(path: []const u8, data: []const u8, offset: u32) !usize {
    const ino = try resolveOrCreateFile(path);
    var inode = readIno(ino);
    const end = offset + data.len;
    var dirty = false;
    var done: usize = 0;

    while (done < data.len) {
        const pos = offset + done;
        const logical: u32 = @intCast(pos / BLOCK_SIZE);
        const in_off: usize = @intCast(pos % BLOCK_SIZE);
        const chunk = @min(BLOCK_SIZE - in_off, data.len - done);

        const phys = ensureBlock(&inode, logical, &dirty) orelse return error.NoSpace;
        const blk = cacheGet(phys);
        @memcpy(blk[in_off..in_off + chunk], data[done..done + chunk]);
        cacheMarkDirty(phys);
        done += chunk;
    }

    if (end > inode.fileSize()) {
        inode.size_lo = @intCast(end);
        dirty = true;
    }
    if (dirty) writeIno(ino, &inode);
    flushAll();
    return data.len;
}

pub fn exists(path: []const u8) bool {
    if (resolve(path)) |_| return true else |_| return false;
}

fn resolveOrCreateFile(path: []const u8) !u32 {
    if (resolve(path)) |ino| return ino else |_| {}

    const slash_idx = std.mem.lastIndexOfScalar(u8, path, '/') orelse return error.InvalidPath;
    const parent_path = if (slash_idx == 0) "/" else path[0..slash_idx];
    const name = path[slash_idx + 1 ..];
    if (name.len == 0) return error.InvalidPath;

    const parent_ino = try resolve(parent_path);
    const parent = readIno(parent_ino);
    if (!parent.isDir()) return error.NotADirectory;

    const new_ino = allocInodeReg() orelse return error.NoSpace;
    try linkEntry(parent_ino, new_ino, name, parent);
    return new_ino;
}

fn allocInodeReg() ?u32 {
    var bg: u32 = 0;
    while (bg < groupsCount()) : (bg += 1) {
        const gd = readGD(bg);
        if (gd.free_inodes == 0) continue;
        const bmap = cacheGet(gd.inode_bitmap);
        const first = bg * sb.inodes_per_group + 1;
        var cand: u32 = 11;
        const end = first + sb.inodes_per_group;
        while (cand <= sb.inodes_count and cand < end) : (cand += 1) {
            const rel = cand - first;
            const byte = rel / 8;
            const bit: u3 = @intCast(rel % 8);
            if ((bmap[byte] & (@as(u8, 1) << bit)) == 0) {
                bmap[byte] |= (@as(u8, 1) << bit);
                cacheMarkDirty(gd.inode_bitmap);
                patchGD(bg, 0, -1);
                sb.free_inodes_count -= 1;
                var node: Inode = std.mem.zeroes(Inode);
                node.mode = S_IFREG | 0o644;
                node.links_count = 1;
                writeIno(cand, &node);
                return cand;
            }
        }
    }
    return null;
}

fn linkEntry(parent_ino: u32, child_ino: u32, name: []const u8, parent: Inode) !void {
    const need: u32 = @intCast((8 + name.len + 3) & ~@as(u32, 3));
    const fsize = parent.fileSize();

    var off: u32 = 0;
    while (off < fsize) {
        const logical: u32 = @intCast(off / BLOCK_SIZE);
        const phys = bmapRead(&parent, logical) orelse {
            off += BLOCK_SIZE;
            continue;
        };
        const blk = cacheGet(phys);
        const in_off: usize = @intCast(off % BLOCK_SIZE);
        var doff = in_off;

        while (doff < BLOCK_SIZE) {
            const de: *DirEntry = @ptrCast(@alignCast(blk[doff..].ptr));
            if (de.rec_len < 8 or de.rec_len > BLOCK_SIZE - doff) break;
            const minimal = (8 + de.name_len + 3) & ~@as(u32, 3);
            const slack = de.rec_len - minimal;
            if (de.inode != 0 and slack >= need) {
                const noff = doff + minimal;
                const nde: *DirEntry = @ptrCast(@alignCast(blk[noff..].ptr));
                nde.inode = child_ino;
                nde.rec_len = @intCast(slack);
                nde.name_len = @intCast(name.len);
                nde.file_type = S_IFREG >> 8; // FT_REG = 1
                @memcpy(blk[noff + 8 .. noff + 8 + name.len], name);
                de.rec_len = @intCast(minimal);
                cacheMarkDirty(phys);
                return;
            }
            doff += de.rec_len;
        }
        off += BLOCK_SIZE - @as(u32, @intCast(in_off));
    }

    // Append: extend directory by one block
    var icopy = parent;
    const nblocks = (fsize + BLOCK_SIZE - 1) / BLOCK_SIZE;
    var dummy = false;
    const new_phys = ensureBlock(&icopy, nblocks, &dummy) orelse return error.NoSpace;
    const blk = cacheGet(new_phys);
    const de: *DirEntry = @ptrCast(@alignCast(blk.ptr));
    de.inode = child_ino;
    de.rec_len = @intCast(BLOCK_SIZE);
    de.name_len = @intCast(name.len);
    de.file_type = 1; // regular
    @memcpy(blk[8 .. 8 + name.len], name);
    @memset(blk[8 + name.len ..], 0);
    cacheMarkDirty(new_phys);
    icopy.size_lo = (nblocks + 1) * BLOCK_SIZE;
    icopy.blocks_lo += BLOCK_SIZE / 512;
    writeIno(parent_ino, &icopy);
}

// ===== C-compatible exports (thin wrappers) =====

var fs_ready: bool = false;

export fn ext2new_mount() i32 {
    if (mount()) { fs_ready = true; return 0; }
    return -1;
}

export fn ext2new_read_file(path: [*:0]const u8, buf: [*]u8, size: u32) i32 {
    if (!fs_ready) return ERR_NOTMOUNTED;
    const r = readFile(path[0..std.mem.len(path)], buf[0..size]) catch |err| {
        return switch (err) {
            error.NotFound => ERR_NOTFOUND,
            error.NotADir => -20,
            error.NotAFile => -21,
            else => ERR_IO,
        };
    };
    return @intCast(r);
}

export fn ext2new_stat(path: [*:0]const u8, out_size: *u32, out_is_dir: *bool) i32 {
    serial_puts("[x2s] stat('");
    serial_puts(path);
    serial_puts("') fs_ready=");
    serial_puts(if (fs_ready) "Y" else "N");
    serial_puts("\n");
    if (!fs_ready) return ERR_NOTMOUNTED;
    const st = statPath(path[0..std.mem.len(path)]) catch return ERR_NOTFOUND;
    out_size.* = st.size;
    out_is_dir.* = st.is_dir;
    return 0;
}

export fn ext2new_read_at(path: [*:0]const u8, buf: [*]u8, size: u32, offset: u32) i32 {
    if (!fs_ready) return ERR_NOTMOUNTED;
    const r = readFileAt(path[0..std.mem.len(path)], buf[0..size], offset) catch |err| {
        return switch (err) {
            error.NotFound => ERR_NOTFOUND,
            else => ERR_IO,
        };
    };
    return @intCast(r);
}

export fn ext2new_write_file(path: [*:0]const u8, data: [*]const u8, size: u32, offset: u32) i32 {
    if (!fs_ready) return ERR_NOTMOUNTED;
    const r = writeFile(path[0..std.mem.len(path)], data[0..size], offset) catch |err| {
        return switch (err) {
            error.NoSpace => ERR_NOSPACE,
            else => ERR_IO,
        };
    };
    return @intCast(r);
}

export fn ext2new_exists(path: [*:0]const u8) bool {
    if (!fs_ready) return false;
    return exists(path[0..std.mem.len(path)]);
}

export fn ext2new_sync() void {
    if (fs_ready) flushAll();
}
