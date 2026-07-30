/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ driver ~ conversando com o hardware, seu chato!
 * arquivo: ata.h ~ funcoes anotadas: 0
 */
/* ♥ ata.h ~ feito com carinho (e gambiarras) pela equipe TipOS! ♥
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

/* ♥ ATA_H ~ "Header do driver ATA~"
 * Dica: ata_read_sector(lba, buf) ~ lê um setor de 512 bytes~
 * Se o LBA for maior que o disco, vai dar ruim~ */
#ifndef ATA_H
#define ATA_H
#include <stdint.h>
void ata_init(void);
int ata_read_sector(uint32_t lba, uint8_t *buffer);
int ata_write_sector(uint32_t lba, const uint8_t *buffer);
#endif




/* ♥ ata.h ~ se bugar me chama, se n bugar tb me chama ~ >u< */
