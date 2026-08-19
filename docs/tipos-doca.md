<!-- moe moe kyun <3 -->
> **🕰️ HISTÓRICO — este documento descreve o passado do projeto**
> O código atual mudou bastante: `OvsbMkM` virou `OvsbMk`, `src/dock`, `src/rust` e o
> `src/userland/disp` foram removidos, e o kernel ganhou Zig + ELF64 (musl) compat.
> Para a verdade de hoje: leia `README.md`, `AGENTS.md` e `KERNEL.md`.

---
# TipOS — Arquitetura da Doca (Dock HAL)

## 1. Conceito

A **Doca** é uma camada HAL que permite ao kernel TipOS usar drivers Linux
sem violar a licença MIT. Funciona como um tradutor ABI: o driver acha que
está rodando no Linux, mas está acoplado ao TipOS via uma interface estável.

### 1.1 Estratégia de Licenciamento (MIT + GPL)

O problema: Linux é GPLv2, drivers Linux derivam do kernel e são GPL.
MIT não pode incorporar código GPL.

A solução é uma abordagem em **camadas com barreiras legais**:

```
 TipOS (MIT)
   │
   ├── kernel TipOS (MIT) — código original
   ├── dock/ (MIT) — API de compatibilidade, independente
   │     • NENHUMA linha copiada do Linux
   │     • ABI implementada por engenharia reversa limpa
   │     • Baseada em especificações públicas (osdev, datasheets)
   │
   └── drivers/ — carregados em runtime pelo usuário
         • Usuário obtém o .ko da sua distribuição Linux (GPL)
         • TipOS NÃO distribui nem inclui os drivers
         • Mesmo modelo do ndiswrapper (carrega .sys Windows)
         • O driver roda na Doca, que é MIT — ele "acha" que é Linux
```

**Regras para manter compatibilidade MIT:**
1. A Doca implementa uma interface funcionalmente equivalente à ABI do Linux
2. Nunca olhamos código-fonte do Linux durante a implementação
3. Toda a implementação é baseada em: documentação pública (OSDev, Intel Manuals),
   datasheets de hardware, especificações ABI (SysV ABI, ELF), e engenharia reversa
   de comportamento observável (não do código)
4. Arquivos de header (`linux/*.h`) são reimplementados — nomes e structs seguem
   a mesma semântica (funcionalidade), mas implementação é original
5. O pacto de driver é carregado em runtime, não linkado estaticamente

---

## 2. Estrutura de Diretórios

