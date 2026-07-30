// moe moe kyun <3
// moe moe kyun <3
// Linux-compatible syscall dispatcher for TipOS (GPL v2)

// ~~ Números de syscall Linux x86_64 ~~
// Tabela completa dos syscalls que a gente (tenta) suportar.
// Alguns são stub, outros funcionam de verdade~
// Se o número não tiver na tabela, ENOSYS (e você reclama~)
// Linux x86_64 syscall numbers
pub const linux = struct {
    pub const read = 0;
    pub const write = 1;
    pub const open = 2;
    pub const close = 3;
    pub const stat = 4;
    pub const fstat = 5;
    pub const lstat = 6;
    pub const lseek = 8;
    pub const mmap = 9;
    pub const mprotect = 10;
    pub const munmap = 11;
    pub const brk = 12;
    pub const rt_sigaction = 13;
    pub const ioctl = 16;
    pub const access = 21;
    pub const nanosleep = 35;
    pub const getpid = 39;
    pub const fork = 57;
    pub const execve = 59;
    pub const _exit = 60;
    pub const getdents64 = 78;
    pub const getcwd = 79;
    pub const chdir = 80;
    pub const mkdir = 83;
    pub const rmdir = 84;
    pub const unlink = 87;
    pub const gettimeofday = 96;
    pub const getuid = 102;
    pub const getgid = 104;
    pub const geteuid = 107;
    pub const getegid = 108;
    pub const ugetrlimit = 111;
    pub const gettid = 186;
    pub const fstatfs = 197;
};

// ~~ Syscalls TipOS ~~
// Números 198-211 são nossos! Coisas que o Linux não tem:
// kbhit, disp_get_fb, disp_flush, mouse_read, kb_mod,
// disp_flush_rect, readdir, shell_cmd, spawn, spawn_shared~
// Se liga, essas são as funções exclusivas do TipOS (sou especial~)
// TipOS-specific syscall numbers (198-211)
pub const tipos = struct {
    pub const kbhit = 198;
    pub const disp_get_fb = 200;
    pub const disp_flush = 201;
    pub const mouse_read = 202;
    pub const kb_mod = 203;
    pub const disp_flush_rect = 205;
    pub const readdir = 207;
    pub const shell_cmd = 209;
    pub const spawn = 210;
    pub const spawn_shared = 211;
};

// ~~ syscall_handler ~~
// Função externa em C que processa o syscall depois da tradução~
extern fn syscall_handler(regs: [*]u64) void;

// ~~ linux_to_tipos ~~
// Tabela de tradução de syscalls Linux → TipOS.
// Inicializa como identity mapping (cada número mapeia pra si mesmo),
// depois sobrescreve com os mapeamentos específicos.
// Por exemplo: Linux read=0 → TipOS read=3, write=1 → TipOS write=4...
// Assim programas Linux rodam sem recompilação (mágica~) ☆
const linux_to_tipos: [256]u8 = brk: {
    var map: [256]u8 = undefined;
    for (&map, 0..) |*v, i| v.* = @as(u8, @intCast(i));
    map[0]   = 3;     // read
    map[1]   = 4;     // write
    map[2]   = 5;     // open
    map[3]   = 6;     // close
    map[4]   = 188;   // stat
    map[5]   = 189;   // fstat
    map[6]   = 199;   // lstat
    map[8]   = 204;   // lseek
    map[9]   = 197;   // mmap
    map[10]  = 74;    // mprotect (stub)
    map[11]  = 73;    // munmap (stub)
    map[12]  = 231;   // brk (stub)
    map[13]  = 134;   // sigaction
    map[16]  = 54;    // ioctl (stub)
    map[21]  = 33;    // access
    map[35]  = 234;   // nanosleep (stub)
    map[39]  = 20;    // getpid
    map[57]  = 210;   // fork via spawn
    map[59]  = 208;   // execve
    map[60]  = 1;     // exit
    map[78]  = 207;   // getdents64 via readdir
    map[83]  = 136;   // mkdir
    map[84]  = 137;   // rmdir
    map[87]  = 10;    // unlink
    map[96]  = 116;   // gettimeofday
    map[102] = 24;    // getuid
    map[104] = 47;    // getgid
    map[107] = 25;    // geteuid
    map[108] = 48;    // getegid
    map[186] = 186;   // gettid via getpid
    break :brk map;
};

// Interrupt handler register layout (pushed by idt.asm):
//   regs[0] = rax, [1]=rcx, [2]=rdx, [3]=rsi, [4]=rdi,
//   [5]=r8,  [6]=r9, [7]=r10, [8]=r11, [9]=rbx,
//   [10]=rbp, [11]=r12, [12]=r13, [13]=r14, [14]=r15
//   [15..19] = iretq frame: RIP, CS, RFLAGS, RSP, SS
//
// Linux x86_64 syscall ABI (int 0x80):
//   rax = num, rdi = a1, rsi = a2, rdx = a3, r10 = a4, r8 = a5, r9 = a6
//
// TipOS C handler expects:
//   regs[4] = a1 (rdi), regs[3] = a2 (rsi), regs[2] = a3 (rdx),
//   regs[1] = a4 (rcx)

// ~~ syscall_handler_zig ~~
// Dispatcher principal de syscalls! Recebe os registradores via IDT,
// traduz o número do syscall (Linux → TipOS se necessário),
// ajusta o arg4 (Linux passa em r10, TipOS espera em rcx),
// e chama o handler em C. Se o número for inválido (>= 256), retorna -1~ ☆
export fn syscall_handler_zig(regs: [*]u64) void {
    const num = regs[0];
    if (num >= 256) {
        regs[0] = 0xFFFF_FFFF_FFFF_FFFF; // -1 = ENOSYS
        return;
    }

    const mapped = linux_to_tipos[@as(usize, @intCast(num))];
    regs[0] = mapped;

    // Fix up arg4 for Linux syscalls: Linux puts arg4 in r10 (regs[7]),
    // but TipOS C handler expects arg4 in regs[1] (rcx slot).
    // Only do this when the number actually changed (Linux → TipOS translation),
    // so TipOS-native calls (198-211, or identity-mapped like 186→186) keep rcx.
    if (mapped != num) {
        regs[1] = regs[7]; // r10 → rcx
    }

    syscall_handler(regs);
}