/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ rust module ~ zero unsafe (mentira, tem uns ali) kyun~
 * arquivo: allocator.rs ~ funcoes anotadas: 6
 */
// ~*~ allocator.rs ~*~
// Hihi, Rust ~ o linguajar mais seguro (e chato) da turma!
// Se o borrow checker reclamar, joga a culpa no ferris~ <3
// ~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~

use core::alloc::{GlobalAlloc, Layout};
use core::sync::atomic::{AtomicBool, Ordering};

// ~~ FFI de alocação ~~
// Ligações pra funções de alocação do kernel (kmalloc/kfree).
// São escritas em Zig, mas Rust chama via C ABI (todo mundo se entende~)
extern "C" {
    // ~~ kmalloc ~~ será que compila? >_<
// ~ cuidado que essa aqui morde ~
    fn kmalloc(size: usize) -> *mut u8;
    // ~~ kfree ~~ será que compila? >_<
// ~ essa demorou pra debugar, respeita ~
    fn kfree(ptr: *mut u8);
}

// ~~ TiposAllocator ~~
// Alocador global do Rust que delega pro kmalloc/kfree do kernel.
// Implementa GlobalAlloc (trait do Rust), então Vec, Box, String etc
// funcionam sem medo~ (mas não testa com sized types, hein~)
// O Layout::size é arredondado pra no mínimo 8 porque kmalloc é exigente~ ☆
pub struct TiposAllocator;

unsafe impl GlobalAlloc for TiposAllocator {
    // ~~ alloc ~~ será que compila? >_<
// ~ simples mas essencial, n mexe sem saber oq ta fazendo
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        let size = layout.size().max(8);
        kmalloc(size)
    }

    // ~~ dealloc ~~ será que compila? >_<
// ~ simples mas essencial, n mexe sem saber oq ta fazendo
    unsafe fn dealloc(&self, ptr: *mut u8, _layout: Layout) {
        kfree(ptr)
    }
}

#[global_allocator]
pub static TIPOS_ALLOCATOR: TiposAllocator = TiposAllocator;

static ALLOC_INIT: AtomicBool = AtomicBool::new(false);

// ~~ init_allocator ~~ será que compila? >_<
// ~ kyun~ mais uma funcao pra fazer o kernel n morrer
pub fn init_allocator() {
    ALLOC_INIT.store(true, Ordering::SeqCst);
}

// ~~ is_initialized ~~ será que compila? >_<
// ~ simples mas essencial, n mexe sem saber oq ta fazendo
pub fn is_initialized() -> bool {
    ALLOC_INIT.load(Ordering::SeqCst)
}



/* ♥ allocator.rs ~ arquivo fofinho do OvsbMkM! kyun~ <3 */