```
src/
├── kernel/                  # Núcleo do TipOS
│   ├── core/                # Scheduler, IPC, VFS, syscalls nativas
│   │   ├── scheduler.c
│   │   ├── ipc.c
│   │   ├── vfs.c
│   │   └── syscall.c
│   ├── mem/                 # Alocador físico + virtual
│   │   ├── pmm.c            # Physical Memory Manager (bitmap)
│   │   ├── vmm.c            # Virtual Memory Manager (page tables)
│   │   └── kmalloc.c        # Heap alocador (slab)
│   ├── arch/                # x86_64
│   │   ├── boot64.asm       # Multiboot2 + long mode
│   │   ├── gdt.c
│   │   ├── idt.c
│   │   ├── pic.c
│   │   └── linker.ld
│   └── init/                # Inicialização
│       └── kmain.c
│
├── dock/                    # Camada Doca — TODO EM MIT
│   ├── include/             # Headers públicos da Doca (API estável)
│   │   ├── dock.h           # Macros e tipos base
│   │   ├── abi_stable.h     # ABI canônica do TipOS para drivers
│   │   ├── api_pci.h
│   │   ├── api_block.h
│   │   ├── api_net.h
│   │   ├── api_input.h
│   │   ├── api_video.h
│   │   └── api_usb.h
│   │
│   ├── core/                # Núcleo da Doca
│   │   ├── registry.c       # Registro e descoberta de drivers
│   │   ├── loader.c         # Carregador de ELF64 (módulos .ko)
│   │   ├── manifest.c       # Parser de manifesto (JSON)
│   │   └── lifecycle.c      # init, fini, reset do driver
│   │
│   ├── abi/                 # Tradutores ABI Linux → TipOS
│   │   ├── resolver.c       # Tabela de símbolos genérica
│   │   ├── linux_5x.c       # Shim para ABI Linux 5.x
│   │   ├── linux_6x.c       # Shim para ABI Linux 6.x
│   │   └── linux_common.c   # Funções compartilhadas entre versões
│   │
│   ├── api/                 # Implementação das APIs de hardware
│   │   ├── api_pci.c        # Enumeração PCI, BAR, IRQ
│   │   ├── api_block.c      # Blocos (discos, SSDs)
│   │   ├── api_net.c        # Rede (netif_rx, sk_buff)
│   │   ├── api_input.c      # Teclado, mouse, touch
│   │   ├── api_video.c      # Framebuffer (vesa, drm compat)
│   │   └── api_usb.c        # USB (UHCI, EHCI, XHCI)
│   │
│   ├── vfs/                 # Ponte para VFS do TipOS
│   │   ├── file_ops.c       # read/write/ioctl do driver → syscall TipOS
│   │   └── devtmpfs.c       # /dev/ virtual (device nodes)
│   │
│   └── utils/               # Utilitários internos
│       ├── buffer.c         # Pool de buffers compartilhados (sk_buff)
│       ├── list.c           # Listas encadeadas (list_head)
│       ├── workqueue.c      # Workqueue para drivers diferidos
│       ├── timer.c          # Timers (timer_list)
│       └── log.c            # Log estruturado (printk → dock_log)
│
├── drivers/                 # Pacotes de driver (carregados em runtime)
│   ├── rtl8139/             # Placa de rede Realtek
│   │   ├── pkg.json
│   │   └── rtl8139.ko       # Binário obtido pelo usuário
│   ├── ahci/                # SATA
│   │   ├── pkg.json
│   │   └── ahci.ko
│   ├── e1000/               # Intel Gigabit Ethernet
│   │   ├── pkg.json
│   │   └── e1000.ko
│   ├── i915/                # Intel Graphics (futuro, complexo)
│   │   ├── pkg.json
│   │   └── i915.ko
│   └── ps2/                 # Driver nativo (fallback, MIT)
│       └── pkg.json
│
└── lib/                     # Bibliotecas compartilhadas
    └── libtipos.so           # syscall wrapper para usermode
```

---

## 3. ABI Canônica (abi_stable.h)

Toda função que um driver Linux pode chamar é mapeada para a ABI estável
do TipOS. A Doca resolve os símbolos em tempo de carga do .ko.

### 3.1 Memória

```c
// Alocação de heap — kmalloc/kfree
void *dock_alloc(size_t size, int flags);
void  dock_free(void *ptr);

// Mapeamento de MMIO — ioremap/iounmap
void *dock_ioremap(phys_addr_t phys, size_t size);
void  dock_iounmap(void *virt);

// DMA
void *dock_dma_alloc(size_t size, dma_addr_t *dma_handle);
void  dock_dma_free(void *cpu_addr, dma_addr_t dma_handle, size_t size);
int   dock_dma_set_mask(struct device *dev, u64 mask);

// Pool de páginas
struct page *dock_alloc_page(gfp_t flags);
void         dock_free_page(struct page *page);
```

### 3.2 Sincronização

```c
// Spinlocks (com e sem IRQ save)
void dock_spin_lock(      spinlock_t *lock);
void dock_spin_unlock(    spinlock_t *lock);
void dock_spin_lock_irq(  spinlock_t *lock);
void dock_spin_unlock_irq(spinlock_t *lock);
unsigned long dock_spin_lock_irqsave(  spinlock_t *lock);
void          dock_spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags);

// Mutexes (sleepable)
void dock_mutex_lock(  mutex_t *mx);
void dock_mutex_unlock(mutex_t *mx);

// Referência counting
void dock_ref_get(struct kref *ref);
int  dock_ref_put(struct kref *ref);
```

### 3.3 PCI

