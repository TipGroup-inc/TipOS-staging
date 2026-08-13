---
name: Bug report
about: Algo quebrou no boot, kernel, userland ou ferramentas? Reporta aqui, sem drama~
title: "bug: "
labels: ["bug"]
assignees: []
---

**Descreve o bug** (o que aconteceu vs o que era pra acontecer)

**Como reproduzir**
1. Rodei `make kernel && make run`
2. Digitei `...`
3. Viu isso: ...

**Log / saída**
```
cola o log aqui (make run-test salva em /tmp/tipos-boot.log)
```

**Onde mora** (chuta pelo menos um):
- [ ] Kernel (OvsbMk/kernel)
- [ ] Drivers (OvsbMk/drivers)
- [ ] FS (OvsbMk/fs)
- [ ] Userland (src)
- [ ] Docs / CI / build

**Critério de aceite** (como a gente sabe que tá resolvido?)

**Regras lembradas** hihi~: sem deletar arquivo sem 2 pares, PR <20 arquivos, CI verde.