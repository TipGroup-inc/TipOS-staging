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

**Userland `malloc()`:** freelist circular com coalescência (stdlib.c), `mmap` para alocações >= 2048 bytes.

---

**O que já foi implementado recentemente:**
- **Paginação por processo** — cada execução ring 3 tem PML4 próprio (clone da identidade do kernel), switch cr3 na entry/exit
- **clone_identity_tables()** — clona PML4/PDP/PD mas **strips o bit U/S** de todas as entradas; spawn paths re-adicionam U/S para code/stack pages
- **TLB flush implícito** no `mov cr3` na troca de contexto
- **Transição ring 0 → ring 3** via TSS + iretq (CS=0x1B/RPL=3)
- **Bugfix `mapped[32]`** — VA|PA OR corrompia phys addr no ELF loader; fix `(va>>32)<<32 | phys`

**O que falta:**
- Isolar o kernel em página alta (0xFFFF800000000000+)
- Substituir bump allocator do kernel por um allocator real (free list)
- Copy-on-write pro `fork`

**Referência de mudança planejada:** `memory.c` → 70% aproveitável, vai virar `kernel/mm/pmm.c` (físico) + novo `kernel/mm/vmm.c` (virtual, identidade → demanda).

Quem já mexeu com CoW ou paginação por processo, bora trocar ideia de design aqui antes de sair codando.