```c
// Config space
u8   dock_pci_read8( struct pci_dev *dev, int offset);
u16  dock_pci_read16(struct pci_dev *dev, int offset);
u32  dock_pci_read32(struct pci_dev *dev, int offset);
void dock_pci_write8( struct pci_dev *dev, int offset, u8 val);
void dock_pci_write16(struct pci_dev *dev, int offset, u16 val);
void dock_pci_write32(struct pci_dev *dev, int offset, u32 val);

// BARs
phys_addr_t dock_pci_resource_start(struct pci_dev *dev, int bar);
size_t      dock_pci_resource_len(   struct pci_dev *dev, int bar);
int         dock_pci_request_region( struct pci_dev *dev, int bar, const char *name);
void        dock_pci_release_region( struct pci_dev *dev, int bar);

// Device lifecycle
int  dock_pci_enable_device(struct pci_dev *dev);
void dock_pci_disable_device(struct pci_dev *dev);
int  dock_pci_set_dma_mask(struct pci_dev *dev, u64 mask);

// IRQ
int  dock_request_irq(unsigned int irq, irq_handler_t handler,
                      unsigned long flags, const char *name, void *dev);
void dock_free_irq(unsigned int irq, void *dev);

// MSI-X
int  dock_pci_enable_msix(struct pci_dev *dev, struct msix_entry *entries, int nvec);
void dock_pci_disable_msix(struct pci_dev *dev);
```

### 3.4 Rede

```c
// sk_buff
struct sk_buff *dock_alloc_skb(unsigned int len);
void            dock_kfree_skb(struct sk_buff *skb);
unsigned char  *dock_skb_put(struct sk_buff *skb, unsigned int len);

// Net device ops
int  dock_register_netdev(struct net_device *ndev);
void dock_unregister_netdev(struct net_device *ndev);
int  dock_netif_rx(struct sk_buff *skb);
void dock_netif_carrier_on(struct net_device *ndev);
void dock_netif_carrier_off(struct net_device *ndev);
void dock_netif_wake_queue(struct net_device *ndev);
void dock_netif_stop_queue(struct net_device *ndev);

// Ethtool (stub para compatibilidade)
int dock_ethtool_get_link_ksettings(struct net_device *ndev,
                                    struct ethtool_link_ksettings *cmd);
```

### 3.5 Bloco

```c
// Request queue
struct request_queue *dock_blk_init_queue(request_fn_proc *rfn, spinlock_t *lock);
void                  dock_blk_cleanup_queue(struct request_queue *q);
struct request       *dock_blk_fetch_request(struct request_queue *q);
void                  dock_blk_end_request(struct request *rq, int error,
                                           unsigned int nr_bytes);
void                  dock_blk_end_request_all(struct request *rq, int error);

// Gendisk
struct gendisk *dock_alloc_disk(int minors);
void            dock_add_disk(struct gendisk *gd);
void            dock_del_gendisk(struct gendisk *gd);
void            dock_put_disk(struct gendisk *gd);

// Bio (camada abaixo do request)
struct bio *dock_bio_alloc(gfp_t gfp_mask, int nr_iovecs);
void        dock_bio_free(struct bio *bio);
int         dock_bio_add_page(struct bio *bio, struct page *page,
                              unsigned int len, unsigned int offset);
```

### 3.6 Input

```c
struct input_dev *dock_input_allocate_device(void);
void              dock_input_free_device(struct input_dev *dev);
int               dock_input_register_device(struct input_dev *dev);
void              dock_input_unregister_device(struct input_dev *dev);
void              dock_input_report_key(struct input_dev *dev,
                                        unsigned int code, int value);
void              dock_input_report_abs(struct input_dev *dev,
                                        unsigned int code, int value);
void              dock_input_sync(struct input_dev *dev);
void              dock_set_bit(unsigned long *addr, unsigned int bit);
```

### 3.7 Video (Framebuffer / DRM)

```c
// FB
struct fb_info *dock_framebuffer_alloc(size_t size, struct device *dev);
int             dock_register_framebuffer(struct fb_info *info);
int             dock_unregister_framebuffer(struct fb_info *info);
void           *dock_fb_get_vaddr(struct fb_info *info);

// DRM (stub para compatibilidade inicial)
int  dock_drm_dev_register(struct drm_device *dev, unsigned long flags);
void dock_drm_dev_unregister(struct drm_device *dev);
```

