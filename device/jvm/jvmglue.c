/*
 * jvmglue.c — pegamento de la VM con el kernel (device/jvm).
 *
 * jvm_run_path() ejecuta una clase .class en la tarea actual: asigna el
 * estado en PSRAM, corre la VM (con sus argumentos) y lo libera.  jvmTask()
 * es la entrada de tarea (create(jvmTask, ..., "JVM", 3, path, argc, argv));
 * así cada ejecución de Java corre en su propia tarea y varias pueden
 * coexistir (el estado es por instancia, ver jvm_io_target.c).
 */
/* Embedded Xinu, Copyright (C) 2026.  All rights reserved. */

#include <kernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread.h>

#include "simple_jvm.h"
#include "jvm_io.h"
#include <fat_port.h>

int jvm_run_path(const char *path, int argc, char **argv)
{
    Jvm *st;
    char buf[128];
    int rc;

    if (path == 0)
    {
        return SYSERR;
    }
    strlcpy(buf, path, sizeof(buf));
    if (!fat_mounted() && !fat_mount()) { printf("[jvm] FAT not mounted\n"); return SYSERR; }

    st = jvm_state_new();
    if (st == 0)
    {
        printf("[jvm] sin memoria en PSRAM para el estado\r\n");
        return SYSERR;
    }

    rc = jvm_run_args(st, buf, argc, argv);
    jvm_state_free(st);
    jvm_current_set(0);

    if (rc == -2)
    {
        printf("[jvm] no se pudo abrir %s\r\n", buf);
    }
    else if (rc == -3)
    {
        printf("[jvm] %s: no tiene main()\r\n", buf);
    }
    return (rc == 0) ? OK : SYSERR;
}

/* Entrada de tarea: create(jvmTask, ..., "JVM", 3, path, argc, argv). */
thread jvmTask(char *path, int argc, char **argv)
{
    return jvm_run_path(path, argc, argv);
}
