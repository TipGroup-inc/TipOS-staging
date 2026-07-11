% ============================================================================
% OvsbMkM (Micro Kernel) - Guia de Troubleshooting
% ============================================================================
% Arquivo: docs/TROUBLESHOOTING.md
% Descrição: Solução de problemas comuns em compilação e execução
% ============================================================================

# Troubleshooting - Fase 1 do OvsbMkM

## 🔧 Problemas de Compilação

### Problema: `nasm: command not found`

**Descrição:** NASM não está instalado ou não está no PATH.

**Solução:**

#### Linux (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install nasm
nasm -version
```

#### Linux (Fedora/RHEL)
```bash
sudo dnf install nasm
nasm -version
```

#### macOS
```bash
brew install nasm
nasm -version
```

#### Windows (Chocolatey)
```powershell
choco install nasm
nasm -version
```

#### Windows (Manual)
1. Baixe NASM de https://www.nasm.us/
2. Extraia para `C:\nasm\`
3. Adicione `C:\nasm\` ao PATH do Windows
4. Restart terminal e teste: `nasm -version`

---

### Problema: `gcc: command not found`

**Descrição:** GCC não está instalado.

**Solução:**

#### Linux (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install gcc make
gcc --version
```

#### Linux (Fedora/RHEL)
```bash
sudo dnf install gcc make
gcc --version
```

#### macOS
```bash
xcode-select --install
gcc --version
```

#### Windows (Chocolatey)
```powershell
choco install mingw
gcc --version
```

#### Windows (Manual)
1. Baixe MinGW-w64 de https://mingw-w64.org/
2. Execute instalador, escolha "x86_64"
3. Instale em `C:\mingw64\`
4. Adicione `C:\mingw64\bin` ao PATH
5. Restart terminal e teste: `gcc --version`

---

### Problema: `ld: command not found`

**Descrição:** GNU LD (linker) não está instalado.

**Solução:**

#### Linux
```bash
sudo apt install binutils    # Inclui ld, nm, objdump, etc.
ld --version
```

#### macOS
LD vem com Xcode Command Line Tools:
```bash
xcode-select --install
ld -version
```

#### Windows
Incluso com MinGW-w64 (instale como GCC acima).

---

### Problema: `Makefile:1: make: command not found`

**Descrição:** GNU Make não está instalado.

**Solução:**

#### Linux
```bash
sudo apt install make
make --version
```

#### macOS
```bash
xcode-select --install
make --version
```

#### Windows (Chocolatey)
```powershell
choco install make
make --version
```

---

### Problema: Erro de compilação do `boot.asm`

```
boot.asm:45: error: invalid operand size for instruction
```

**Descrição:** Erro de sintaxe em Assembly.

**Solução:**

1. Verifique se sintaxe Intel está sendo usada (NASM padrão)
2. Procure por registradores de tamanho mismatch (ex: `mov eax, rax`)
3. Verifique se constantes estão definidas corretamente
4. Teste compilação isolada:

```bash
nasm -felf64 -F dwarf -o build/boot.o kernel/boot/boot.asm -l build/boot.lst
cat build/boot.lst | grep -A2 -B2 "error"
```

---

### Problema: Erro de compilação do `kernel.c`

```
kernel.c:45: error: expected '=', ',', ';', 'asm' or '__attribute__' before '{' token
```

**Descrição:** Erro de sintaxe C.

**Solução:**

1. Procure por falta de `;` ao final de statements
2. Procure por falta de `}` em funções/structs
3. Procure por `typedef` incompletos
4. Compile com verbose:

```bash
gcc -ffreestanding -fno-builtin -nostdlib -Wall -Wextra -E kernel/core/kernel.c | head -50
```

---

### Problema: Erro de linker `undefined reference to 'kmain'`

```
ld: build/boot.o: undefined reference to `kmain'
```

**Descrição:** Símbolo `kmain` não foi encontrado em `kernel.o`.

**Solução:**

1. Verifique se `kernel.c` foi compilado:
```bash
nm build/kernel.o | grep kmain
```

2. Deve aparecer como `T kmain` (defined in text section)

3. Se não aparecer, recompile:
```bash
gcc -ffreestanding -fno-builtin -nostdlib -Wall -O2 -c \
    -o build/kernel.o kernel/core/kernel.c
```

4. Verifique se `kernel.c` tem função `kmain`:
```bash
grep "^void kmain" kernel/core/kernel.c
```

---

### Problema: `kernel.elf` muito grande ou vazio

**Descrição:** Arquivo ELF gerado está corrupto ou inusitadamente grande.

**Solução:**

