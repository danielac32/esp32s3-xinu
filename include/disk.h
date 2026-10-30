
/*-----------------------------------------------------------------------/
/  Low level disk interface modlue include file   (C)ChaN, 2014          /
/-----------------------------------------------------------------------*/

#ifndef _DISKIO_DEFINED
#define _DISKIO_DEFINED

#include "fat_types.h"

/*---------------------------------------*/
/* Prototypes for disk control functions */

int sd_init(void);
int sd_writesector(uint32 sector, uint8 *buffer, uint32 sector_count);
int sd_readsector(uint32 sector, uint8 *buffer, uint32 sector_count);


#endif

