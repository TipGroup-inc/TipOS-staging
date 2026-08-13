## Checklist — leia antes de abrir o PR, baka~ >_<

**Antes de abrir:**
- [ ] Card no board em "Code Review" (move o card, não deixa no backlog)
- [ ] Branch com nome padrão: `<squad>/<issue-numero>-<descricao-curta>` (ex: `fs-terminal/12-fix-fopen`)
- [ ] Commits seguem a política: `feat:`, `fix:`, `docs:`, `chore:` (zoeira opcional depois do `~~`)

**Código:**
- [ ] Compila: `make kernel` (e `make userland` se mexeu em src/)
- [ ] Testei no QEMU: `make run-test` (log em /tmp/tipos-boot.log)
- [ ] Refatoração incremental, nada de big bang (regra XP)
- [ ] **Menos de 20 arquivos mudados** — se passou disso, precisa reunião de alinhamento antes (nem pro Coelho, dnv!)

**Regras sagradas:**
- [ ] Não deletei nenhum arquivo — se deletei, tenho aprovação de 2 pares documentada no card
- [ ] Critério de aceite do card foi atendido (linka o card na descrição)
- [ ] Docs atualizadas se o comportamento mudou (docs/README.md é o índice)

**Depois de abrir:**
- [ ] Aguardar 2 approvals do time (ou 1 se for bugfix urgente combinado no daily)
- [ ] CI verde quando existir (GitHub Actions, issue #28)