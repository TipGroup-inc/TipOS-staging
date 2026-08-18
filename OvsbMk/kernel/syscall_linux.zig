// moe moe kyun <3
// moe moe kyun <3
// Linux-compatible syscall dispatcher for TipOS (GPL v2)

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
    pub const fstatfs = 197;

    // Extended I/O
    pub const pipe = 22;
    pub const dup = 23;
    pub const dup2 = 33;
    pub const pipe2 = 293;
    pub const dup3 = 292;

    // File operations
    pub const openat = 257;
    pub const newfstatat = 262;
    pub const faccessat = 269;
    pub const readlinkat = 267;
    pub const fcntl = 72;

    // Socket/Network
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
    pub const rt_sigpending = 15;
    pub const rt_sigtimedwait = 130;
    pub const rt_sigqueueinfo = 129;
    pub const kill = 62;
    pub const tkill = 200;
    pub const tgkill = 234;

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
    pub const getdents = 61;
    pub const readdir = 78; // alias for getdents64

    // Process control
    pub const wait4 = 61;
    pub const waitid = 247;
    pub const set_tid_address = 258;
    pub const set_robust_list = 273;
    pub const get_robust_list = 274;

    // Arch prctl (TLS)
    pub const arch_prctl = 158;

    // CPU affinity
    pub const getcpu = 309;
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
    map[7]   = 22;    // pipe
    map[8]   = 204;   // lseek
    map[9]   = 197;   // mmap
    map[10]  = 74;    // mprotect (stub)
    map[11]  = 73;    // munmap (stub)
    map[12]  = 231;   // brk (stub)
    map[13]  = 134;   // sigaction
    map[14]  = 14;    // rt_sigprocmask
    map[15]  = 15;    // rt_sigpending
    map[16]  = 54;    // ioctl (stub)
    map[21]  = 33;    // access
    map[22]  = 22;    // pipe
    map[23]  = 23;    // dup
    map[24]  = 24;    // sched_yield
    map[25]  = 25;    // mremap
    map[26]  = 26;    // msync
    map[27]  = 27;    // mincore
    map[28]  = 28;    // madvise
    map[33]  = 33;    // dup2
    map[35]  = 234;   // nanosleep (stub)
    map[39]  = 20;    // getpid
    map[40]  = 40;    // sendfile
    map[41]  = 41;    // socket
    map[42]  = 42;    // connect
    map[43]  = 43;    // accept
    map[44]  = 44;    // sendto
    map[45]  = 45;    // recvfrom
    map[46]  = 46;    // sendmsg
    map[46]  = 47;    // recvmsg
    map[47]  = 47;    // recvmsg
    map[49]  = 49;    // bind
    map[50]  = 50;    // listen
    map[54]  = 54;    // fcntl
    map[57]  = 210;   // fork via spawn
    map[59]  = 208;   // execve
    map[60]  = 1;     // exit
    map[61]  = 61;    // getdents
    map[62]  = 62;    // kill
    map[72]  = 72;    // fcntl
    map[78]  = 207;   // getdents64 via readdir
    map[83]  = 136;   // mkdir
    map[84]  = 137;   // rmdir
    map[87]  = 10;    // unlink
    map[96]  = 116;   // gettimeofday
    map[97]  = 97;    // getrlimit
    map[98]  = 98;    // getrusage
    map[100] = 100;   // times
    map[102] = 24;    // getuid
    map[104] = 47;    // getgid
    map[107] = 25;    // geteuid
    map[108] = 48;    // getegid
    map[130] = 130;   // rt_sigtimedwait
    map[129] = 129;   // rt_sigqueueinfo
    map[134] = 134;   // rt_sigprocmask
    map[158] = 158;   // arch_prctl
    map[160] = 160;   // setrlimit
    map[165] = 165;   // mount
    map[166] = 166;   // umount2
    map[200] = 200;   // tkill
    map[202] = 202;   // futex
    map[203] = 203;   // sched_setaffinity
    map[204] = 204;   // sched_getaffinity
    map[213] = 213;   // epoll_create
    map[232] = 232;   // epoll_ctl
    map[233] = 233;   // epoll_wait
    map[233] = 233;   // epoll_wait
    map[234] = 234;   // tgkill
    map[247] = 247;   // waitid
    map[258] = 258;   // set_tid_address
    map[273] = 273;   // set_robust_list
    map[274] = 274;   // get_robust_list
    map[281] = 281;   // epoll_pwait
    map[285] = 285;   // fallocate
    map[288] = 288;   // accept4
    map[291] = 291;   // epoll_create1
    map[292] = 292;   // dup3
    map[293] = 293;   // pipe2
    map[296] = 296;   // prlimit64
    map[302] = 302;   // prlimit64
    map[293] = 293;   // pipe2
    map[318] = 318;   // getrandom
    map[309] = 309;   // getcpu
    map[332] = 332;   // statx
    map[339] = 339;   // sendfile64
    map[326] = 326;   // copy_file_range
    map[221] = 221;   // fadvise64
    map[296] = 296;   // prlimit64
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
// e chama o handler em C. Se o número for inválido (>= 512), retorna -1~ ☆
export fn syscall_handler_zig(regs: [*]u64) void {
    const num = regs[0];
    if (num >= 512) {
        regs[0] = 0xFFFF_FFFF_FFFF_FFFF; // -1 = ENOSYS
        return;
    }

    const mapped = linux_to_tipos[@as(usize, @intCast(num))];
    regs[0] = @as(u64, mapped);

    // Fix up arg4 for Linux syscalls: Linux puts arg4 in r10 (regs[7]),
    // but TipOS C handler expects arg4 in regs[1] (rcx slot).
    // Only do this when the number actually changed (Linux → TipOS translation),
    // so TipOS-native calls (198-211, or identity-mapped like 186→186) keep rcx.
    if (mapped != num) {
        regs[1] = regs[7]; // r10 → rcx
    }

    syscall_handler(regs);
}