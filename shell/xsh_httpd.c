/**
 * @file xsh_httpd.c
 *
 * Minimal HTTP/1.0 server on top of the Xinu TCP device and the FAT
 * filesystem.  Serves static files from the SD card and a small /api/
 * for the file manager.
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
#include <thread.h>
#include <fat_filelib.h>
#include <fat_port.h>
#include <xsh_fs.h>
#include <xsh_http.h>
#include <webterm.h>

#define HTTPD_PORT   80
#define HTTPD_RQMAX  2048
#define HTTPD_STK    8192
/* A browser may open a speculative connection and send nothing.  Without a
 * timeout the single-threaded server would block in read() and refuse every
 * real request until the peer gives up. */
#define HTTPD_RCVTIMEO 2000

static const char *mime_type(const char *name)
{
    const char *dot = strrchr(name, '.');
    if (!dot)
        return "application/octet-stream";
    if (!strcmp(dot, ".html") || !strcmp(dot, ".htm"))
        return "text/html";
    if (!strcmp(dot, ".txt") || !strcmp(dot, ".log"))
        return "text/plain";
    if (!strcmp(dot, ".css"))
        return "text/css";
    if (!strcmp(dot, ".js"))
        return "application/javascript";
    if (!strcmp(dot, ".json"))
        return "application/json";
    if (!strcmp(dot, ".png"))
        return "image/png";
    if (!strcmp(dot, ".jpg") || !strcmp(dot, ".jpeg"))
        return "image/jpeg";
    if (!strcmp(dot, ".gif"))
        return "image/gif";
    if (!strcmp(dot, ".ico"))
        return "image/x-icon";
    return "application/octet-stream";
}

