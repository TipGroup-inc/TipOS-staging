/* moe moe kyun <3 */
🔌 **USB — TipOS**

**Status atual: não existe.** Feature de fase avançada (Fase 4 do roadmap em equipe, junto com input, 4-6 semanas estimadas).

**Plano:**
- Driver **XHCI** (USB 3.0 host controller)
- Classes prioritárias: **HID** (teclado/mouse), **mass storage**, áudio
- Escopo inicial: enumeração de dispositivos, endpoints, transferências básicas

**Pré-requisito:** PCI enumeration (parte da Fase 1 do Doca/HAL — ver #ideias) pra descobrir o controlador XHCI no barramento.

Feature futura, mas se alguém já mexeu com XHCI antes, contribuições de design (layout de estruturas, fluxo de enumeração) são bem-vindas desde já — não precisa esperar chegar a vez no roadmap pra discutir.
