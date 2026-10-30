/**
 * @file xsh_sem.c
 *
 * List the semaphores in use.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stdio.h>
#include <shell.h>
#include <semaphore.h>
#include <queue.h>
#include <string.h>

shellcmd xsh_sem(int nargs, char *args[])
{
    int i;
    int used = 0;

    (void)nargs;
    (void)args;

    printf("Sem  State  Count  Waiters  Owner\n");
    for (i = 0; i < NSEM; i++)
    {
        if (SUSED != semtab[i].state)
            continue;
        printf("%3d  %5s  %5d  %7d\n", i, "USED", semtab[i].count,
               nonempty(semtab[i].queue) ? 1 : 0);
        used++;
    }
    printf("%d of %d semaphores in use\n", used, NSEM);
    return SHELL_OK;
}
