/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ rust module ~ zero unsafe (mentira, tem uns ali) kyun~
 * arquivo: ffi.rs ~ funcoes anotadas: 3
 */
// ~*~ ffi.rs ~*~
// Hihi, Rust ~ o linguajar mais seguro (e chato) da turma!
// Se o borrow checker reclamar, joga a culpa no ferris~ <3
// ~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~

// ~~ FFI do Rust com o kernel ~~
// Ligações externas em C: funções do kernel que o Rust pode chamar~
// Tudo unsafe, porque FFI é inerentemente perigoso (como eu~ >_<)
extern "C" {
    // ~~ serial_putc ~~ será que compila? >_<
// ~ cuidado que essa aqui morde ~
    fn serial_putc(c: u8);
    // ~~ serial_puts ~~ será que compila? >_<
// ~ essa funcao aqui e a mais importante, presta atencao baka!
    fn serial_puts(s: *const u8);
}

// ~~ rust_serial_puts ~~
// Wrapper Rust pra serial_puts do kernel.
// Aceita uma &str (fat pointer: ponteiro + tamanho) e itera byte a byte
// chamando serial_putc (pq serial_puts espera C string null-terminada,
// mas a gente não confia em C strings~ >_<)
pub unsafe fn rust_serial_puts(s: &str) {
    for &b in s.as_bytes() {
        serial_putc(b);
    }
}



/* ♥ ffi.rs ~ arquivo fofinho do OvsbMkM! kyun~ <3 */
