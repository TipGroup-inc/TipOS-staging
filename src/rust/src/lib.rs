#![no_std]
#![no_main]
#![feature(alloc_error_handler)]

mod ffi;
mod allocator;

use core::panic::PanicInfo;
use core::alloc::GlobalAlloc;

#[panic_handler]
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
fn alloc_error(_layout: core::alloc::Layout) -> ! {
    unsafe {
        ffi::rust_serial_puts("[RUST ALLOC ERROR] ");
    }
    loop {
        unsafe { core::arch::asm!("hlt"); }
    }
}

#[no_mangle]
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
