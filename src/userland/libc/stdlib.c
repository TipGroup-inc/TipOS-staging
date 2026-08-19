/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ userland ~ programinha de ring 3, longe do kernel!
 * arquivo: stdlib.c ~ funcoes anotadas: 11
 */
 /*~*~ stdlib.c ~*~
  * Hihi, olha esse arquivo aqui~ Que lindo, né? >_<
  * Escrito com muito amor (e gambiarras) pela equipe TipOS!
  * Se quebrar, a culpa é sua~ <3
  *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

// ~~ stdlib.c ~~
// Utilitários gerais da libc: alocação (malloc/free/calloc/realloc),
// conversão (atoi/itoa), mmap/munmap, e exit.
// Alocador: freelist circular com header de 4 bytes (size_aligned | USED),
// inserção ordenada por endereço, coalescência de blocos adjacentes.
// Alocações grandes (>MMAP_THRESH=2048) vão pro mmap do kernel.
 
/* ~ essa demorou pra debugar, respeita ~ */
static long _syscall(long num, long a1, long a2, long a3, long a4) {
    long ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "D"(a1), "S"(a2), "d"(a3), "c"(a4)
        : "r11", "memory"
    );
    return ret;
}

// ~~ atoi ~~
// Converte string → int. Pula espaços iniciais, aceita +/-, lê dígitos.
// Ex: "  -42" → -42, "  123" → 123. Simples e direto~ ☆
/* ~~ atoi ~~ */
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
int atoi(const char *s) {
    int n = 0, sign = 1;
    while (*s == ' ') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
    }
    return sign * n;
}

// ~~ itoa ~~
// Converte int → string (buffer fornecido por quem chama~).
// Lida com números negativos (prefixo '-'), gera dígitos ao contrário
// num temporário, depois inverte. Buffer precisa ter espaço pra 12 chars~
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
char *itoa(int n, char *buf) {
    char *p = buf;
    unsigned u;
    if (n < 0) { *p++ = '-'; u = -n; } else u = n;
    char tmp[12], *t = tmp;
    do { *t++ = '0' + (u % 10); u /= 10; } while (u);
    while (t > tmp) *p++ = *--t;
    *p = '\0';
    return buf;
}

// ~~ Alocador de memória ~~
// HEAP_SIZE = 64KB de heap estático embutido (suficiente pra programas pequenos~)
// HDR_SZ = tamanho do header (sizeof unsigned long = 4 bytes)
// Cada bloco livre/ocupado tem: [size | USED][next_pointer]
// USED = bit 0 do size indica ocupado, AMASK = ~USED pra mascarar
// NEXT(b) = ponteiro pro próximo bloco (armazenado após o header)
// ALIGN8(n) = alinha pra 8 bytes (requisito de alinhamento da libc~)
// MMAP_THRESH = 2048: acima disso, usa mmap ao invés do heap interno
#define HEAP_SIZE 65536
static char _heap[HEAP_SIZE];
static int _hinit;

#define HDR_SZ ((int)sizeof(unsigned long))
#define USED   1UL
#define AMASK  (~USED)
#define NEXT(b)  (*(void**)((char*)(b) + HDR_SZ))
#define ALIGN8(n) (((n) + 7) & ~7)
#define MMAP_THRESH 2048

// ~~ freep ~~
// Ponteiro pra freelist circular. Sempre aponta pro último bloco
// que visitamos (melhora performance na realocação~)
static void *freep;

// ~~ hinit ~~
// Inicializa o heap: coloca um bloco livre de HEAP_SIZE - HDR_SZ bytes
// e faz a freelist apontar pra si mesma (circular~).
// Só executa uma vez (flag _hinit~)
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
static void hinit(void) {
    if (_hinit) return;
    _hinit = 1;
    unsigned long *h = (unsigned long *)_heap;
    *h = HEAP_SIZE - HDR_SZ;
    NEXT(h) = h;
    freep = h;
}

// ~~ malloc ~~
// Aloca n bytes de memória.
// Estratégia: first-fit na freelist circular.
// Se n >= 2048 (MMAP_THRESH), usa mmap do kernel (syscall 197).
// Para alocações pequenas, percorre a freelist:
//   - Se o bloco for grande o suficiente (>= need + HDR_SZ + 16), divide
//   - Se não, usa o bloco inteiro (marca USED e remove da freelist)
// Header: 4 bytes (size | USED), retorna ponteiro após o header.
// Se não achar espaço, retorna NULL (e você chora~)
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
void *malloc(int n) {
    if (n <= 0) return 0;
    if (!_hinit) hinit();

    if (n >= MMAP_THRESH) {
        int total = n + HDR_SZ;
        total = (total + 4095) & ~4095;
        unsigned long *h = (unsigned long *)_syscall(197, 0, total, 3, 0);
        if (!h) return 0;
        *h = (unsigned long)total | USED;
        return (char*)h + HDR_SZ;
    }

    n = ALIGN8(n);
    int need = n + HDR_SZ;

    unsigned long *prev = (unsigned long *)freep;
    unsigned long *curr = (unsigned long *)NEXT(prev);

    for (;;) {
        int csize = (int)(*curr & AMASK);
        if (csize >= need) {
            if (csize >= need + HDR_SZ + 16) {
                unsigned long *new = (unsigned long *)((char*)curr + need);
                *new = (unsigned long)(csize - need);
                NEXT(new) = NEXT(curr);
                *curr = (unsigned long)need | USED;
                NEXT(prev) = new;
            } else {
                *curr |= USED;
                NEXT(prev) = NEXT(curr);
            }
            freep = prev;
            return (char*)curr + HDR_SZ;
        }
        prev = curr;
        curr = (unsigned long *)NEXT(curr);
        if (curr == freep) break;
    }
    return 0;
}