### 3.8 USB

```c
// URB
struct urb *dock_usb_alloc_urb(int iso_packets, gfp_t mem_flags);
void        dock_usb_free_urb(struct urb *urb);
int         dock_usb_submit_urb(struct urb *urb, gfp_t mem_flags);
int         dock_usb_unlink_urb(struct urb *urb);

// Device
int  dock_usb_register_dev(struct usb_driver *driver);
void dock_usb_deregister_dev(struct usb_driver *driver);
int  dock_usb_control_msg(struct usb_device *dev, unsigned int pipe,
                          __u8 request, __u8 requesttype,
                          __u16 value, __u16 index,
                          void *data, __u16 size, int timeout);
```

### 3.9 Utilidades

```c
// Log
int  dock_printk(const char *fmt, ...);   // printk → dock_log
void dock_log_dump(const char *module);   // despeja ring buffer

// Timer
void dock_init_timer(struct timer_list *t);
void dock_add_timer(struct timer_list *t);
int  dock_del_timer(struct timer_list *t);

// Workqueue
struct workqueue_struct *dock_create_workqueue(const char *name);
void                     dock_destroy_workqueue(struct workqueue_struct *wq);
int                      dock_schedule_work(struct work_struct *work);

// Listas
void dock_INIT_LIST_HEAD(struct list_head *list);
void dock_list_add(struct list_head *new, struct list_head *head);
void dock_list_del(struct list_head *entry);
int  dock_list_empty(const struct list_head *head);

// IDR (alocação de IDs)
void dock_idr_init(struct idr *idp);
int  dock_idr_alloc(struct idr *idp, void *ptr, int start, int end, gfp_t gfp);
void *dock_idr_find(struct idr *idp, int id);
void dock_idr_remove(struct idr *idp, int id);

// Device model
int  dock_device_register(struct device *dev);
void dock_device_unregister(struct device *dev);
int  dock_driver_register(struct device_driver *drv);
void dock_driver_unregister(struct device_driver *drv);
```

---

## 4. Formato do Manifesto (pkg.json)

Cada driver é um diretório com manifesto + binário ELF `.ko`:

```json
{
  "name": "rtl8139",
  "version": "1.0.0",
  "type": "net",
  "abi": "linux_5x",
  "abi_version": "5.15.0",
  "entry": "init_module",
  "fini": "cleanup_module",
  "license": "GPL",
  "author": "Realtek Semiconductor Corp.",
  "deps": [],

  "pci": [
    { "vendor": 0x10ec, "device": 0x8139,
      "subvendor": 0xffff, "subdevice": 0xffff,
      "class": 0x020000 }
  ],

  "heap_max": 65536,
  "arena_pages": 4,
  "irq_count": 1,
  "io_ports": [
    { "start": 0xBF00, "end": 0xBF1F }
  ],

  "mmio_areas": [
    { "bar": 1, "size": 256 }
  ],

  "need_dma": false,
  "need_timer": true,
  "need_workqueue": false
}
```

Campos:
- `abi`: qual tradutor ABI usar (`linux_5x`, `linux_6x`)
- `abi_version`: versão específica para resolução fina de símbolos
- `entry`/`fini`: símbolos de entrada/saída do módulo
- `pci`: devices que o driver atende (vendor/device match)
- `heap_max`: limite de alocação de heap do driver
- `arena_pages`: quantas páginas (4 KB) reservar para o sandbox
- `irq_count`: quantas IRQs o driver vai registrar
- `io_ports`: portas de I/O que o driver vai usar
- `mmio_areas`: áreas MMIO (BARs) que o driver vai mapear
- `need_dma`, `need_timer`, `need_workqueue`: flags de recursos

---

## 5. Carregador ELF (loader.c)

O carregador mapeia o `.ko` em uma **arena de memória isolada**:

