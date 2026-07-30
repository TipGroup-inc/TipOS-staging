/* moe moe kyun <3 */
/* moe moe kyun <3 */
📁 **VFS — TipOS**

**Status atual: não existe como camada separada.** Hoje o acesso a arquivo é **direto ao FAT32** — `fat32_read_file`, `fat32_write_file`, etc. são chamadas diretamente pelas syscalls (`open`, `read`, `write`, `stat`...) sem nenhuma abstração de VFS no meio.

Isso funciona bem pra um único filesystem, mas trava qualquer plano de múltiplas partições, `tmpfs`, `devfs` ou `procfs`.

---

**Plano (Fase 3 do roadmap em equipe, junto com #armazenamento):**
- Virtual File System layer real
- Mount table: `/dev/sda1 → /`, `/dev/sda2 → /home`
- Comandos `mount` / `umount`
- `fstab` (montagens automáticas no boot)
- `tmpfs` (`/tmp` em RAM), `devfs` (`/dev/`), `procfs` (`/proc/[pid]/`) — este último depende de #processos existir primeiro

**Referência de API atual do FAT32** (o que o VFS vai precisar abstrair):
```
fat32_read_file / fat32_write_file / fat32_create_file / fat32_delete_file
fat32_list_dir / fat32_change_dir / fat32_get_cwd_name
fat32_mkdir / fat32_rmdir / fat32_rename / fat32_stat
```

Se alguém tem experiência com VFS de outros SOs (Linux VFS, ToaruOS, etc.), ajuda a validar se vale a pena replicar esse modelo ou simplificar pro nosso caso (ainda single-address-space, sem múltiplos processos).
