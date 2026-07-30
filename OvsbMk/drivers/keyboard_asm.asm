; moe moe kyun <3
;
; keyboard_asm.asm — IRQ1 (keyboard) and IRQ12 (mouse) interrupt wrappers
;
; These are low-level interrupt service routines (ISRs) that handle the
; hardware interrupt entry/exit sequence in x86-64 long mode.
;
; When an interrupt fires via the IDT (Interrupt Descriptor Table), the CPU
; automatically pushes SS, RSP, RFLAGS, CS, and RIP onto the stack. Then
; control transfers here. We must:
;   1. Save all callee-saved registers (RBX, RBP, R12-R15) AND caller-saved
;      registers (RAX, RCX, RDX, RSI, RDI, R8-R11) because the C handler
;      may clobber them per the System V AMD64 ABI.
;   2. Call the C interrupt handler function.
;   3. Restore all registers in reverse order.
;   4. Execute IRETQ (Interrupt Return, 64-bit), which pops RIP, CS, RFLAGS,
;      RSP, SS from the interrupt stack frame.
;
; The keyboard IRQ1 handler calls keyboard_handler() which reads the scancode
; from port 0x60 and sends EOI (0x20) to the master PIC at port 0x20.
;
; The mouse IRQ12 handler calls mouse_handler() which reads the PS/2 data
; byte from port 0x60 and sends EOI to both the slave PIC (0xA0) and the
; master PIC (0x20) since IRQ12 is cascaded through the slave.
;
; The CPU automatically disables interrupts (clears IF) on entry and
; restores them on IRETQ, so we don't need explicit CLI/STI.

bits 64
global keyboard_irq_handler
extern keyboard_handler

keyboard_irq_handler:
    ; Save all general-purpose registers (15 pushes = 120 bytes stack).
    ; This is necessary because the C handler may use any register per
    ; the calling convention. We save everything to be safe.
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    call keyboard_handler
    ; Restore in reverse order (stack is LIFO)
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    iretq  ; Return from interrupt: pops RIP, CS, RFLAGS, RSP, SS

; mouse_irq_handler: Same save/restore pattern as keyboard, but calls
; mouse_handler() instead. IRQ12 is on the slave PIC (IRQ 8-15 cascade).
global mouse_irq_handler
extern mouse_handler

mouse_irq_handler:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    call mouse_handler
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    iretq

; ♥ keyboard_asm.asm ~ feito com carinho (e uma raiva controlada) ~ kyun!
