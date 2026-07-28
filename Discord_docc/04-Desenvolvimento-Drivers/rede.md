/* moe moe kyun <3 */
🌐 **REDE — TipOS**

**Status atual: não existe.** Nenhum driver de rede implementado ainda. Feature de fase avançada (Fase 4 do roadmap em equipe, 4 semanas estimadas).

---

**Plano:**
- Driver **Intel e1000** (funciona bem em QEMU, boa pra desenvolvimento)
- Driver **RTL8139** (comum em hardware real mais antigo)
- **lwIP** portado como servidor **user-space** (decisão em `tipos-dev-stack.md` — evita reimplementar TCP/IP do zero; alternativa "stack própria" foi descartada por ser trabalho demais)
- Socket syscalls: `socket`, `bind`, `listen`, `connect`, `send`, `recv`
- DHCP client, DNS resolver (`/etc/resolv.conf`), `ping`, `wget`/`curl`

**Referência da arquitetura como serviço isolado:** ver também #netstack (pasta Serviços de Sistema) — a ideia é que a stack de rede rode como processo servidor separado, se comunicando com o kernel via IPC (Mach), não embutida no kernel.

---

**Pré-requisitos bloqueantes:**
- IPC funcionando (#ipc) — pro driver rodar em user-space e conversar com o kernel
- PCI enumeration (Fase 1 do Doca/HAL, ver #ideias) — pra descobrir e inicializar a placa de rede

Quem curte redes/protocolos, esse é o canal pra ajudar a desenhar a stack antes de qualquer linha de driver ser escrita.
