// moe moe kyun <3
// moe moe kyun <3
// Linux-compatible syscall dispatcher for TipOS (GPL v2)
// ~~ tradutor oficial Linux → TipOS ~~ n mexe sem saber oq ta fazendo <3

// ~~ Números de syscall Linux x86_64 ~~
// Tabela completa dos syscalls que a gente (tenta) suportar.
// Alguns são stub, outros funcionam de verdade~
// Se o número não tiver na tabela, ENOSYS (e você reclama~)
// Linux x86_64 syscall numbers
pub const linux = struct {
    // Basic I/O
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

    // Extended I/O ~~ pra quando o musl pedir mais ~~
    pub const pipe = 22;
    pub const dup = 32;
    pub const dup2 = 33;
    pub const pipe2 = 293;
    pub const dup3 = 292;

    // File operations
    pub const openat = 257;
    pub const mkdirat = 258;
    pub const newfstatat = 262;
    pub const unlinkat = 263;
    pub const renameat = 264;
    pub const faccessat = 269;
    pub const readlinkat = 267;
    pub const fcntl = 72;
    pub const chmod = 90;
    pub const fchmod = 91;
    pub const chown = 92;
    pub const fchown = 93;
    pub const statfs = 137;
    pub const fstatfs = 138;

    // Socket/Network ~~ pra weston/X11 rodar ~~
    pub const socket = 41;
    pub const bind = 49;
    pub const listen = 50;
    pub const accept = 43;
    pub const accept4 = 288;
    pub const connect = 42;
    pub const sendto = 44;
    pub const recvfrom = 45;
    pub const sendmsg = 46;
    pub const recvmsg = 47;
    pub const shutdown = 48;
    pub const socketpair = 53;

    // Poll/Epoll
    pub const poll = 7;
    pub const epoll_create = 213;
    pub const epoll_create1 = 291;
    pub const epoll_ctl = 232;
    pub const epoll_wait = 233;
    pub const epoll_pwait = 281;

    // Synchronization
    pub const futex = 202;
    pub const sched_yield = 24;
    pub const madvise = 28;

    // Time
    pub const clock_gettime = 228;
    pub const clock_nanosleep = 229;

    // Event loop / timers ~~ pra weston/event loop ~~
    pub const ppoll = 271;
    pub const eventfd = 323;
    pub const eventfd2 = 323; // same as eventfd with flags
    pub const timerfd_create = 283;
    pub const timerfd_settime = 286;
    pub const timerfd_gettime = 287;

    // Random
    pub const getrandom = 318;

    // Process
    pub const prlimit64 = 302;
    pub const sched_getaffinity = 204;
    pub const sched_setaffinity = 203;

    // Filesystem
    pub const statx = 332;
    pub const mount = 165;
    pub const umount2 = 166;

    // Process info
    pub const getrlimit = 97;
    pub const setrlimit = 160;
    pub const getrusage = 98;
    pub const times = 100;

    // Signals
    pub const rt_sigprocmask = 14;
    pub const rt_sigreturn = 15;
    pub const rt_sigtimedwait = 130;
    pub const rt_sigqueueinfo = 129;
    pub const sigaltstack = 131;
    pub const kill = 62;
    pub const tkill = 200;
    pub const tgkill = 234;
    pub const prctl = 157;

    // Memory
    pub const mremap = 25;
    pub const msync = 26;
    pub const mincore = 27;
    pub const mlock = 149;
    pub const munlock = 150;
    pub const mlockall = 151;
    pub const munlockall = 152;

    // Extended file ops
    pub const sendfile = 40;
    pub const sendfile64 = 339;
    pub const copy_file_range = 326;
    pub const fadvise64 = 221;
    pub const fallocate = 285;
    pub const fallocate64 = 285;

    // Directory
    pub const getdents = 78;
    pub const readdir = 78; // alias for getdents64

    // Process control
    pub const wait4 = 61;
    pub const waitid = 247;
    pub const set_tid_address = 218;
    pub const set_robust_list = 273;
    pub const get_robust_list = 274;
    pub const rseq = 334;

    // Arch prctl (TLS) ~~ pra musl ter TLS ~~
    pub const arch_prctl = 158;

    // CPU affinity
    pub const getcpu = 309;
};