```
                   ┌─────────────────────────────┐
                   │   Arena do Driver (4 páginas) │
                   ├─────────────────────────────┤
                   │  .text (código do driver)    │  ← mapeado RX
                   │  .rodata (strings, tables)   │  ← mapeado R
                   │  .data (variáveis globais)   │  ← mapeado RW
                   │  .bss  (zero-inicializado)   │  ← mapeado RW
                   │  Heap  (kmalloc)              │  ← mapeado RW
                   │  Stack (pilha de 4 KB)        │  ← mapeado RW
                   │  MMIO  (BARs mapeados)        │  ← mapeado segundo o
                   │         (via dock_ioremap)    │     manifesto
                   └─────────────────────────────┘
```

Passos:
1. Aloca `arena_pages` páginas contíguas
2. Carrega segmentos ELF (`PT_LOAD`) nos offsets corretos
3. Resolve símbolos não-definidos contra a tabela do tradutor ABI
4. Aplica relocações (`R_X86_64_RELATIVE`, `R_X86_64_GLOB_DAT`, etc.)
5. Mapeia as áreas de MMIO/IO solicitadas no manifesto
6. Chama `init_module()`

---

## 6. Sandbox de Memória

Cada driver roda em isolamento parcial:

| Recurso | Isolamento |
|---|---|
| **Heap** | Arena própria, `kmalloc` só aloca dentro dela |
| **Stack** | Pilha separada de 4 KB, guard page abaixo |
| **Páginação** | Arena mapeada em endereços fixos, outras arenas invisíveis |
| **IRQs** | Só pode registrar IRQs declaradas no manifesto |
| **MMIO** | Só mapeia BARs que pediu no manifesto |
| **I/O ports** | Só acessa portas declaradas (filtro via bitmap de I/O) |
| **Syscalls** | Só enxerga funções do tradutor ABI, não do kernel TipOS |
| **Timeout** | Se `init_module()` não retornar em 5s, mata o driver |

