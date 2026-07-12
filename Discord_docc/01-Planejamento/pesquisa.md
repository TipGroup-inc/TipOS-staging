🔬 **PESQUISA & REFERÊNCIAS — TipOS**

Material de estudo pra consultar antes de implementar qualquer feature nova. (Fonte: `tipos-dev-stack.md`, seção 7)

---

**📚 Docs essenciais**
- OSDev Wiki (bíblia do dev de SO) — https://wiki.osdev.org
- Intel SDM (manual da CPU, vol. 2A) — https://www.intel.com/content/www/us/en/architecture-and-technology/64-ia-32-architectures-software-developer-vol-2a-manual.html
- Mach IPC spec (CMU original) — https://www.cs.cit.tum.de/fileadmin/w00cfhd/papers/1991-mach-ipc.pdf
- lwIP (stack TCP/IP leve) — https://savannah.nongnu.org/projects/lwip/
- QEMU + GDB debug — https://wiki.osdev.org/Kernel_Debugging

**🧭 SOs de referência**
- Limine bootloader (moderno, UEFI + BIOS) — https://github.com/limine-bootloader/limine
- Redox OS (SO em Rust) — https://doc.redox-os.org
- ToaruOS (microkernel educacional em C) — https://github.com/klange/toaruos
- seL4 (microkernel verificado formalmente) — https://sel4.systems

---

**🔧 Toolchain / cross-compiler**
Target `x86_64-elf`: binutils 2.42+, gcc 14+ (só C, sem libc), nasm 2.16+, `grub-mkrescue`.
Receita de build completa em `tipos-dev-stack.md`, seção 2.

---

**📌 Tópicos que ainda precisam de pesquisa aprofundada:**
- Relocations Mach-O + `dyld` mínimo (ver limitações no README do OvsbMkM)
- Paginação por processo (hoje é single-address-space, sem isolamento)
- Copy-on-write pra `fork`
- ELF loader básico (cabeçalhos, segmentos, entry point)
- Driver AHCI/NVMe (hoje só ATA PIO legado)
- Mapeamento de ~150 syscalls Linux → nativas (compat layer)

Achou algo relevante? Posta aqui com uma linha explicando por que é útil pro TipOS.
