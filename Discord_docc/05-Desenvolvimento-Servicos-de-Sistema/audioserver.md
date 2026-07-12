🎚️ **AUDIO SERVER — TipOS**

**Status atual: não existe.** Companion do canal #audio (pasta Drivers) — aqui é a discussão do servidor em si, lá é sobre o driver HDA.

**Plano:**
- Servidor de áudio em user-space: mixagem de múltiplos streams
- Comunicação com o driver Intel HDA via IPC
- API pra apps: `play`, `record`

**Decisão em aberto (`tipos-dev-stack.md`):** portar um subset do **PulseAudio** ou escrever um servidor simples do zero. Ainda não decidido — depende de quanto esforço o time quer investir vs. quão cedo precisamos de áudio funcionando.

**Dependências bloqueantes:**
- IPC (#ipc) — apps precisam falar com o servidor sem acesso direto ao hardware
- Driver Intel HDA (#audio)

Ainda no papel — bora desenhar a API (`play`/`record`, formato de stream, latência aceitável) antes de qualquer código.
