<!-- moe moe kyun <3 -->
🔊 **ÁUDIO — TipOS**

**Status atual: não existe.** Feature de fase avançada (Fase 7 do roadmap em equipe, junto com vídeo — 6-8 semanas estimadas).

**Plano:**
- Driver **Intel HDA** (High Definition Audio)
- Servidor de áudio em user-space: mixagem de streams + comunicação com apps via IPC
- Decisão em aberto (`tipos-dev-stack.md`): portar um subset do **PulseAudio** ou escrever um **servidor simples** do zero. ALSA foi descartado por ser "muito acoplado ao Linux".

**Dependências bloqueantes:** IPC (#ipc) funcionando, já que o servidor de áudio precisa rodar isolado do kernel e conversar com apps via mensagens.

Ainda não começou — se alguém curte áudio/DSP, esse é o canal pra desenhar a arquitetura (API de `play`/`record`, formato de stream, mixagem) antes de qualquer driver ser escrito.