// ~~ free ~~
// Libera memória alocada por malloc/calloc/realloc.
// Se o bloco foi alocado via mmap (fora do heap), usa munmap (syscall 73).
// Se foi alocado no heap interno:
//   - Limpa o bit USED do header
//   - Insere na freelist em ordem de endereço (primeiro, inserção ordenada~)
//   - Coalesce com o próximo bloco se adjacente (união~)
//   - Coalesce com o anterior se adjacente (mais união~)
// O ponteiro `freep` é atualizado pro bloco anterior (consistência~)
/* ~~ Liberando memória~~ não esquece de pagar o aluguel! */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
void free(void *p) {
    if (!p) return;
    unsigned long *h = (unsigned long *)((char*)p - HDR_SZ);
    if ((char*)h < _heap || (char*)h >= _heap + HEAP_SIZE) {
        int total = (int)(*h & AMASK);
        _syscall(73, (long)h, total, 0, 0);
        return;
    }
    *h &= ~USED;

    unsigned long *prev = (unsigned long *)freep;
    unsigned long *curr = (unsigned long *)NEXT(prev);

    for (;;) {
        if (h > prev && h < curr) break;
        if (prev > curr && (h > prev || h < curr)) break;
        prev = curr;
        curr = (unsigned long *)NEXT(curr);
        if (prev == freep) break;
    }

    NEXT(h) = curr;
    NEXT(prev) = h;

    if ((char*)h + (int)(*h & AMASK) == (char*)curr) {
        *h = (*h & AMASK) + (*curr & AMASK);
        NEXT(h) = NEXT(curr);
    }
    if ((char*)prev + (int)(*prev & AMASK) == (char*)h) {
        *prev = (*prev & AMASK) + (*h & AMASK);
        NEXT(prev) = NEXT(h);
    }

    freep = prev;
}

// ~~ calloc ~~
// Aloca e zera memória: malloc(n * size) + memset(0).
// Útil pra arrays que precisam de inicialização garantida~ (ninguém gosta de lixo~)
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
void *calloc(int n, int size) {
    int total = n * size;
    void *p = malloc(total);
    if (p) {
        char *cp = (char *)p;
        for (int i = 0; i < total; i++) cp[i] = 0;
    }
    return p;
}

// ~~ realloc ~~
// Realoca memória: se o bloco atual já tem espaço suficiente, retorna ele.
// Senão, aloca novo bloco, copia o mínimo entre old e n, libera o antigo.
// Se p == NULL, comporta como malloc. Se n <= 0, comporta como free~ ☆
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
void *realloc(void *p, int n) {
    if (!p) return malloc(n);
    if (n <= 0) { free(p); return 0; }
    unsigned long *h = (unsigned long *)((char*)p - HDR_SZ);
    int old = (int)(*h & AMASK) - HDR_SZ;
    if (old >= n) return p;
    void *new = malloc(n);
    if (!new) return 0;
    char *sp = (char*)p;
    char *dp = (char*)new;
    int copy = old < n ? old : n;
    for (int i = 0; i < copy; i++) dp[i] = sp[i];
    free(p);
    return new;
}

// ~~ mmap ~~
// Mapeia páginas de memória via syscall 197 (TipOS mmap).
// Alinha length pra cima em múltiplo de 4096 (páginas~).
// addr, prot, flags são ignorados (sempre mapeia em endereço qualquer~)
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
void *mmap(void *addr, int length, int prot, int flags) {
    (void)addr; (void)prot; (void)flags;
    int total = (length + 4095) & ~4095;
    return (void*)_syscall(197, 0, total, 3, 0);
}

// ~~ munmap ~~
// Desmapeia páginas via syscall 73. O `length` é arredondado pra
// múltiplo de 4096 (a página mínima~)
/* ~~ munmap ~~ */
/* ~ essa demorou pra debugar, respeita ~ */
int munmap(void *addr, int length) {
    int total = (length + 4095) & ~4095;
    return (int)_syscall(73, (long)addr, total, 0, 0);
}

// ~~ exit ~~
// Termina o processo com código de saída `code`.
// Chama syscall 1 (exit). Se o syscall retornar (não deveria~),
// entra em loop com HLT porque o processo já era~ ☆
/* ~~ exit ~~ */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
void exit(int code) {
    _syscall(1, code, 0, 0, 0);
    for (;;) __asm__("hlt");
}




/* ♥ stdlib.c ~ feito com carinho (e uma raiva controlada) ~ kyun! */
