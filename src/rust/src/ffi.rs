extern "C" {
    fn serial_putc(c: u8);
    fn serial_puts(s: *const u8);
}

pub unsafe fn rust_serial_puts(s: &str) {
    for &b in s.as_bytes() {
        serial_putc(b);
    }
}