**Proteção por bitmap de I/O** (x86 TSS):
- O campo `I/O Bitmap Base` no TSS aponta para um bitmap de 65536 bits
- Cada porta de I/O tem um bit: 0 = permitida, 1 = armadilha (#GP)
- O manifesto declara as portas, a Doca seta os bits correspondentes
- Qualquer `in`/`out` não autorizado gera GP fault e o driver é desativado

---

## 7. Ciclo de Vida

```
        dock_scan()
            │
            ▼
      ┌─────────────┐
      │  UNLOADED   │
      └──────┬──────┘
             │ dock_load()
             ▼
      ┌─────────────┐
      │   LOADED    │  ← ELF carregado, símbolos resolvidos
      └──────┬──────┘
             │ init_module()
             ▼
      ┌─────────────┐
      │    INIT     │  ← executando init_module()
      └──────┬──────┘
     ┌───────┴───────┐
     ▼               ▼
┌──────────┐  ┌──────────┐
│ RUNNING  │  │  ERROR   │  ← init falhou ou timeout
└──────┬───┘  └──────────┘
       │ dock_unload()
       ▼
  ┌──────────┐
  │ STOPPING │  ← chamando cleanup_module()
  └──────┬───┘
         ▼
   ┌──────────┐
   │ UNLOADED │  ← arena liberada
   └──────────┘
```

---

## 8. Resolvedor de Símbolos (resolver.c)

O resolvedor liga as chamadas do driver às funções da Doca:

```c
// Tabela de símbolos do tradutor ABI
typedef struct {
    const char *name;    // "kmalloc"
    void       *addr;    // &dock_alloc
} dock_sym_t;

// Exemplo: tabela para linux_5x
dock_sym_t linux_5x_syms[] = {
    { "kmalloc",              dock_alloc },
    { "kfree",                dock_free },
    { "printk",               dock_printk },
    { "ioremap",              dock_ioremap },
    { "iounmap",              dock_iounmap },
    { "spin_lock",            dock_spin_lock },
    { "spin_unlock",          dock_spin_unlock },
    { "request_irq",          dock_request_irq },
    { "free_irq",             dock_free_irq },
    { "pci_read_config_byte", dock_pci_read8 },
    // ... (~600+ símbolos para cobertura completa)
    { NULL, NULL }  // sentinela
};
```

Processo de resolução:
1. Pega cada símbolo não-definido do `.ko` (da tabela `.symtab`)
2. Busca na tabela do tradutor ABI (`linux_5x_syms[]`)
3. Se achar: substitui o endereço pelo da Doca
4. Se não achar: procura em outros drivers carregados (dependências)
5. Se não achar em lugar nenhum: retorna erro, driver não carrega

Acobertamento mínimo para drivers reais (PCI+net): **~150 símbolos**.
Cobertura completa para a maioria dos drivers: **~600 símbolos**.

---

## 9. Plano de Implementação (MIT, sem GPL)

### Fase 0 — Prova de Conceito (1-2 semanas)

**Objetivo:** Carregar um driver `.ko` real e chamar `init_module()`.

- [ ] Carregador ELF64 mínimo (loader.c)
- [ ] Arena de memória com alocação de páginas
- [ ] Resolvedor de símbolos simples (tabela fixa)
- [ ] 10 símbolos: `kmalloc`, `kfree`, `printk`, `memset`, `memcpy`, `strlen`, `strcmp`, `sprintf`, `sscanf`, `module_put`
- [ ] Manifesto manual (sem parser JSON, struct hard-coded)
- [ ] Teste com `hello.ko` compilado manualmente (só printk)
- [ ] Teste com `rtl8139.ko` real — ver se init_module() roda sem crash

### Fase 1 — PCI Enumeração (2-3 semanas)

**Objetivo:** Driver PCI consegue ver o hardware.

- [ ] Enumeração PCI via scanning de bus (0xCF8/0xCFC)
- [ ] `dock_pci_read/write*` — config space
- [ ] `dock_pci_enable_device` — memory/IO enable no comando register
- [ ] `dock_pci_request_region` — verifica BARs, marca como usados
- [ ] `dock_pci_resource_start/len` — retorna endereços dos BARs
- [ ] `dock_ioremap/iounmap` — mapeia BARs de memória
- [ ] IRQ: `dock_request_irq` + `dock_free_irq` (PIC → IOAPIC)
- [ ] MSI-X: `dock_pci_enable/disable_msix`
- [ ] ~80 símbolos (PCI + genéricos)
- [ ] Teste: rtl8139 init_module() → vê PCI device → retorna 0

### Fase 2 — Rede (3-4 semanas)

**Objetivo:** Driver de rede envia e recebe pacotes.

- [ ] `dock_alloc_skb`, `dock_kfree_skb`, `dock_skb_put`
- [ ] `dock_register_netdev` + ops (open, stop, start_xmit, get_stats)
- [ ] `dock_netif_rx` — recebe pacote do driver → fila de entrada
- [ ] `dock_netif_wake_queue/stop_queue` — controle de fluxo
- [ ] Pilha de rede mínima (ARP + IP + UDP)
- [ ] Loopback: pacotes transmitidos aparecem como recebidos
- [ ] ~150 símbolos (rede + PCI + util)
- [ ] Teste: rtl8139 transmite frame, loopback recebe

### Fase 3 — Bloco + FS (3-4 semanas)

**Objetivo:** Driver ATA/SATA lê e escreve discos.

- [ ] `dock_blk_init_queue` + request handling
- [ ] `dock_alloc_disk` + `dock_add_disk`
- [ ] DMA: `dock_dma_alloc`, `dock_dma_free`
- [ ] `dock_bio_alloc/bio_add_page/bio_submit`
- [ ] Timer: `dock_init_timer/add_timer/del_timer`
- [ ] Workqueue: `dock_create_workqueue`, `dock_schedule_work`
- [ ] ~200 símbolos (bloco + PCI + DMA + timer)
- [ ] Teste: driver AHCI detecta disco, lê setor 0

### Fase 4 — Input + USB (4-6 semanas)

**Objetivo:** Teclado, mouse, dispositivos USB.

- [ ] `dock_input_allocate_device/register_device/report_key/sync`
- [ ] Teste: driver USB HID detecta teclado/mouse
- [ ] URB: `dock_usb_alloc_urb/submit_urb/control_msg`
- [ ] `dock_usb_register_dev` — binding driver↔device
- [ ] ~250 símbolos (input + USB + PCI)
- [ ] Teste: pendrive é detectado, lê MBR

### Fase 5 — Video (6-8 semanas)

**Objetivo:** Driver gráfico (simples: VESA, complexo: i915).

- [ ] `dock_framebuffer_alloc/register_framebuffer`
- [ ] VESA framebuffer nativo
- [ ] `dock_drm_dev_register` (stub — aceita, cria /dev/dri)
- [ ] Teste: driver simpledrm (kernel 6.x) mostra framebuffer
- [ ] Teste: driver bochs (qemu -vga std) modo gráfico
- [ ] ~400 símbolos (DRM + FB + PCI + DMA + IRQ)
- [ ] i915: complexidade extra (GTT, power management, display)

### Fase 6 — Amadurecimento

- [ ] Parser JSON real para manifesto
- [ ] Bitmap de I/O por driver (proteção de portas)
- [ ] Assinatura de manifesto (SHA-256)
- [ ] Carregamento em runtime (hotplug: pendrive → carrega driver USB)
- [ ] Reload de driver após erro
- [ ] Ferramenta de usuário: `tipos-dock load/unload/list`
- [ ] Documentação de criação de manifesto
- [ ] Cobertura de ~600 símbolos para compatibilidade ampla

---

## 10. Cobertura de Símbolos por Categoria

| Categoria | Símbolos | Prioridade |
|---|---|---|
| Memória (kmalloc, ioremap, DMA) | ~40 | Fase 0 |
| Strings/printk/varargs | ~30 | Fase 0 |
| Spinlocks/mutexes/semaforos | ~25 | Fase 1 |
| PCI config/IRQ/MSI | ~60 | Fase 1 |
| Dispositivo/barramento | ~35 | Fase 1 |
| sk_buff/Net device | ~80 | Fase 2 |
| Net headers/csum/tso/gso | ~40 | Fase 2 |
| Block/request/bio/gendisk | ~60 | Fase 3 |
| DMA mapping/API | ~25 | Fase 3 |
| Timer/workqueue | ~20 | Fase 3 |
| Input subsystem | ~30 | Fase 4 |
| USB core/URB/HID | ~100 | Fase 4 |
| FB/DRM | ~60 | Fase 5 |
| **Total** | **~605** | |

---

## 11. Diferenças da Abordagem Tradicional

| Característica | Linux (GPL) | ndiswrapper | TipOS Doca (MIT) |
|---|---|---|---|
| **Licença** | GPLv2 | GPLv2 | **MIT** |
| **Drivers** | Nativos do kernel | Windows .sys (proprietário) | Linux .ko (GPL, carregado em runtime) |
| **Distribuição** | Inclui drivers | Usuário obtém .sys | **Usuário obtém .ko** (não distribuímos) |
| **Sandbox** | Nenhum | Nenhum | **Arena isolada + bitmap I/O** |
| **Manifesto** | Kconfig/Modinfo | Nenhum | **pkg.json com recursos explícitos** |
| **ABI** | Instável (muda entre versões) | Única (Windows NDIS) | **Múltiplos shims versionados** |
| **Tamanho da Doca** | N/A (kernel inteiro) | ~50 KB | **Estimado ~100-200 KB** |

---

## 12. Riscos e Mitigações

| Risco | Mitigação |
|---|---|
| ** Driver crasha ring 0** | Arena isolada + timeout + guard page na pilha |
| **Driver acessa I/O não autorizado** | Bitmap de I/O por TSS → #GP → Doca desativa |
| **Símbolo faltando na Doca** | Log claro: "símbolo X não encontrado"; manifesto documenta requisitos |
| **ABI Linux muda** | Criar novo shim (ex: linux_6x.c) sem quebrar shims antigos |
| **Licença: GPL → MIT** | Doca é clean room; driver .ko é fornecido pelo usuário; mesmo modelo do ndiswrapper (já testado em tribunal) |
| **Performance (tradução)** | Overhead de uma chamada indireta por símbolo (~5 ns) — insignificante |
| **Manutenção de 600 símbolos** | Foco em ~150 para drivers PCI+net; resto cresce sob demanda |
```
