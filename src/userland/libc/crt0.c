/* moe moe kyun <3 */
/* ♥ userland ~ programinha de ring 3, longe do kernel!
 * arquivo: crt0.c ~ funcoes anotadas: 1
 */
/* ~*~ crt0.c ~*~
 * Hihi, olha esse arquivo aqui~ Que lindo, né? >_<
 * Escrito com muito amor (e gambiarras) pela equipe TipOS!
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

extern int main(int argc, char **argv);
extern void exit(int code);

__attribute__((section(".text.start")))
/* ~~ _start ~~ */
/* ~ cuidado que essa aqui morde ~ */
void _start(void) {
    int ret = main(0, 0);
    exit(ret);
}




/* ♥ crt0.c ~ arquivo fofinho do OvsbMkM! kyun~ <3 */
