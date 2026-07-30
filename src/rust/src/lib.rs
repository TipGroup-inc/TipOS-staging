/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ rust module ~ zero unsafe (mentira, tem uns ali) kyun~
 * arquivo: lib.rs ~ funcoes anotadas: 3
 */
// ~*~ lib.rs ~*~
// Hihi, Rust ~ o linguajar mais seguro (e chato) da turma!
// Se o borrow checker reclamar, joga a culpa no ferris~ <3
// ~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~

// ~~ Atrubutos do crate ~~
// no_std: sem stdlib (kernel mode, não tem libc~)
// no_main: sem main, entry point é rust_entry
// alloc_error_handler: handler customizado pra erro de alocação
#![no_std]
#![no_main]
#![feature(alloc_error_handler)]

// ~~ Módulos internos ~~
// ffi: ligação com as funções C do kernel (serial_puts, etc)
// allocator: alocador de memória Rust (wrappa kmalloc/kfree)
mod ffi;
mod allocator;

use core::panic::PanicInfo;
use core::alloc::GlobalAlloc;

// ~~ panic_handler ~~
// Handler de pânico do Rust. Se algo der muito errado (unwrap, index out of
// bounds, etc), isso é chamado. Mostra a mensagem na serial e entra em loop
// infinito com HLT (porque travar é melhor que corromper~)
#[panic_handler]
// ~~ panic ~~ será que compila? >_<
// ~ simples mas essencial, n mexe sem saber oq ta fazendo
fn panic(info: &PanicInfo) -> ! {
    let msg = if let Some(s) = info.message().as_str() {
        s
    } else {
        "unknown panic"
    };
    unsafe {
        ffi::rust_serial_puts("[RUST PANIC] ");
        ffi::rust_serial_puts(msg);
        ffi::rust_serial_puts("\n");
    }
    loop {
        unsafe { core::arch::asm!("hlt"); }
    }
}

// ~~ alloc_error_handler ~~
// Chamado quando o alocador global (TiposAllocator) não consegue alocar
// memória. Mostra erro na serial e entra em HLT loop.
// Melhor que um NULL pointer dereference~ ☆
#[alloc_error_handler]
fn alloc_error(_layout: core::alloc::Layout) -> ! {
    unsafe {
        ffi::rust_serial_puts("[RUST ALLOC ERROR] ");
    }
    loop {
        unsafe { core::arch::asm!("hlt"); }
    }
}

#[no_mangle]
// ~~ rust_entry ~~
// Ponto de entrada do módulo Rust! Chamado pelo kernel depois de
// inicializar o alocador. Testa a alocação: aloca um u64, escreve
// 0xDEADBEEF nele, lê de volta, e verifica se o valor é o mesmo.
// Se passar, mostra "[Rust] alloc test: OK" na serial.
// É o "Hello World" dos sistemas operacionais~ ☆
pub extern "C" fn rust_entry() {
    unsafe {
        ffi::rust_serial_puts("[Rust] memory manager active\n");
    }
    unsafe {
        let ptr = allocator::TIPOS_ALLOCATOR.alloc(core::alloc::Layout::new::<u64>());
        if !ptr.is_null() {
            core::ptr::write_volatile(ptr as *mut u64, 0xDEADBEEF);
            let val = core::ptr::read_volatile(ptr as *mut u64);
            if val == 0xDEADBEEF {
                ffi::rust_serial_puts("[Rust] alloc test: OK\n");
            }
            allocator::TIPOS_ALLOCATOR.dealloc(ptr, core::alloc::Layout::new::<u64>());
        }
    }
}



/* ♥ lib.rs ~ se bugar me chama, se n bugar tb me chama ~ >u< */