1. Verifique tamanho:
```bash
ls -lh build/kernel.elf
file build/kernel.elf
```

2. Esperado: 10-20 KB, tipo "ELF 64-bit LSB executable"

3. Se vazio (0 bytes), erro no linker. Tente:
```bash
ld -m elf_x86_64 -T kernel/boot/linker.ld -v \
    -o build/kernel.elf build/boot.o build/kernel.o
```

4. Se muito grande (> 1 MB), pode ter símbolos debug. Remova:
```bash
strip build/kernel.elf
```

---

## 🎮 Problemas de Execução

### Problema: `qemu-system-x86_64: command not found`

**Descrição:** QEMU não está instalado.

**Solução:**

#### Linux (Ubuntu/Debian)
```bash
sudo apt install qemu-system-x86
qemu-system-x86_64 --version
```

#### Linux (Fedora/RHEL)
```bash
sudo dnf install qemu-system-x86
qemu-system-x86_64 --version
```

#### macOS
```bash
brew install qemu
qemu-system-x86_64 --version
```

#### Windows (Chocolatey)
```powershell
choco install qemu
qemu-system-x86_64 --version
```

#### Windows (Manual)
1. Baixe QEMU de https://qemu.weilnetz.de/
2. Execute instalador
3. Instale em `C:\Program Files\QEMU\`
4. Adicione ao PATH
5. Restart e teste: `qemu-system-x86_64 --version`

---

### Problema: QEMU abre mas não exibe nada

**Sintomas:**
- Janela preta
- Nenhuma saída
- Sem crash, apenas vazio

**Solução:**

1. Tente com saída serial:
```bash
qemu-system-x86_64 -m 256M -kernel build/kernel.elf \
    -serial stdio -no-reboot
```

2. Se vir saída, significa VGA não está funcionando (raro)

3. Verifique se kernel.elf é válido:
```bash
objdump -f build/kernel.elf
```

Deve mostrar:
```
architecture: i386:x86-64, flags 0x00000102:
EXEC_P, D_PAGED
start address 0x0000000000100000
```

4. Se `start address` não é `0x100000`, o linker script falhou

5. Recompile tudo:
```bash
make clean
make all
make run
```

---

### Problema: Tela preta, mas sem prompt

**Sintomas:**
- QEMU abre
- Tela está preta (não vazia)
- Nenhuma mensagem de inicialização
- Nenhuma resposta do teclado

**Causas Possíveis:**

1. **Kernel crashed silenciosamente** — Muito possível em bare-metal

**Solução:**

Use debugging via serial:

```bash
# Adicione em kernel.c após vga_clear_screen():
vga_puts("DEBUG: VGA inicializado\n");

