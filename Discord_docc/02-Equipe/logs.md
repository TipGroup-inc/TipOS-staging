/* moe moe kyun <3 */
📜 **LOGS — TipOS**

Formato sugerido:

```
**Tipo:** compilação / QEMU serial / crash / dmesg
**Comando usado:** ex. `make run` ou `qemu-system-x86_64 -cdrom TipOS.iso -drive file=disk.img,format=raw -serial stdio`
```
```log
[colar o log aqui dentro de um bloco de código]
```

**Dicas de debug (`KERNEL.md`, seção 20):**
- O kernel manda logs pra porta serial COM1 (`0x3F8`) — sempre mais confiável que VGA
- `debug_puts()` escreve no VGA + serial · `serial_puts()` só no serial · `vga_puts()` só no VGA
- Pra debug com GDB:
```bash
qemu-system-x86_64 -cdrom TipOS.iso -drive file=disk.img,format=raw -s -S
gdb -ex "target remote :1234" -ex "symbol-file OvsbMkM/build/kernel.elf" -ex "break kmain" -ex "continue"
```
- O syscall handler escreve o número da chamada nos primeiros pixels do VGA — útil pra ver qual syscall travou sem precisar do serial.

Cole aqui qualquer crash, backtrace ou log de boot estranho.
