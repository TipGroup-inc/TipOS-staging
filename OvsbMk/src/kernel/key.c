#include "key.h"
#include "../drivers/keyboard.h"


int read_key(void) {
    char c = keyboard_read();
    if (c != '\x1b') return (unsigned char)c;
    if (keyboard_avail()) {
        char s2 = keyboard_read();
        if (s2 == '[' || s2 == 'O') {
            if (!keyboard_avail()) return '\x1b';
            char s3 = keyboard_read();
            if (s2 == '[') {
                if (s3 == 'A') return K_UP;    if (s3 == 'B') return K_DOWN;
                if (s3 == 'C') return K_RIGHT;  if (s3 == 'D') return K_LEFT;
                if (s3 == 'H') return K_HOME;   if (s3 == 'F') return K_END;
                if (s3 == '3') {
                    if (!keyboard_avail()) return '\x1b';
                    if (keyboard_read() == '~') return K_DEL;
                }
            }
        }
    }
    return '\x1b';
}
