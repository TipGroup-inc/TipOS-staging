💾 **ARMAZENAMENTO — TipOS**

**Driver ATA PIO — já implementado** (`OvsbMkM/src/drivers/ata.c`):
- Portas `0x1F0–0x1F7` (primary IDE, master)
- **LBA28**, setor de 512 bytes
- `ata_read_sector(uint32_t lba, uint8_t *buf)` / `ata_write_sector(...)` — espera DRQ, lê/escreve 256 words
- `ata_init()` — espera 1s (busy loop), seleciona drive 0

> ⚠️ Curiosidade: GCC 15+ gera `inw %edx` em vez de `inw %dx`. O Makefile já corrige isso via `sed` no assembly gerado.

---

**FAT32 — completo e funcional** (`OvsbMkM/src/fs/fat32.c`, 748 linhas):

| Função | Descrição |
|---|---|
| `fat32_read_file` / `fat32_write_file` | Ler / escrever (sobrescreve) |
| `fat32_create_file` / `fat32_delete_file` | Criar / remover |
| `fat32_list_dir` | Lista diretório na VGA |
| `fat32_change_dir` / `fat32_get_cwd_name` | Navegação |
| `fat32_mkdir` / `fat32_rmdir` | Diretórios |
| `fat32_rename` | Renomear |
| `fat32_stat` | Info de arquivo |

Nomes convertidos automaticamente pra **8.3** (`name_to_83`): `hello.c` → `HELLO   C`, `file.txt.bak` → `FILE~1  BAK`.

Estrutura no disco (imagem de 64MB):
```
/BIN/       → binários Mach-O
/USR/BIN/   → PATH secundário (futuro)
/LOCAL/BIN/ → PATH terciário
```

---

**Em aberto:**
- Driver **AHCI** (SATA) — futuro
- Driver **NVMe** — futuro
- **Ext2** — futuro, alternativa ao FAT32 avaliada e adiada (`tipos-dev-stack.md`: "FAT32 é mais fácil de implementar")
- **VFS layer** real (`mount`, `umount`, `fstab`, `tmpfs`, `devfs`, `procfs`) — hoje o acesso a arquivo é direto via FAT32, sem camada de abstração

Bugs de leitura/escrita em disco, reporta aqui com log serial (ver #logs).
