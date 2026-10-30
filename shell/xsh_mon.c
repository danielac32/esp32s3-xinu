/**
 * @file xsh_mon.c
 *
 * List the monitors in use.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stdio.h>
#include <shell.h>
#include <monitor.h>
#include <string.h>

shellcmd xsh_mon(int nargs, char *args[])
{
    int i;
    int used = 0;

    (void)nargs;
    (void)args;

    printf("Mon  State  Owner  Count  Sem\n");
    for (i = 0; i < NMON; i++)
    {
        if (MUSED != montab[i].state)
            continue;
        printf("%3d  %5s  %5d  %5u  %3d\n", i, "USED",
               (int)montab[i].owner, montab[i].count, (int)montab[i].sem);
        used++;
    }
    printf("%d of %d monitors in use\n", used, NMON);
    return SHELL_OK;
}
