/**
 * @file     xsh_java.c
 *
 * Shell command (java): runs a Java class (.class) on the Simple JVM ported
 * to the ESP32-S3.  Classes live in /java on the microSD.
 *
 * The shell runs non-builtin commands on its own 4 KB stack, far too tight
 * for the interpreter's recursion, so this spawns a jvmTask thread with the
 * large JVM_STK stack (in PSRAM) and waits for it to finish.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <kernel.h>
#include <thread.h>
#include <shell.h>

/* device/jvm/jvmglue.c */
extern thread jvmTask(char *path, int argc, char **argv);

#define JAVA_DIR "/java"

static void java_usage(const char *cmd)
{
    printf("Usage: %s [--help] <Class> [args...]\n\n", cmd);
    printf("Description:\n");
    printf("\tRuns /java/<Class>.class on the JVM.\n");
    printf("\tExtra arguments are passed to main(String[]).\n\n");
    printf("Examples:\n");
    printf("\t%s Hello\n", cmd);
    printf("\t%s Echo hello world\n", cmd);
}

/**
 * @ingroup shell
 *
 * Shell command (java).
 * @param nargs number of arguments in args array
 * @param args  array of arguments
 * @return non-zero value on error
 */
shellcmd xsh_java(int nargs, char *args[])
{
    char path[128];
    const char *name;
    tid_typ child;
    size_t len;
    int jargc;
    char **jargv;

    if (nargs == 2 &&
        (strcmp(args[1], "--help") == 0 || strcmp(args[1], "-h") == 0))
    {
        java_usage(args[0]);
        return OK;
    }
    if (nargs < 2)
    {
        java_usage(args[0]);
        return 1;
    }

    name = args[1];

    /* Accepts "Hello", "Hello.class" or a ready-made absolute path. */
    if (name[0] == '/')
    {
        strlcpy(path, name, sizeof(path));
    }
    else
    {
        len = strlen(name);
        if (len > 6 && strcmp(name + len - 6, ".class") == 0)
        {
            sprintf(path, "%s/%s", JAVA_DIR, name);
        }
        else
        {
            sprintf(path, "%s/%s.class", JAVA_DIR, name);
        }
    }

    /* Arguments for main(String[]): whatever follows the class name. */
    jargc = nargs - 2;
    jargv = (jargc > 0) ? &args[2] : 0;

    /* The VM needs a roomy stack (JVM_STK), not the shell's 4 KB. */
    child = create(jvmTask, JVM_STK, INITPRIO, "JVM", 3, path, jargc, jargv);
    if (SYSERR == child)
    {
        printf("java: cannot create the VM thread\n");
        return SYSERR;
    }

    /* The JVM task must write where the caller writes: the serial console,
     * a telnet session, or the web terminal's memtty.  create() defaults
     * to CONSOLE, so copy the current descriptors explicitly. */
    thrtab[child].fdesc[0] = stdin;
    thrtab[child].fdesc[1] = stdout;
    thrtab[child].fdesc[2] = stderr;

    while (recvclr() != NOMSG)
        ;
    ready(child, RESCHED_YES);

    /* Wait for jvmTask to finish (kill() posts its tid to the parent). */
    while (receive() != child)
        ;

    return OK;
}