# Recompile e execute
```

2. **Framebuffer VGA não foi inicializado** — Menos provável

**Solução:**

Verifique endereço de framebuffer:
```c
// Em kernel.c, após vga_clear_screen():
uint16_t *test = (uint16_t*)0xB8000;
*test = 0x0A41;  // Escrever 'A' em verde
```

3. **Loop infinito antes de kmain()** — Boot assembly travou

**Solução:**

Adicione breakpoints no boot.asm:

```asm
; Após cada etapa importante
mov ax, 0x1234
mov bx, 0x5678
hlt  ; Testa até aqui
jmp $
```

---

### Problema: Kernel exibe algo, mas caracteres aleatórios

**Sintomas:**
- Mensagens aparecem, mas com "lixo"
- Caracteres errados ou mal posicionados
- Cores erradas

**Causas:**

1. **Offset do buffer VGA incorreto** — memória errada
2. **Máscara de cor errada** — cor calculada mal
3. **Endereço físico do framebuffer errado** — raro em QEMU

**Solução:**

1. Teste simples em kernel.c:
```c
// Escrever um único 'A' no canto
uint16_t *vga = (uint16_t*)0xB8000;
*vga = (0x0A << 8) | 'A';  // Verde + 'A'
while(1);
```

2. Se aparecer 'A' verde no canto superior esquerdo, VGA funciona

3. Se não, VGA está quebrado — improvável, é endereço padrão

---

### Problema: Prompt aparece mas teclado não funciona

**Sintomas:**
- "MkM > " aparece
- Digitação não aparece
- Nenhuma resposta do teclado

**Causas:**

1. **Driver PS/2 nunca saiu do ps2_read_key()** — esperando scancode
2. **Status port 0x64 sempre retorna 0** — porta errada
3. **Timeout infinito em delay_us()** — loop muito lento

**Solução:**

1. Teste porta PS/2 diretamente:
```c
// Em kernel.c, após prompt
uint8_t status = inb(0x64);
vga_puts("Status: 0x");
vga_put_int(status);
vga_putchar('\n');
```

Se status é sempre 0x00, porta está errada.

2. Teste se scancode está sendo lido:
```c
uint8_t scancode = inb(0x60);
vga_puts("Scancode: 0x");
vga_put_int(scancode);
vga_putchar('\n');
// Loop esperando próxima tecla
```

3. Se recebe 0xFF ou 0x00 sempre, emulação PS/2 não funciona no QEMU

**Workaround:** Use `-serial stdio` e modifique kernel para ler de serial:
```c
uint8_t inb_serial() {
    return inb(0x3F8);  // Porta serial COM1
}
```

---

### Problema: Comando `help` não funciona

**Sintomas:**
- Digite "help" + Enter
- Aparece "Comando nao encontrado: help"
- Outros comandos também não funcionam

**Causas:**

1. **strcmp() retorna valor errado** — implementação incorreta
2. **Parser não separou argumentos** — buffer de comando vazio
3. **Tabela de comandos não inicializada** — código não executado

**Solução:**

1. Teste parser:
```c
// Em execute_command(), antes de strcmp:
vga_puts("DEBUG: argc=");
vga_put_int(cmd_parsed.argc);
vga_puts(", argv[0]=");
if (cmd_parsed.argc > 0) {
    vga_puts(cmd_parsed.argv[0]);
}
vga_putchar('\n');
```

2. Teste strcmp:
```c
// Função de teste
void test_strcmp() {
    if (strcmp("help", "help") == 0) {
        vga_puts("strcmp OK\n");
    } else {
        vga_puts("strcmp BROKEN\n");
    }
}
// Chame em kmain() antes de terminal_loop()
```

3. Se strcmp está quebrado, há erro na função. Comparar byte a byte manualmente:
```c
const char *a = "help";
const char *b = "help";
while (*a && *b && *a == *b) {
    vga_putchar('.');
    a++;
    b++;
}
vga_putchar('\n');
```

---

### Problema: `echo` imprime a comando, não o argumento

**Sintomas:**
```
MkM > echo test
echo
```

Ao invés de:
```
MkM > echo test
test
```

**Causa:**

Loop em `cmd_echo()` começa em `i=0` em vez de `i=1`.

**Solução:**

Verifique kernel.c:
```c
/* ERRADO */
for (int i = 0; i < cmd_parsed.argc; i++) {
    vga_puts(cmd_parsed.argv[i]);
}

/* CERTO */
for (int i = 1; i < cmd_parsed.argc; i++) {
    vga_puts(cmd_parsed.argv[i]);
}
```

---

## 📊 Verificação de Saúde

Após compilar, execute este checklist:

```bash
# 1. Verificar kernel.elf existe e tem tamanho razoável
ls -lh build/kernel.elf
file build/kernel.elf
# Esperado: ~15KB, ELF 64-bit LSB executable

# 2. Verificar símbolos
nm build/kernel.elf | grep kmain
# Esperado: linhas com `T kmain` e `U strlen` etc.

# 3. Verificar seções
objdump -h build/kernel.elf
# Esperado: .text, .data, .bss, .multiboot

# 4. Executar e testar
make run
# Esperado: prompt MkM > aparece em segundos

# 5. Testar comandos
# Digite: help<Enter>
# Esperado: lista de comandos

# 6. Testar echo
# Digite: echo test<Enter>
# Esperado: "test" impresso

# 7. Testar clear
# Digite: clear<Enter>
# Esperado: tela limpa

# 8. Testar about
# Digite: about<Enter>
# Esperado: informações do MkM

# 9. Sair
# Digite: shutdown<Enter>
# Esperado: "Desligando..." e CPU halts
```

Se todos passam: **Parabéns! Fase 1 funciona! 🎉**

---

## 🔗 Recursos Adicionais

- **OSDev PS/2 Keyboard:** https://wiki.osdev.org/PS2_Keyboard
- **OSDev VGA:** https://wiki.osdev.org/VGA
- **x86-64 ISA Reference:** https://www.amd.com/en/technologies/x86
- **GCC Inline Assembly:** https://gcc.gnu.org/onlinedocs/gcc/Using-Inline-Assembly-with-C-Code.html
- **NASM Manual:** https://www.nasm.us/xdocs/

---

**Última atualização:** 2026-06-20

Se problema persistir:
1. Verifique permissões de arquivo (`chmod +x scripts/run.sh`)
2. Verifique espaço em disco (`df -h`)
3. Tente em máquina diferente (virtualização pode ajudar)
4. Abra issue no GitHub com output completo de erro
