<!-- moe moe kyun <3 -->
🌐 **NETSTACK — TipOS**

**Status atual: não existe.** Companion do canal #rede (pasta Drivers) — aqui é a discussão de arquitetura da stack **como serviço isolado**, lá é sobre os drivers de placa de rede em si.

**Decisão já tomada (`tipos-dev-stack.md`):**
> lwIP portado como servidor **user-space** — não embutido no kernel.

**Escopo planejado:**
- Pilha TCP/IP via lwIP rodando como processo servidor separado
- Loopback (`127.0.0.1`)
- Socket API exposta pra apps via syscalls (`socket`, `bind`, `listen`, `connect`, `send`, `recv`)
- DHCP client
- DNS resolver (`/etc/resolv.conf`)
- Firewall básico — bem no futuro

**Por que servidor separado e não no kernel:** mantém o kernel pequeno e o parsing de pacotes de rede (superfície de ataque grande) isolado do ring 0. Modelo alinhado com a filosofia de mover cada vez mais coisa pra user-space (ver Doca/HAL em #ideias).

**Dependências bloqueantes:**
- IPC (#ipc) — servidor precisa conversar com apps e com o driver de rede via mensagens
- Driver Intel e1000 ou RTL8139 funcionando (#rede)

Canal pra discutir a arquitetura antes de portar o lwIP — como fica a interface entre driver ↔ netstack ↔ syscalls de socket?
