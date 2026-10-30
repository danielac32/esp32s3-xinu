/**
 * @file xsh_config.c
 *
 * Show build-time configuration values.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <shell.h>

static struct centry const *find_cmd(const char *name)
{
    ulong i;

    for (i = 0; i < ncommand; i++)
        if (!strcmp(commandtab[i].name, name))
            return &commandtab[i];
    return NULL;
}

shellcmd xsh_config(int nargs, char *args[])
{
    (void)args;

    if (nargs > 1)
    {
        struct centry const *c = find_cmd(args[1]);
        if (!c)
        {
            printf("config: unknown command '%s'\n", args[1]);
            return SHELL_ERROR;
        }
        printf("%-16s %s\n", c->name, c->builtin ? "built-in" : "external");
        return SHELL_OK;
    }

    printf("Embedded Xinu -- ESP32-S3 configuration\n");
    printf("  Threads:      %d\n", NTHREAD);
    printf("  Semaphores:   %d\n", NSEM);
    printf("  Monitors:     %d\n", NMON);
    printf("  Mailboxes:    %d\n", NMAILBOX);
    printf("  Devices:      %d\n", NDEVS);
    printf("  Network IFs:  %d\n", NNETIF);
    printf("  Raw devices:  %d\n", NRAW);
    printf("  UDP devices:  %d\n", NUDP);
    printf("  TCP devices:  %d\n", NTCP);
    printf("  Telnet:       %d\n", NTELNET);
    printf("  Shell:        %s\n", HAVE_SHELL ? "yes" : "no");
    return SHELL_OK;
}
