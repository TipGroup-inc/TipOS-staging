# TipOS — Painel Kanban & Regras de Fluxo (Kanban/Lean + XP)

> moe moe kyun <3 — o board de verdade mora no GitHub Projects:
> **https://github.com/orgs/TipGroup-inc/projects/8** ("TipOS — Kanban")
>
> Este arquivo é o espelho versionado do board: colunas, regras e squads.
> Sem card vago, sem silêncio, sem big bang. Nhenhe~ >_<

---

## 1. Colunas do board (WIP limitado)

| Coluna | Regra |
|--------|-------|
| **Backlog** | Fila geral, ninguém pegou ainda |
| **Próximos** | Cards escolhidos pro sprint (no planning) |
| **Em Andamento** | **WIP: máx. 2 cards por pessoa** — não enche, baka~ |
| **Code Review** | PR aberto, aguardando **2 approvals** |
| **Teste** | Validação no QEMU / CI (`make run-test`) |
| **Concluído** | Feito, testado, mergeado — kyun! |

## 2. Squads

| Squad | Domínio | Labels |
|-------|---------|--------|
| **Core & Memória** | Estabilidade do Kernel, fork/exec, Scheduler, Memória | `squad/core-memoria` |
| **DevOps & QA** | CI, testes automatizados, board Kanban, docs, build | `squad/devops-qa` |
| **FS & Terminal** | VFS, ext2, TTY multiplexado, Shell, Comandos | `squad/fs-terminal` |
| **Rede & Drivers** | e1000, lwIP, socket syscalls, PCI, USB | `squad/rede-drivers` |
| **Userland & Ferramentas** | graphy, make, libc, TUI, cc bridge | `squad/userland-ferramentas` |

Tarefa que cruza dois squads (ex: fork mexe em processo mas o terminal precisa
aguentar múltiplo pid) é **pair obrigatório** entre alguém de cada lado — n faz sozinho, hihi~

## 3. Sprints (2 semanas cada)

- **Sprint 1 — Estabilidade + Ferramenta básica** (labels `sprint/1`):
  meta: sistema não cai com vários programas rodando, graphy usável.
  Entrega: graphy edita arquivo grande sem cair; make compila um .c simples via bridge host.
- **Sprint 2 — Terminal multiplex + Processo** (labels `sprint/2`):
  meta: múltiplo terminal F1-F6, fork/exec de verdade.
  Entrega: roda `sleep 10 &`, troca de terminal, scrollback ok.
- **Sprint 3 — VFS + ext2** (labels `sprint/3`):
  meta: FS com permissão e montagem.
  Entrega: monta ext2 além do FAT32, permissão básica funcionando.
- **Sprint 4 — Rede, TCP/IP mínimo** (labels `sprint/4`):
  meta: ping e wget funcionando, só isso mesmo.
  Entrega: `ping 8.8.8.8` e `wget http://example.com/index.html` no QEMU.

## 4. Regras Lean

1. **Antes de pegar uma tarefa, pergunta: isso ajuda o usuário final?** Se não ajuda, adia.
2. WIP máximo **2 cards por pessoa** em "Em Andamento" — nada de 50 tarefas abertas.
3. Nada de card vago: todo card tem **descrição clara + critério de aceite + squad**.
4. Prioridade é o terminal (multiplex) — virtio_gpu agora não é prioridade, fica pra depois.

## 5. Regras XP (código)

- **Pair programming** nas tarefas críticas: fork, execve, VFS. Um codifica, o outro revisa junto.
- **Collective code ownership**: qualquer um mexe em qualquer módulo, MAS **sempre abrindo card e avisando antes** — nunca silenciosamente.
- **TDD onde der**: teste de syscall (fork + getpid comparando resultado) rodando no CI.
- **Refatoração incremental**: proibido big bang. Limpa uma função por vez, commit pequeno. Nunca 50 funções de uma vez.
- **Coding standard**: AGENTS.md + `clang-format` + NASM com flag consistente.
- **CI verde**: GitHub Actions roda `make run-test` headless em cada PR; build tem que ficar verde.

## 6. Regras que não têm discussão (>_<)

1. **Ninguém deleta arquivo num commit sem aprovação de pelo menos 2 pares.**
2. **PR com mais de 20 arquivos mudados precisa de reunião de alinhamento antes de revisar** — sem exceção (nem pro Coelho, dnv!).
3. Branch `main` protegida: PR obrigatório + 2 approvals.
4. Política de commit: `feat:`, `fix:`, `docs:`, `chore:` — e seguida dessa vez, tá?

## 7. Ritual

- **Daily de 15 min**: cada um fala o que fez, o que vai fazer, e se travou em algo.
- **Sprint planning**: cards saem do backlog pra "Próximos" e depois pra "Em Andamento".
- **Primeira tarefa sugerida**: kfree de verdade (card #5) — chama alguém pra parear, kyun~ <3

## 8. Estado do board (snapshot)

- 31 cards no board (issues #3–#33), todos no **Backlog** até o primeiro planning.
- Labels de squad em todas as issues; sprints 1–4 marcados.
- Roadmap futuro (pós-sprint 4): auto-hospedagem, modo perf, wine, macOS compat, VT-x.