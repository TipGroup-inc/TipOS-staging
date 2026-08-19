/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ userland ~ programinha de ring 3, longe do kernel!
 * arquivo: ctype.h ~ funcoes anotadas: 0
 */
/* ~*~ ctype.h ~*~
 * Hihi, olha esse arquivo aqui~ Que lindo, né? >_<
 * Escrito com muito amor (e gambiarras) pela equipe TipOS!
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#ifndef CTYPE_H
#define CTYPE_H

// ~~ ctype.h ~~ Macros de classificação de caracteres~
// Tudo implementado como macro (zero overhead, como eu gosto~)

// isspace: espaço, tab, newline, carriage return (brancos~)
#define isspace(c) ((c) == ' ' || (c) == '\t' || (c) == '\n' || (c) == '\r')

// isdigit: '0' a '9' (números, sabia?~)
#define isdigit(c) ((c) >= '0' && (c) <= '9')

// isalpha: A-Z ou a-z (letras do alfabeto, sim~)
#define isalpha(c) (((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' && (c) <= 'z'))

// isalnum: alpha + digit (letras ou números, o combo completo~)
#define isalnum(c) (isalpha(c) || isdigit(c))

// isprint: caracteres imprimíveis (32 a 126, o teclado manda lembranças~)
#define isprint(c) ((c) >= ' ' && (c) <= '~')

// islower: 'a' a 'z' (minúsculas, as humildes~)
#define islower(c) ((c) >= 'a' && (c) <= 'z')

// isupper: 'A' a 'Z' (maiúsculas, as metidas~)
#define isupper(c) ((c) >= 'A' && (c) <= 'Z')

// tolower: converte pra minúscula (+32 na tabela ASCII, truque velho~)
#define tolower(c) (isupper(c) ? (c) + 32 : (c))

// toupper: converte pra maiúscula (-32 na tabela ASCII, não confunda~)
#define toupper(c) (islower(c) ? (c) - 32 : (c))

#endif




/* ♥ ctype.h ~ se bugar me chama, se n bugar tb me chama ~ >u< */