// ~~ Syscalls TipOS ~~
// Números 198-211 são nossos! Coisas que o Linux não tem:
// kbhit, disp_get_fb, disp_flush, mouse_read, kb_mod,
// disp_flush_rect, readdir, shell_cmd, spawn, spawn_shared~
// Se liga, essas são as funções exclusivas do TipOS (sou especial~)
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
const linux_to_tipos: [512]u16 = brk: {
    var map: [512]u16 = undefined;
    for (&map, 0..) |*v, i| v.* = @as(u16, @intCast(i));
    map[0]   = 3;     // read
    map[1]   = 4;     // write
    map[2]   = 5;     // open
    map[3]   = 6;     // close
    map[4]   = 188;   // stat
    map[5]   = 189;   // fstat
    map[6]   = 199;   // lstat
    map[7]   = 7;     // poll (TipOS 7 = poll)
    map[8]   = 204;   // lseek
    map[9]   = 197;   // mmap
    map[10]  = 74;    // mprotect (real: reescreve PTEs)
    map[11]  = 73;    // munmap (real: desmapeia PTEs + entry)
    map[12]  = 231;   // brk (stub)
    map[13]  = 134;   // sigaction
    map[14]  = 14;    // rt_sigprocmask (livre no TipOS)
    map[15]  = 15;    // rt_sigreturn (livre no TipOS)
    map[131] = 131;   // sigaltstack (livre no TipOS)
    map[157] = 157;   // prctl (livre no TipOS)
    map[202] = 96;    // futex (202 colide c/ mouse_read) → 96 livre
    map[234] = 103;   // tgkill (234 colide c/ nanosleep) → 103 livre
    map[334] = 334;   // rseq (livre no TipOS)
    map[16]  = 54;    // ioctl (stub)
    map[21]  = 33;    // access
    map[22]  = 22;    // pipe (livre no TipOS)
    map[23]  = 23;    // select (TipOS 23 = select)
    map[24]  = 90;    // sched_yield (24 colide c/ getuid)
    map[25]  = 75;    // mremap (stub ENOSYS)
    map[26]  = 26;    // msync (no-op)
    map[27]  = 27;    // mincore (livre no TipOS)
    map[28]  = 28;    // madvise (no-op)
    map[33]  = 91;    // dup2 (33 colide c/ access)
    map[35]  = 234;   // nanosleep (stub)
    map[39]  = 20;    // getpid
    map[20]  = 409;   // writev (20 colide c/ getpid → 409 livre)
    map[40]  = 40;    // sendfile (livre no TipOS)
    map[41]  = 41;    // socket (livre no TipOS)
    map[42]  = 42;    // connect (livre no TipOS)
    map[43]  = 43;    // accept (livre no TipOS)
    map[44]  = 44;    // sendto (livre no TipOS)
    map[45]  = 45;    // recvfrom (livre no TipOS)
    map[46]  = 46;    // sendmsg (livre no TipOS)
    map[47]  = 92;    // recvmsg (47 colide c/ getgid)
    map[48]  = 408;   // shutdown (48 colide c/ getegid) → 408 livre
    map[49]  = 49;    // bind (livre no TipOS)
    map[50]  = 50;    // listen (livre no TipOS)
    map[53]  = 53;    // socketpair (livre no TipOS)
    map[54]  = 93;    // fcntl (54 colide c/ ioctl)
    map[57]  = 214;   // fork REAL (issue #72)
    map[59]  = 208;   // execve
    map[60]  = 1;     // exit
    map[61]  = 61;    // wait4 (61 livre no TipOS) — getdents é 78, não 61!
    map[62]  = 62;    // kill
    map[72]  = 72;    // fcntl
    map[78]  = 207;   // getdents via readdir
    map[83]  = 136;   // mkdir
    map[84]  = 137;   // rmdir
    map[87]  = 10;    // unlink
    map[96]  = 116;   // gettimeofday
    map[97]  = 97;    // getrlimit
    map[98]  = 98;    // getrusage
    map[100] = 100;   // times
    map[95]  = 400;   // umask (95 colide c/ tkill) → 400 livre no TipOS
    map[99]  = 401;   // sysinfo (99 colide c/ sched_setaffinity) → 401 livre
    map[102] = 24;    // getuid
    map[104] = 47;    // getgid
    map[107] = 25;    // geteuid
    map[108] = 48;    // getegid
    map[130] = 130;   // rt_sigtimedwait (livre no TipOS)
    map[129] = 129;   // rt_sigqueueinfo (livre no TipOS)
    map[134] = 94;    // rt_sigprocmask (134 colide c/ sigaction)
    map[158] = 158;   // arch_prctl
    map[160] = 160;   // setrlimit (livre no TipOS)
    map[165] = 165;   // mount (livre no TipOS)
    map[166] = 166;   // umount2 (livre no TipOS)
    map[200] = 95;    // tkill (200 colide c/ disp_get_fb)
    map[202] = 96;    // futex (202 colide c/ mouse_read)
    map[203] = 99;    // sched_setaffinity (203 colide c/ kb_mod)
    map[204] = 101;   // sched_getaffinity (204 colide c/ lseek)
    map[213] = 213;   // epoll_create (livre no TipOS)
    map[232] = 232;   // epoll_wait (livre no TipOS)
    map[233] = 233;   // epoll_ctl (livre no TipOS)
    map[234] = 103;   // tgkill (234 colide c/ nanosleep)
    map[247] = 247;   // waitid (livre no TipOS)
    map[218] = 218;   // set_tid_address (218 livre no TipOS)
    map[273] = 273;   // set_robust_list (livre no TipOS)
    map[274] = 274;   // get_robust_list (livre no TipOS)
    map[257] = 257;   // openat (livre no TipOS)
    map[258] = 258;   // mkdirat (livre no TipOS)
    map[262] = 262;   // newfstatat (livre no TipOS)
    map[263] = 263;   // unlinkat (livre no TipOS)
    map[264] = 264;   // renameat (livre no TipOS)
    map[267] = 267;   // readlinkat (livre no TipOS)
    map[90]  = 402;   // chmod (90 colide c/ sched_yield) → 402 livre
    map[91]  = 403;   // fchmod (91 colide c/ dup2) → 403 livre
    map[92]  = 404;   // chown (92 colide c/ recvmsg) → 404 livre
    map[93]  = 405;   // fchown (93 colide c/ fcntl) → 405 livre
    map[137] = 406;   // statfs (137 colide c/ rmdir2) → 406 livre
    map[138] = 407;   // fstatfs (138 livre no TipOS) → 407
    map[281] = 281;   // epoll_pwait (livre no TipOS)
    map[285] = 285;   // fallocate (livre no TipOS)
    map[288] = 288;   // accept4 (livre no TipOS)
    map[291] = 291;   // epoll_create1 (livre no TipOS)
    map[292] = 292;   // dup3 (livre no TipOS)
    map[293] = 293;   // pipe2 (livre no TipOS)
    map[296] = 296;   // prlimit64 (livre no TipOS)
    map[302] = 302;   // prlimit64 (livre no TipOS)
    map[318] = 318;   // getrandom (livre no TipOS)
    map[271] = 271;   // ppoll (livre no TipOS)
    map[283] = 283;   // timerfd_create (livre no TipOS)
    map[286] = 286;   // timerfd_settime (livre no TipOS)
    map[287] = 287;   // timerfd_gettime (livre no TipOS)
    map[290] = 290;   // eventfd2 (livre no TipOS)
    map[309] = 309;   // getcpu (livre no TipOS)
    map[332] = 332;   // statx (livre no TipOS)
    map[326] = 326;   // copy_file_range (livre no TipOS)
    map[221] = 221;   // fadvise64 (livre no TipOS)
    map[231] = 212;   // ~~ exit_group ~~
                        // Linux exit_group = 231, mas brk também = 231!
                        // Colisão! Então mapeamos pra 212 (nosso exit_group).
                        // Brk vai ser tratado direto no dispatcher C~
                        // (231 é tipo um numero de onibus que TODO
                        //  mundo quer pegar — aí a gente separa as
                        //  filas pra ninguem se machucar~)
    break :brk map;
};

