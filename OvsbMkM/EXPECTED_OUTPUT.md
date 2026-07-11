```markdown
% ============================================================================
% OvsbMkM (Micro Kernel) - SAÍDA ESPERADA (ATUALIZADO)
% ============================================================================
% Arquivo: docs/EXPECTED_OUTPUT.md
% Descrição: Screenshots e exemplos de saída terminal esperada
% Versão: 2.0 — 64-bit com GRUB + Multiboot2
% ============================================================================

# Saída Esperada - Terminal OvsbMkM Fase 1 (64-bit)

Este documento mostra exatamente o que você deve ver ao executar o kernel 64-bit.

---

## 🎯 Inicialização

### Ao Executar `./build.sh && ./run.sh`

**Terminal output (durante compilação):**

```
[1/4] Compilando bootloader...
[2/4] Compilando kernel...
[3/4] Linkando...
[4/4] Criando ISO...

PRONTO! ISO criada: OvsbMkM.iso
Execute: ./run.sh
```

**Dentro do QEMU (na janela):**

```
OvsbMkM 64-bit Terminal v3.0
Digite 'help'

MkM> █
```

O cursor piscará depois de `>`. Se vir isso, **kernel está funcional!** ✅

---

## 📖 Exemplos de Comandos

### 1. Comando: `help`

**Input:**
```
MkM> help
```

**Output esperada:**
```
help, clear, echo, about, shutdown
MkM> 
```

**O que vira:** Lista de comandos em uma linha, depois novo prompt.

---

### 2. Comando: `echo`

#### Exemplo 1: Texto simples

**Input:**
```
MkM> echo Ola Mundo!
```

**Output esperada:**
```
Ola Mundo!
MkM> 
```

#### Exemplo 2: Múltiplas palavras

**Input:**
```
MkM> echo O MkM eh 64-bit
```

**Output esperada:**
```
O MkM eh 64-bit
MkM> 
```

#### Exemplo 3: Sem argumentos

**Input:**
```
MkM> echo
```

**Output esperada:**
```

MkM> 
```

(Nada impresso, apenas nova linha)

---

### 3. Comando: `about`

**Input:**
```
MkM> about
```

**Output esperada:**
```
OvsbMkM 64-bit
Microkernel macOS High Sierra
MkM> 
```

---

### 4. Comando: `clear`

**Antes:**
```
MkM> help
help, clear, echo, about, shutdown
MkM> 
```

**Depois de `clear`:**
```
MkM> 
```

A tela fica completamente vazia. Cursor volta ao topo esquerdo.

---

### 5. Comando: `shutdown`

**Input:**
```
MkM> shutdown
```

**Output esperada:**
```
Desligando...
```

A CPU para (`cli; hlt`). O QEMU **não fecha automaticamente** — pressione `Ctrl+Alt+Q` ou feche a janela para sair.

---

### 6. Comando Inválido

**Input:**
```
MkM> xyz
```

**Output esperada:**
```
? xyz
MkM> 
```

---

## ⌨️ Entrada do Teclado

### Digitação Normal

**Input:** Digite `echo test` lentamente

**Aparência na tela:**

```
MkM> e
MkM> ec
MkM> ech
MkM> echo
MkM> echo 
MkM> echo t
MkM> echo te
MkM> echo tes
MkM> echo test
```

Cada caractere aparece imediatamente após digitar.

### Backspace

**Input:** Digite `echo test`, depois Backspace 4 vezes

**Aparência:**

```
MkM> echo test
MkM> echo tes
MkM> echo te
MkM> echo t
MkM> echo 
```

Cada backspace remove um caractere.

### Enter

**Input:** Digite `help`, depois Enter

**Aparência:**

```
MkM> help
help, clear, echo, about, shutdown
MkM> 
```

Comando é executado, nova linha, novo prompt.

---

## 🎬 Cenário Completo

Fluxo típico de uso:

```
OvsbMkM 64-bit Terminal v3.0
Digite 'help'

MkM> help
help, clear, echo, about, shutdown

MkM> about
OvsbMkM 64-bit
Microkernel macOS High Sierra

MkM> echo Teste do MkM 64-bit!
Teste do MkM 64-bit!

MkM> xyz
? xyz

MkM> clear
[Tela limpa]

MkM> shutdown
Desligando...
[CPU para — feche o QEMU manualmente]
```

---

## 📊 Detalhes de Visualização

### Cores

- **Fundo:** Preto absoluto
- **Texto:** Verde claro (VGA color 0x0A)
- **Cursor:** Retângulo verde piscante

### Fonte

- Tipo: Monospace (VGA padrão)
- Resolução: 80 caracteres × 25 linhas

### Layout

```
┌────────────────────────────────────────────────────────────────────────────┐
│OvsbMkM 64-bit Terminal v3.0                                                 │
│Digite 'help'                                                                │
│                                                                             │
│MkM> █                                                                       │
│                                                                             │
│                         (21 linhas vazias)                                  │
│                                                                             │
└────────────────────────────────────────────────────────────────────────────┘
```

---

## 🔄 Scroll

Se digitar muitos comandos (saída > 23 linhas), a tela rola automaticamente para cima.

---

## ⚠️ Se NÃO Ver Isso

### Cenário 1: Tela Preta, Nada Aparece

**Significado:** Kernel crashou antes de inicializar VGA.

**O que fazer:**
1. Verifique se `boot64.asm` e `kernel.c` compilaram sem erros
2. Verifique tamanho de `build/kernel.elf` (deve ser ~22 KB)
3. Execute com `-no-reboot` para ver mensagens de erro
4. Recompile tudo: `./build.sh`

### Cenário 2: "Boot error" ou GRUB não carrega

**Significado:** ISO mal gerada ou kernel não encontrado.

**O que fazer:**
1. Verifique se `iso/boot/kernel.elf` existe
2. Verifique `iso/boot/grub/grub.cfg` está correto
3. Regenere a ISO: `grub-mkrescue -o OvsbMkM.iso iso/`

### Cenário 3: Prompt aparece mas teclado não responde

**Significado:** Driver PS/2 travou.

**O que fazer:**
1. O QEMU emula PS/2 por padrão — deve funcionar
2. Tente pressionar teclas algumas vezes
3. Reinicie o QEMU

### Cenário 4: "64" apareceu, mas terminal não

**Significado:** Kernel antigo (teste) ainda na ISO.

**O que fazer:**
1. Verifique se `kernel.c` é o arquivo com terminal (não o de teste)
2. Recompile: `./build.sh`

---

## 📋 Checklist de Verificação Visual

- [ ] Janela QEMU abre corretamente
- [ ] Fundo é preto
- [ ] Texto é verde claro e legível
- [ ] Primeira linha: "OvsbMkM 64-bit Terminal v3.0"
- [ ] Prompt "MkM> " aparece
- [ ] Teclado funciona (letras aparecem ao digitar)
- [ ] Backspace apaga caracteres
- [ ] Enter executa comando
- [ ] `help` lista comandos
- [ ] `echo teste` imprime "teste"
- [ ] `about` mostra informações
- [ ] `clear` limpa tela
- [ ] `shutdown` para a CPU
- [ ] Comando inválido mostra "? comando"

Se todos marcados: **Terminal 64-bit está 100% correto! ✅**

---

**Última atualização:** 2026-06-21 — Terminal 64-bit funcional! 🚀
```