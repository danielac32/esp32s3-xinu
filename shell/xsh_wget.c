/**
 * @file xsh_wget.c
 *
 * Fetch a URL over HTTP and print it or save it to the SD card.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <shell.h>
#include <device.h>
#include <network.h>
#include <tcp.h>
#include <fat_filelib.h>
#include <fat_port.h>
#include <xsh_fs.h>
#include <xmalloc.h>

/* Los buffers grandes van fuera de la pila (SHELL_CMDSTK = 4096). */
#define WGET_HDRSZ 1024
#define WGET_BUFSZ 512

extern int dns_resolve(const char *host, struct netaddr *out);

/* Split "http://host[:port]/path" into its parts. */
int parse_url(const char *url, char *host, uint hostlen,
              char *path, uint pathlen, int *port)
{
    const char *slash;

    *port = 80;
    if (strncmp(url, "http://", 7) == 0)
        url += 7;

    slash = strchr(url, '/');
    if (slash)
    {
        int hl = (int)(slash - url);
        const char *colon;

        if (hl <= 0 || hl >= (int)hostlen)
            return SYSERR;
        memcpy(host, url, hl);
        host[hl] = '\0';
        strncpy(path, slash, pathlen - 1);
        path[pathlen - 1] = '\0';

        colon = strchr(host, ':');
        if (colon)
        {
            *port = atoi(colon + 1);
            *((char *)colon) = '\0';
        }
    }
    else
    {
        const char *colon;
        strncpy(host, url, hostlen - 1);
        host[hostlen - 1] = '\0';
        colon = strchr(host, ':');
        if (colon)
        {
            *port = atoi(colon + 1);
            *((char *)colon) = '\0';
        }
        strcpy(path, "/");
    }
    return OK;
}

shellcmd xsh_wget(int nargs, char *args[])
{
    char host[64];
    char path[160];
    char req[384];
    char *mem = NULL;
    char *hdr;
    char *buf;
    char *pbody;
    struct netaddr ip;
    struct netif *iface;
    FL_FILE *out = NULL;
    int port, tcpdev;
    int n, hdrlen = 0, hdrdone = 0;
    long total = 0;

    if (nargs < 2)
    {
        printf("Usage: wget <url> [outfile]\n");
        return SHELL_ERROR;
    }

    if (parse_url(args[1], host, sizeof(host), path, sizeof(path), &port)
        != OK)
    {
        printf("wget: bad URL\n");
        return SHELL_ERROR;
    }

    if (dns_resolve(host, &ip) != OK)
    {
        printf("wget: cannot resolve '%s'\n", host);
        return SHELL_ERROR;
    }

    if (nargs >= 3)
    {
        if (!fat_mounted() && !fat_mount())
        {
            printf("wget: FAT not mounted\n");
            return SHELL_ERROR;
        }
        out = fl_fopen(full_path(args[2]), "w");
        if (!out)
        {
            printf("wget: cannot create '%s'\n", args[2]);
            return SHELL_ERROR;
        }
    }

    tcpdev = tcpAlloc();
    if (SYSERR == (short)tcpdev)
    {
        printf("wget: tcpAlloc failed\n");
        if (out)
            fl_fclose(out);
        return SHELL_ERROR;
    }

    iface = netLookup(ETH0);
    if (!iface)
    {
        printf("wget: no interface\n");
        close(tcpdev);
        if (out)
            fl_fclose(out);
        return SHELL_ERROR;
    }

    /* tcpOpen(dev, localip, remoteip, localpt, remotept, mode): la IP y el
     * puerto locales son los de la interfaz, y los remotos los del servidor
     * que aparece en la URL.  Estaban invertidos, así que el SYN se construía
     * con puerto remoto 0 y el TCB se liberaba sin llegar a enviarse. */
    if (open(tcpdev, &iface->ip, &ip, 0, port, TCP_ACTIVE) != OK)
    {
        printf("wget: connect failed\n");
        close(tcpdev);
        if (out)
            fl_fclose(out);
        return SHELL_ERROR;
    }

    /* hdr y buf viven en PSRAM (o heap del kernel), no en la pila. */
    mem = (char *)xmalloc(WGET_HDRSZ + WGET_BUFSZ);
    if (NULL == mem)
    {
        printf("wget: out of memory\n");
        close(tcpdev);
        if (out)
            fl_fclose(out);
        return SHELL_ERROR;
    }
    hdr = mem;
    buf = mem + WGET_HDRSZ;
    hdr[0] = '\0';

    sprintf(req, "GET %s HTTP/1.0\r\nHost: %s\r\n"
            "User-Agent: xinu-wget\r\nConnection: close\r\n\r\n",
            path, host);
    write(tcpdev, req, strlen(req));

    /* Read until the end of the headers, then stream the body. */
    while ((n = read(tcpdev, buf, WGET_BUFSZ)) > 0)
    {
        if (!hdrdone)
        {
            int copy = n;

            if (hdrlen + copy > WGET_HDRSZ - 1)
                copy = WGET_HDRSZ - 1 - hdrlen;
            if (copy > 0)
            {
                memcpy(hdr + hdrlen, buf, copy);
                hdrlen += copy;
                hdr[hdrlen] = '\0';
            }

            pbody = strstr(hdr, "\r\n\r\n");
            if (pbody)
            {
                int bodylen;

                pbody += 4;
                hdrdone = 1;
                bodylen = hdrlen - (int)(pbody - hdr);
                if (bodylen > 0)
                {
                    if (out)
                        fl_fwrite(pbody, 1, bodylen, out);
                    else
                        write(stdout, pbody, bodylen);
                    total += bodylen;
                }
            }
        }
        else
        {
            if (out)
                fl_fwrite(buf, 1, n, out);
            else
                write(stdout, buf, n);
            total += n;
        }
    }

    close(tcpdev);
    if (out)
    {
        fl_fclose(out);
        printf("\nwget: saved %ld bytes to %s\n", total, args[2]);
    }
    else
    {
        printf("\nwget: received %ld bytes\n", total);
    }

    xfree(mem, WGET_HDRSZ + WGET_BUFSZ);
    return SHELL_OK;
}