// Interrupt handler register layout (pushed by idt.asm):
//   regs[0] = rax, [1]=rcx, [2]=rdx, [3]=rsi, [4]=rdi,
//   [5]=r8,  [6]=r9, [7]=r10, [8]=r11, [9]=rbx,
//   [10]=rbp, [11]=r12, [13]=r13, [14]=r14, [14]=r15
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
// e chama o handler em C. Se o número for inválido (>= 512), retorna -1~ ☆
extern fn syscall_trace(num: u64, rip: u64) void;

export fn syscall_handler_zig(regs: [*]u64) void {
    const num = regs[0];
    if (num >= 512) {
        regs[0] = 0xFFFF_FFFF_FFFF_FFFF; // -1 = ENOSYS
        return;
    }

    syscall_trace(num, regs[15]);

    const mapped = linux_to_tipos[@as(usize, @intCast(num))];
    regs[0] = @as(u64, mapped);

    // Fix up arg4 for Linux syscalls: Linux puts arg4 in r10 (regs[7]),
    // but TipOS C handler expects arg4 in regs[1] (rcx slot).
    // Only do this when the number actually changed (Linux → TipOS translation),
    // so TipOS-native calls (198-211, or identity-mapped like 186→186) keep rcx.
    // Sockets identity-mapped (socketpair=53, accept4=288) also need arg4 in rcx.
    const is_socket = (num >= 41 and num <= 53) or num == 288;
    if (mapped != num or is_socket) {
        regs[1] = regs[7]; // r10 → rcx
    }

    syscall_handler(regs);
}