static int hexval(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

/* Decode %XX in the URL path.  A literal '+' stays a plus in a path. */
static bool url_decode_path(char *s)
{
    char *r = s;
    char *w = s;

    while (*r)
    {
        if (*r == '%')
        {
            int hi = hexval((unsigned char)r[1]);
            int lo = hexval((unsigned char)r[2]);
            char c;

            if (hi < 0 || lo < 0)
                return FALSE;
            c = (char)((hi << 4) | lo);
            if (c == '\0')
                return FALSE;
            *w++ = c;
            r += 3;
        }
        else
        {
            *w++ = *r++;
        }
    }
    *w = '\0';
    return TRUE;
}

/* Parse "METHOD /path?query HTTP/1.x".  The query string is kept raw;
 * the API decodes each value separately so that %26 stays a value. */
static bool parse_request(const char *req, struct http_request *r)
{
    const char *p;
    const char *q;
    const char *qm;
    uint len;
    uint plen;
    uint qlen;

    p = strchr(req, ' ');
    if (!p)
        return FALSE;
    len = (uint)(p - req);
    if (len == 0 || len >= sizeof(r->method))
        return FALSE;
    memcpy(r->method, req, len);
    r->method[len] = '\0';

    p++;
    q = strchr(p, ' ');
    if (!q)
        return FALSE;
    len = (uint)(q - p);
    if (len == 0)
        return FALSE;

    qm = memchr(p, '?', len);
    plen = qm ? (uint)(qm - p) : len;
    qlen = qm ? (uint)(q - qm - 1) : 0;
    if (plen >= sizeof(r->path) || qlen >= sizeof(r->query))
        return FALSE;

    memcpy(r->path, p, plen);
    r->path[plen] = '\0';
    r->query[0] = '\0';
    if (qlen)
    {
        memcpy(r->query, qm + 1, qlen);
        r->query[qlen] = '\0';
    }

    if (r->path[0] != '/')
        return FALSE;
    return url_decode_path(r->path);
}

/* Offset just past the blank line that ends the headers, or -1. */
static int header_end(const char *req, int used)
{
    int i;

    for (i = 0; i + 1 < used; i++)
    {
        if (req[i] == '\n' && req[i + 1] == '\n')
            return i + 2;
        if (i + 3 < used && req[i] == '\r' && req[i + 1] == '\n'
            && req[i + 2] == '\r' && req[i + 3] == '\n')
            return i + 4;
    }
    return -1;
}

static int ci_ncmp(const char *a, const char *b, int n)
{
    while (n-- > 0)
    {
        int ca = (unsigned char)*a++;
        int cb = (unsigned char)*b++;

        if (ca >= 'A' && ca <= 'Z')
            ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z')
            cb += 'a' - 'A';
        if (ca != cb)
            return ca - cb;
        if (!ca)
            return 0;
    }
    return 0;
}

static long content_length(const char *req, int hdrlen)
{
    const char *p = req;
    const char *end = req + hdrlen;

    while (p < end)
    {
        const char *e = p;
        while (e < end && *e != '\n')
            e++;

        if (ci_ncmp(p, "Content-Length:", 15) == 0)
        {
            p += 15;
            while (p < e && (*p == ' ' || *p == '\t'))
                p++;
            return atoi(p);
        }
        p = e + 1;
    }
    return -1;
}

static void send_error(int dev, int code, const char *text)
{
    char hdr[160];

    sprintf(hdr,
            "HTTP/1.0 %d %s\r\n"
            "Content-Type: text/plain\r\n"
            "Connection: close\r\n\r\n",
            code, text);
    write(dev, hdr, strlen(hdr));
    write(dev, text, strlen(text));
}

static void serve_static(int dev, const char *path)
{
    char full[FS_BUF_LEN];
    char hdr[256];
    char buf[512];
    FL_FILE *f;
    uint len;
    int r;

    if (strcmp(path, "/") == 0)
        path = "/index.html";
    if (strlen(path) >= sizeof(full))
    {
        send_error(dev, 404, "Not Found");
        return;
    }
    strcpy(full, full_path(path));

    f = fl_fopen(full, "r");
    if (!f)
    {
        send_error(dev, 404, "Not Found");
        return;
    }

    fl_fseek(f, 0, 2);
    len = (uint)fl_ftell(f);
    fl_fseek(f, 0, 0);

    sprintf(hdr,
            "HTTP/1.0 200 OK\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %lu\r\n"
            "Connection: close\r\n\r\n",
            mime_type(full), (unsigned long)len);
    write(dev, hdr, strlen(hdr));

    while ((r = fl_fread(buf, 1, sizeof(buf), f)) > 0)
    {
        if (write(dev, buf, r) != r)
            break;
    }
    fl_fclose(f);
}

/* Closing a TCP device blocks until the peer acknowledges our FIN.  The
 * server must not wait for that: while it does, no listening socket
 * exists and the next connection attempt is refused.  Hand the close to
 * a short-lived thread and go back to listening immediately. */
static thread httpdReleaser(int dev)
{
    close(dev);
    return OK;
}

static void httpdRelease(int dev)
{
    tid_typ tid;

    tid = create((void *)httpdReleaser, INITSTK, INITPRIO, "httpdRel", 1,
                 dev);
    if (SYSERR == tid)
    {
        close(dev);
        return;
    }
    ready(tid, RESCHED_YES);
}

thread httpdServer(int ethdev, int port)
{
    struct netif *iface;
    int tcpdev;
    char req[HTTPD_RQMAX + 1];
    struct http_request rq;
    int n, used, hdrlen;
    long clen;

    iface = netLookup(ethdev);
    if (!iface)
    {
        fprintf(stderr, "httpd: no network interface\n");
        return SYSERR;
    }

    if (!fat_mounted() && !fat_mount())
    {
        fprintf(stderr, "httpd: FAT not mounted\n");
        return SYSERR;
    }

    /* One worker for the browser terminal.  It is created once even if
     * httpd is started again from the shell. */
    if (OK != webtermInit())
        fprintf(stderr, "httpd: web terminal unavailable\n");

    while (TRUE)
    {
        tcpdev = tcpAllocPassive();
        if (SYSERR == (short)tcpdev)
        {
            sleep(10);
            continue;
        }

        if (open(tcpdev, &iface->ip, NULL, port, NULL, TCP_PASSIVE) < 0)
        {
            close(tcpdev);
            sleep(10);
            continue;
        }

        control(tcpdev, TCP_CTRL_RCVTIMEO, HTTPD_RCVTIMEO, 0);

        /* Read the request headers.  The loop stops at the blank line,
         * but a fast client may already have sent part of the body. */
        used = 0;
        req[0] = '\0';
        while (used < HTTPD_RQMAX
               && (n = read(tcpdev, req + used, HTTPD_RQMAX - used)) > 0)
        {
            used += n;
            req[used] = '\0';
            if (header_end(req, used) > 0)
                break;
        }

        hdrlen = header_end(req, used);
        if (hdrlen > 0 && parse_request(req, &rq))
        {
            if (strncmp(rq.path, "/api/", 5) == 0)
            {
                clen = content_length(req, hdrlen);

                /* curl -T may wait for this before sending the body. */
                if (strstr(req, "100-continue") != NULL)
                {
                    const char *cont = "HTTP/1.0 100 Continue\r\n\r\n";
                    write(tcpdev, cont, strlen(cont));
                }

                httpApiHandle(tcpdev, &rq, req + hdrlen,
                              used - hdrlen, clen);
            }
            else if (strcmp(rq.method, "GET") == 0)
            {
                serve_static(tcpdev, rq.path);
            }
            else
            {
                send_error(tcpdev, 405, "Method Not Allowed");
            }
        }
        else if (used > 0)
        {
            send_error(tcpdev, 400, "Bad Request");
        }

        httpdRelease(tcpdev);
    }

    return SYSERR;
}

shellcmd xsh_httpd(int nargs, char *args[])
{
    int port = HTTPD_PORT;
    tid_typ tid;

    if (nargs > 1)
        port = atoi(args[1]);
    if (port <= 0 || port > 65535)
    {
        printf("Usage: httpd [port]\n");
        return SHELL_ERROR;
    }

    tid = create((void *)httpdServer, HTTPD_STK, INITPRIO, "httpd", 2,
                 ETH0, port);
    if (SYSERR == tid)
    {
        printf("httpd: failed to create thread\n");
        return SHELL_ERROR;
    }
    ready(tid, RESCHED_YES);
    return SHELL_OK;
}
