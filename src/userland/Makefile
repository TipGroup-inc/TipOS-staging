/* moe moe kyun <3 */
/* moe moe kyun <3 */
# ~~ Makefile do userland TipOS ~~
# Compila programas de usuário como flat binary → Mach-O via packer.
# Tudo freestanding (sem libc do sistema, só a nossa~)
#
# ~~ Toolchain ~~
# gcc: compilador (sim, GCC, não me julga~)
# objcopy: extrai .text como flat binary
# python3: roda o macho_pack.py pra wrap em Mach-O
# mcopy: copia pro disco (imagem FAT)
CC = gcc
# ~~ Flags de compilação ~~
# -ffreestanding: sem assumir libc host
# -nostdlib -static -fno-PIC: tudo estático, sem position-independent
# -mno-red-zone: kernel mode (interrupções usam stack abaixo do RSP)
# -mno-mmx -mno-sse -mgeneral-regs-only: sem SIMD (salvar contexto é chato~)
# -nostartfiles: sem crt0 padrão (temos o nosso~)
# -fno-stack-protector: sem canários (somos corajosas~)
# -O1: otimização básica (não sou exigente~)
# -I include: headers customizados
CFLAGS = -ffreestanding -nostdlib -static -fno-PIC \
         -mno-red-zone -mno-mmx -mno-sse -mgeneral-regs-only \
         -nostartfiles -fno-asynchronous-unwind-tables \
         -fno-unwind-tables -Wl,--build-id=none \
         -fno-stack-protector -O1 -I include

# ~~ Linker ~~
# -e _start: entry point = _start (nossa crt0)
# -T libc/link.ld: script de linker personalizado (link.ld manda lembranças~)
LDFLAGS = -e _start -T libc/link.ld
OBJCOPY = objcopy
PACKER = python3 tools/macho_pack.py
MCOPY = mcopy -o -i ../../disk.img
LIBC_SRC = libc/crt0.c libc/stdio.c libc/stdlib.c libc/string.c libc/tui.c

# ~~ Lista de programas ~~
# Por enquanto vazia, mas quando tiver programa, é só adicionar o nome
# (sem .c) que o Make cuida do resto~ (mágica~)
PROGS =
PROGS_SRC = $(addsuffix .c, $(addprefix progs/, $(PROGS)))
PROGS_ELF = $(addsuffix .elf, $(addprefix ../../build/userland/, $(PROGS)))
PROGS_BIN = $(addsuffix .bin, $(addprefix ../../build/userland/, $(PROGS)))
PROGS_MACHO = $(addsuffix .macho, $(addprefix ../../build/userland/, $(PROGS)))

.PHONY: all clean install

# ~~ all ~~
# Compila tudo (delegado pros targets específicos~)
all: $(PROGS_MACHO)

# ~~ Cria diretório de build ~~
../../build/userland:
	mkdir -p ../../build/userland

# ~~ .c → .elf ~~
# Compila programa + libc em um ELF estático
../../build/userland/%.elf: progs/%.c $(LIBC_SRC) ../../build/userland
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBC_SRC)

# ~~ .elf → .bin ~~
# Extrai seção .text como flat binary (sem headers ELF~)
../../build/userland/%.bin: ../../build/userland/%.elf
	$(OBJCOPY) -O binary -j .text $< $@

# ~~ .bin → .macho ~~
# Empacota em Mach-O (pro kernel conseguir carregar~)
../../build/userland/%.macho: ../../build/userland/%.bin
	$(PACKER) $< $@

# ~~ install ~~
# Copia os Mach-O pro disco (imagem FAT, diretório /BIN)
install: all
	@for f in $(PROGS_MACHO); do \
		name=`basename $$f .macho | tr a-z A-Z`; \
		$(MCOPY) $$f ::/BIN/$$name; \
	done

# ~~ clean ~~
# Limpa tudo (volta ao estado virgem~)
clean:
	rm -f ../../build/userland/$(addsuffix .elf, $(PROGS))
	rm -f ../../build/userland/$(addsuffix .bin, $(PROGS))
	rm -f ../../build/userland/$(addsuffix .macho, $(PROGS))
