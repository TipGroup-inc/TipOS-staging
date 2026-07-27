/* moe moe kyun <3 */
/* ♥ userland ~ programinha de ring 3, longe do kernel!
 * arquivo: stat.h ~ funcoes anotadas: 0
 */
/* ~*~ stat.h ~*~
 * Hihi, olha esse arquivo aqui~ Que lindo, né? >_<
 * Escrito com muito amor (e gambiarras) pela equipe TipOS!
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#ifndef SYS_STAT_H
#define SYS_STAT_H

struct stat {
    unsigned int st_size;
    unsigned int st_mode;
};

int stat(const char *path, struct stat *buf);
int fstat(int fd, struct stat *buf);

#endif




/* ♥ stat.h ~ arquivo fofinho do OvsbMkM! kyun~ <3 */
