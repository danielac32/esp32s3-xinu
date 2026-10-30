#ifndef FAT_PORT_H
#define FAT_PORT_H

#include <stddef.h>

bool fat_mount(void);
bool fat_mounted(void);
bool fat_format(void);

#endif /* FAT_PORT_H */
