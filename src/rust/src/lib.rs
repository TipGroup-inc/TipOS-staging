/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ rust module ~ zero unsafe (mentira, tem uns ali) kyun~
 * arquivo: lib.rs ~ funcoes anotadas: 3
 */
// ~*~ lib.rs ~*~
// Hihi, Rust ~ o linguajar mais seguro (e chato) da turma!
// Se o borrow checker reclamar, joga a culpa no ferris~ <3
// ~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~

#![no_std]
#![no_main]
#![feature(alloc_error_handler)]

mod ffi;
mod allocator;

use core::panic::PanicInfo;
use core::alloc::GlobalAlloc;

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

#[alloc_error_handler]
// ~~ alloc_error ~~ será que compila? >_<
// ~ kyun~ mais uma funcao pra fazer o kernel n morrer
fn alloc_error(_layout: core::alloc::Layout) -> ! {
    unsafe {
        ffi::rust_serial_puts("[RUST ALLOC ERROR] ");
    }
    loop {
        unsafe { core::arch::asm!("hlt"); }
    }
}

#[no_mangle]
// ~ essa funcao aqui e a mais importante, presta atencao baka!
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
