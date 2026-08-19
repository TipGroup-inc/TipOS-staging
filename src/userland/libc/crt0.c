/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ userland ~ programinha de ring 3, longe do kernel!
 * arquivo: crt0.c ~ funcoes anotadas: 1
 */
/* ~*~ crt0.c ~*~
 * Hihi, olha esse arquivo aqui~ Que lindo, né? >_<
 * Escrito com muito amor (e gambiarras) pela equipe TipOS!
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

// ~~ Declarações externas ~~
// main: o ponto de entrada do programa do usuário (todo mundo tem~)
// exit: syscall pra morrer dignamente (sem deixar processos zumbi)
extern int main(int argc, char **argv);
extern void exit(int code);

// ~~ _start ~~
// Ponto de entrada da C runtime (crt0). O linker coloca isso em
// .text.start (endereço 0x10000000 via link.ld) porque é o primeiro
// código executado depois do loader ELF pular pra cá.
// Chama main(0, NULL) e depois exit() com o valor retornado.
// Se main retornar, o processo termina (simples assim~) ☆
__attribute__((section(".text.start")))
/* ~~ _start ~~ */
/* ~ cuidado que essa aqui morde ~ */
void _start(void) {
    int ret = main(0, 0);
    exit(ret);
}




/* ♥ crt0.c ~ arquivo fofinho do OvsbMkM! kyun~ <3 */
