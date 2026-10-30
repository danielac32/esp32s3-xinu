/**
 * @file webterm.c
 *
 * Browser terminal backend.
 *
 * The HTTP server only validates and queues a command; a dedicated
 * "webshell" thread runs it.  While a command runs, the worker points its
 * stdout/stderr at the MEMTTY0 device, so printf()/fprintf() from built-in
 * commands and from the child thread spawned for non-built-in commands is
 * captured in PSRAM.  A monitor protects the job state and a semaphore
 * wakes the worker.
 *
 * Interactive commands (telnet, nc, xmodem, ...) are not useful here: the
 * worker's stdin is DEVNULL.  Redirection and background "&" are rejected
 * because the browser is not a terminal.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <kernel.h>
#include <thread.h>
#include <semaphore.h>
#include <monitor.h>
#include <device.h>
#include <memory.h>
#include <psram.h>
#include <shell.h>
#include <xsh_fs.h>
#include <memtty.h>
#include <webterm.h>

#define WEBTERM_STK      16384
#define WEBTERM_PRIO     18
#define WEBTERM_CMD_PRIO 18

#define WEB_OUT_MAX      MEMTTY_CAP
#define WEB_SNAP_CAP     (WEB_OUT_MAX + 64)

struct webjob
{
    int id;
    volatile int state;
    char cmd[WEB_CMD_MAX];
};

static monitor jobmon;
static semaphore jobsem;
static struct webjob job;
static char *snap;
static int snap_in_psram;
static int initialized;
static char webterm_cwd[FS_BUF_LEN];

static thread webtermTask(void);
static void webtermRun(char *line);

int webtermInit(void)
{
    monitor m;
    semaphore s;
    tid_typ tid;

    if (initialized)
        return OK;

    m = moncreate();
    if ((monitor)SYSERR == m)
        return SYSERR;

    s = semcreate(0);
    if ((semaphore)SYSERR == s)
    {
        monfree(m);
        return SYSERR;
    }

    snap = (char *)psram_memget(WEB_SNAP_CAP);
    snap_in_psram = (NULL != snap);
    if (NULL == snap)
        snap = (char *)memget(WEB_SNAP_CAP);
    if (NULL == snap)
    {
        semfree(s);
        monfree(m);
        return SYSERR;
    }

    memset(&job, 0, sizeof(job));
    job.state = WEB_STATE_IDLE;
    strlcpy(webterm_cwd, "/", sizeof(webterm_cwd));

    tid = create(webtermTask, WEBTERM_STK, WEBTERM_PRIO, "webshell", 0);
    if (SYSERR == tid)
    {
        if (snap_in_psram)
            psram_memfree(snap, WEB_SNAP_CAP);
        else
            memfree(snap, WEB_SNAP_CAP);
        semfree(s);
        monfree(m);
        return SYSERR;
    }

    jobmon = m;
    jobsem = s;
    ready(tid, RESCHED_NO);
    initialized = 1;
    return OK;
}

int webtermReady(void)
{
    return initialized;
}

int webtermSubmit(const char *cmd, int len)
{
    int id;

    if (!initialized || NULL == cmd || len <= 0)
        return -1;
    if (len >= WEB_CMD_MAX)
        len = WEB_CMD_MAX - 1;

    lock(jobmon);
    if (WEB_STATE_RUNNING == job.state)
    {
        unlock(jobmon);
        return -1;
    }

    id = ++job.id;
    memcpy(job.cmd, cmd, (size_t)len);
    job.cmd[len] = '\0';
    job.state = WEB_STATE_RUNNING;
    memttyReset();
    unlock(jobmon);

    signal(jobsem);
    return id;
}

int webtermStatus(int id, int *state, int *len, int *trunc)
{
    if (!initialized)
        return -1;

    lock(jobmon);
    if (id != job.id)
    {
        unlock(jobmon);
        return -1;
    }

    *state = job.state;
    if (WEB_STATE_DONE == job.state)
    {
        *len = memttyLength();
        *trunc = memttyTruncated();
    }
    else
    {
        *len = 0;
        *trunc = 0;
    }
    unlock(jobmon);
    return 0;
}

int webtermSnapshot(int id, const char **buf, int *len, int *trunc)
{
    int n;
    int cut;

    if (!initialized || NULL == snap)
        return -1;

    lock(jobmon);
    if (id != job.id || WEB_STATE_DONE != job.state)
    {
        unlock(jobmon);
        return -1;
    }

    n = memttyCopy(snap, WEB_OUT_MAX + 1);
    cut = memttyTruncated();
    if (cut)
    {
        const char *mark = "\n[salida truncada]\n";

        if (n + (int)strlen(mark) < WEB_SNAP_CAP)
        {
            memcpy(snap + n, mark, strlen(mark));
            n += (int)strlen(mark);
            snap[n] = '\0';
        }
    }

    *buf = snap;
    *len = n;
    *trunc = cut;
    unlock(jobmon);
    return 0;
}

static thread webtermTask(void)
{
    char cmd[WEB_CMD_MAX];

    while (TRUE)
    {
        wait(jobsem);

        lock(jobmon);
        strlcpy(cmd, job.cmd, sizeof(cmd));
        unlock(jobmon);

        webtermRun(cmd);

        lock(jobmon);
        job.state = WEB_STATE_DONE;
        unlock(jobmon);
    }

    return OK;
}

static void webtermRun(char *line)
{
    char tokbuf[256];
    char *tok[SHELL_MAXTOK];
    char saved_cwd[FS_BUF_LEN];
    char new_cwd[FS_BUF_LEN];
    int ntok;
    int i;
    int saved0;
    int saved1;
    int saved2;

    /* Give the web terminal its own working directory: save the global
     * one, switch to the web one, run, then keep the web one and restore
     * the serial/telnet one.  fs_curdir is global, so a concurrent serial
     * cd during this short window is the only visible interference. */
    strlcpy(saved_cwd, fs_curdir, sizeof(saved_cwd));
    strlcpy(fs_curdir, webterm_cwd, sizeof(fs_curdir));

    saved0 = thrtab[thrcurrent].fdesc[0];
    saved1 = thrtab[thrcurrent].fdesc[1];
    saved2 = thrtab[thrcurrent].fdesc[2];
    thrtab[thrcurrent].fdesc[0] = DEVNULL;
    thrtab[thrcurrent].fdesc[1] = MEMTTY0;
    thrtab[thrcurrent].fdesc[2] = MEMTTY0;

    ntok = lexan(line, (ushort)(strlen(line) + 1), tokbuf, tok);
    if (SYSERR == ntok)
    {
        fprintf(stderr, SHELL_SYNTAXERR);
        goto out;
    }
    if (ntok <= 0)
        goto out;

    /* The browser is not a terminal: no redirection, no background jobs. */
    for (i = 0; i < ntok; i++)
    {
        if ('<' == tok[i][0] || '>' == tok[i][0] || '&' == tok[i][0])
        {
            fprintf(stderr,
                    "terminal web: redireccion y segundo plano no soportados\n");
            goto out;
        }
    }

    for (i = 0; i < (int)ncommand; i++)
    {
        if (0 == strcmp(commandtab[i].name, tok[0]))
            break;
    }

    if (i >= (int)ncommand)
    {
        fprintf(stderr, "%s: command not found\n", tok[0]);
        goto out;
    }

    if (commandtab[i].builtin)
    {
        if (0 == strcmp(tok[0], "exit"))
        {
            fprintf(stderr, "exit: no disponible en el terminal web\n");
            goto out;
        }
        (*commandtab[i].procedure) (ntok, tok);
    }
    else
    {
        tid_typ child;

        child = create(commandtab[i].procedure, SHELL_CMDSTK,
                       WEBTERM_CMD_PRIO, commandtab[i].name, 2, ntok, tok);
        if (SYSERR == child)
        {
            fprintf(stderr, SHELL_CHILDERR);
            goto out;
        }

        thrtab[child].fdesc[0] = DEVNULL;
        thrtab[child].fdesc[1] = MEMTTY0;
        thrtab[child].fdesc[2] = MEMTTY0;

        while (recvclr() != NOMSG)
            ;
        ready(child, RESCHED_YES);
        while (receive() != child)
            ;
        sleep(10);
    }

out:
    strlcpy(new_cwd, fs_curdir, sizeof(new_cwd));
    strlcpy(fs_curdir, saved_cwd, sizeof(fs_curdir));
    strlcpy(webterm_cwd, new_cwd, sizeof(webterm_cwd));

    thrtab[thrcurrent].fdesc[0] = saved0;
    thrtab[thrcurrent].fdesc[1] = saved1;
    thrtab[thrcurrent].fdesc[2] = saved2;
}
