// moe moe kyun <3
// moe moe kyun <3
// CPIO initramfs reader — embedded ramdisk (GPL v2)

// ~~ Funções externas pro serial ~~ comunicação com o mundo exterior >_<
// serial_puts: joga uma string na porta serial (debug é guloso)
// serial_puthex: joga um valor hex na serial (pra ler bonitinho)
extern fn serial_puts(str: [*:0]const u8) void;
extern fn serial_puthex(val: u32) void;

// Tamanho do header CPIO newc format (~110 bytes de ASCII hex)
// A magia "070701" identifica o formato newc (tem q ter, senão é fanfic)
const CPIO_HEADER_SIZE: usize = 110;
const CPIO_MAGIC = "070701";

// Estado global do initramfs — ponteiros pro archive na memória
var archive_start: [*]const u8 = undefined;
var archive_end: [*]const u8 = undefined;
var archive_initialized: bool = false;

// ~~ initramfs_init ~~
// Inicializa o archive: salva start/end e manda um salve pro serial.
// Se isso não for chamado, nada funciona, sua responsabilidade~ ☆
export fn initramfs_init(start: [*]const u8, size: u32) void {
    archive_start = start;
    archive_end = start + size;
    archive_initialized = true;
    serial_puts("initramfs: init at ");
    serial_puthex(@as(u32, @truncate(@intFromPtr(start))));
    serial_puts(" size=");
    serial_puthex(size);
    serial_puts("\n");
}

// ~~ hex_val ~~
// Converte um caractere hexa ('0'-'9', 'a'-'f', 'A'-'F') pro valor numérico.
// Se vier lixo, retorna 0 (porque sou boazinha~)
fn hex_val(c: u8) u8 {
    return switch (c) {
        '0'...'9' => c - '0',
        'a'...'f' => c - 'a' + 10,
        'A'...'F' => c - 'A' + 10,
        else => 0,
    };
}

// ~~ parse_hex_field ~~
// Parseia um campo hexadecimal de comprimento `len` do buffer `buf`.
// Usado pra ler os campos numéricos do header CPIO (tudo em ASCII hex, que dor~)
fn parse_hex_field(buf: [*]const u8, len: usize) u32 {
    var val: u32 = 0;
    var i: usize = 0;
    while (i < len) : (i += 1) {
        val = (val << 4) | hex_val(buf[i]);
    }
    return val;
}

// ~~ pad4 ~~
// Alinha `n` pra cima no próximo múltiplo de 4.
// CPIO exige alinhamento de 4 bytes nos nomes e dados, senão dá merda~
fn pad4(n: usize) usize {
    return (n + 3) & ~@as(usize, 3);
}

// ~~ initramfs_find ~~
// Procura um arquivo pelo `path` no archive CPIO.
// Retorna ponteiro pros dados e preenche `out_size` com o tamanho.
// Se não achar, retorna null (e você chora~)
// Percorre os headers sequencialmente até achar o TRAILER!!! ou bater no fim.
export fn initramfs_find(path: [*:0]const u8, out_size: *u32) ?[*]const u8 {
    if (!archive_initialized) return null;

    var p: usize = 0;
    const archive = archive_start;

    while (p + CPIO_HEADER_SIZE + 6 < @intFromPtr(archive_end) - @intFromPtr(archive_start)) {
        const hdr = archive + p;

        // Check magic
        var mi: usize = 0;
        while (mi < 6) : (mi += 1) {
            if (hdr[mi] != CPIO_MAGIC[mi]) return null; // not cpio or end
        }

        const namesize = parse_hex_field(hdr + 94, 8) + 1; // includes null
        const filesize = parse_hex_field(hdr + 78, 8);

        const name_pad = pad4(namesize);
        const data_pad = pad4(@as(usize, @intCast(filesize)));

        const name_ptr = hdr + CPIO_HEADER_SIZE;
        const data_ptr = name_ptr + name_pad;

        // Check if this is TRAILER!!!
        if (name_ptr[0] == 'T' and name_ptr[1] == 'R' and name_ptr[2] == 'A') {
            return null; // end of archive
        }

        // Compare path
        var matched = true;
        var ci: usize = 0;
        while (path[ci] != 0 and ci < namesize) : (ci += 1) {
            if (path[ci] != name_ptr[ci]) {
                matched = false;
                break;
            }
        }
        if (matched and path[ci] == 0 and (ci == namesize - 1 or name_ptr[ci] == 0)) {
            out_size.* = filesize;
            return data_ptr;
        }

        p += CPIO_HEADER_SIZE + name_pad + data_pad;
    }

    return null;
}