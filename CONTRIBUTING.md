# Contribuindo no TipOS <3

Moe moe kyun~ Bem-vinde! Aqui tem o fluxo pra tu não se perder (e não quebrar o kernel, hihi~).

## 1. Primeiros passos (10 min)

1. **Leia o AGENTS.md** — ele tem a stack, a estrutura do projeto e o onboarding completo.
2. **Boota o sistema**: `make kernel && make run` — se o QEMU abrir, o ambiente tá certo.
3. **Navegue os docs**: `docs/README.md` (índice) → `docs/KERNEL.md` → `docs/uml/` (diagramas).
4. **Repos irmãos**: `disp` e `term` precisam estar clonados em `../` (o Makefile raiz instala eles no disco).

## 2. Pegando uma tarefa

- Todas as tarefas moram no **GitHub Projects** (board "TipOS — Kanban"): https://github.com/orgs/TipGroup-inc/projects/8
- Regras do board (WIP, colunas, sprints): `docs/KANBAN.md`
- **Só pega card que tem: descrição + critério de aceite + label de squad.** Card vago não existe.
- Nunca comece o dia sem olhar o board. Sem silêncio, sem card vago. >_<

## 3. Labels que importam

| Label | Significado |
|-------|-------------|
| `good first issue` | Porta de entrada — ideal pro primeiro PR |
| `dificuldade/*` | facil / media / dificil — calibra com teu nível |
| `squad/*` | dono da tarefa (quem faz o review principal) |
| `sprint/*` | em qual sprint a entrega é esperada |

## 4. Fluxo de trabalho

```bash
git checkout -b <squad>/<numero-issue>-<descricao-curta>   # ex: fs-terminal/12-fix-fopen
# ...commits pequenos e incrementais...
git push origin <sua-branch>
```

- **Política de commit**: `feat:`, `fix:`, `docs:`, `chore:` — seguida dessa vez, tá?
- **PR**: usa o template (tem checklist automático). Linka o card/issue na descrição.
- **Reviews**: precisa de 2 approvals. Review automático do time dono da pasta via CODEOWNERS.
- **CI**: `make run-test` headless no QEMU — precisa ficar verde (quando o Actions existir, issue #28).

## 5. Regras que não têm discussão (>_<)

1. **Ninguém deleta arquivo num commit sem aprovação de 2 pares** — documenta no card.
2. **PR com mais de 20 arquivos** precisa de reunião de alinhamento antes (nem pro Coelho, dnv!).
3. `main`/`master` são protegidas: PR obrigatório + 2 approvals.
4. Refatoração **incremental** — nunca 50 funções de uma vez.
5. **TDD onde der**: syscalls têm teste (fork + getpid no CI).
6. Comentários em pt-BR, estilo moe moe kyun (padrão do projeto, não discute >_<).

## 6. Em caso de dúvida

- Daily de 15 min todo dia: cada um fala o que fez, o que vai fazer e onde travou.
- Tarefa crítica (fork, execve, VFS)? **Pair programming obrigatório** — chama alguém do squad vizinho.
- Docs históricos têm banner 🕰️ — contam o passado, não o presente.