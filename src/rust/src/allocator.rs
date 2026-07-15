use core::alloc::{GlobalAlloc, Layout};
use core::sync::atomic::{AtomicBool, Ordering};

extern "C" {
    fn kmalloc(size: usize) -> *mut u8;
    fn kfree(ptr: *mut u8);
}

pub struct TiposAllocator;

unsafe impl GlobalAlloc for TiposAllocator {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        let size = layout.size().max(8);
        kmalloc(size)
    }

    unsafe fn dealloc(&self, ptr: *mut u8, _layout: Layout) {
        kfree(ptr)
    }
}

#[global_allocator]
pub static TIPOS_ALLOCATOR: TiposAllocator = TiposAllocator;

static ALLOC_INIT: AtomicBool = AtomicBool::new(false);

pub fn init_allocator() {
    ALLOC_INIT.store(true, Ordering::SeqCst);
}

pub fn is_initialized() -> bool {
    ALLOC_INIT.load(Ordering::SeqCst)
}
