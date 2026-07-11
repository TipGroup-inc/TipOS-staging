CC = gcc
CFLAGS = -ffreestanding -nostdlib -static -fPIC \
         -mno-red-zone -mno-mmx -mno-sse -mgeneral-regs-only \
         -nostartfiles -fno-asynchronous-unwind-tables \
         -fno-unwind-tables -Wl,--build-id=none \
         -fno-stack-protector -O1
LDFLAGS = -e _start -Wl,-Ttext,0
OBJCOPY = objcopy
PACKER = python3 tools/macho_pack.py
MCOPY = mcopy -o -i ../../disk.img

PROGS = hello
PROGS_SRC = $(addsuffix .c, $(addprefix progs/, $(PROGS)))
PROGS_ELF = $(addsuffix .elf, $(addprefix ../../build/userland/, $(PROGS)))
PROGS_BIN = $(addsuffix .bin, $(addprefix ../../build/userland/, $(PROGS)))
PROGS_MACHO = $(addsuffix .macho, $(addprefix ../../build/userland/, $(PROGS)))

.PHONY: all clean install

all: $(PROGS_MACHO)

../../build/userland:
	mkdir -p ../../build/userland

../../build/userland/%.elf: progs/%.c ../../build/userland
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

../../build/userland/%.bin: ../../build/userland/%.elf
	$(OBJCOPY) -O binary -j .text $< $@

../../build/userland/%.macho: ../../build/userland/%.bin
	$(PACKER) $< $@

install: all
	@for f in $(PROGS_MACHO); do \
		name=`basename $$f .macho | tr a-z A-Z`; \
		$(MCOPY) $$f ::$$name; \
	done

clean:
	rm -f ../../build/userland/$(addsuffix .elf, $(PROGS))
	rm -f ../../build/userland/$(addsuffix .bin, $(PROGS))
	rm -f ../../build/userland/$(addsuffix .macho, $(PROGS))
