<!-- moe moe kyun <3 -->
🧠 **MEMÓRIA — TipOS**

**Arquivo:** `OvsbMkM/src/kernel/memory.c` (83 linhas)

**Layout atual:**
```
0x00200000  Page tables (PML4, PDP, PD)
0x00900000  Stack do kernel
0x00800000–0xC00000  Heap do kernel (bump allocator, 4MB)
0x02000000  Userland programs (slide base)
```

**Já implementado:**
- `kmalloc()` — bump allocator, O(1), **sem `kfree()` real** (não reusa memória)
- `kfree()` — só libera se foi alocado via page allocator (`mmap_user`)
- Page allocator: bitmap de 1024 páginas de 4096 bytes
- `mmap_user` / `munmap_user` — syscalls 197/73, alocam/liberam páginas anônimas

**Userland `malloc()`:** bump allocator de 64KB heap estático próprio (libc), `free()` é no-op — igual ao do kernel.

---

**O que falta (mesma ordem do roadmap):**
- Paginação **por processo** (hoje é single-address-space — todo mundo enxerga a mesma memória)
- Transição ring 0 → ring 3 (TSS)
- Copy-on-write pro `fork`
- TLB flush correto na troca de contexto
- Isolar o kernel em página alta (hoje kernel e userland dividem o mesmo range)
- Substituir bump allocator do kernel por um allocator real (free list) — hoje `kfree()` não recicla nada

**Referência de mudança planejada:** `memory.c` → 70% aproveitável, vai virar `kernel/mm/pmm.c` (físico) + novo `kernel/mm/vmm.c` (virtual, identidade → demanda).

Quem já mexeu com CoW ou paginação por processo, bora trocar ideia de design aqui antes de sair codando.
