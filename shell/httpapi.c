/**
 * @file httpapi.c
 *
 * Small REST API for the file manager served by httpd.
 *
 *   GET    /api/files?path=/dir      list a directory (JSON)
 *   GET    /api/download?path=/file  download a file
 *   PUT    /api/file?path=/file      upload a file (raw body)
 *   DELETE /api/file?path=/file      delete a file
 *   POST   /api/mkdir?path=/dir      create a directory
 *   POST   /api/rmdir?path=/dir      remove an empty directory
 *
 * Paths are absolute inside the SD card.  full_path() resolves "." and
 * ".." and clamps at the root, so a request cannot escape the card.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <device.h>
#include <shell.h>
#include <fat_filelib.h>
#include <xsh_fs.h>
#include <xsh_http.h>
#include <webterm.h>

#define API_PARAM       512
#define API_BUF         1024
#define API_MAX_UPLOAD  (16L * 1024L * 1024L)

static int write_all(int dev, const void *buf, int len)
{
    const char *p = (const char *)buf;

    while (len > 0)
    {
        int n = write(dev, p, len);
        if (n <= 0)
            return -1;
        p += n;
        len -= n;
    }
    return 0;
}

static void json_escape(const char *src, char *dst, int dstlen)
{
    static const char hex[] = "0123456789abcdef";
    int o = 0;

    if (dstlen <= 0)
        return;

    while (*src && o < dstlen - 1)
    {
        unsigned char c = (unsigned char)*src++;

        if (c == '"' || c == '\\')
        {
            if (o + 2 >= dstlen)
                break;
            dst[o++] = '\\';
            dst[o++] = (char)c;
        }
        else if (c < 0x20)
        {
            if (o + 6 >= dstlen)
                break;
            dst[o++] = '\\';
            dst[o++] = 'u';
            dst[o++] = '0';
            dst[o++] = '0';
            dst[o++] = hex[(c >> 4) & 0x0f];
            dst[o++] = hex[c & 0x0f];
        }
        else
        {
            dst[o++] = (char)c;
        }
    }
    dst[o] = '\0';
}

static int send_headers(int dev, int code, const char *reason,
                        const char *ctype, long len, const char *disp)
{
    char hdr[320];
    int n;

    n = sprintf(hdr, "HTTP/1.0 %d %s\r\n", code, reason);
    n += sprintf(hdr + n, "Content-Type: %s\r\n", ctype);
    if (len >= 0)
        n += sprintf(hdr + n, "Content-Length: %ld\r\n", len);
    if (disp && disp[0])
        n += sprintf(hdr + n,
                     "Content-Disposition: attachment; filename=\"%s\"\r\n",
                     disp);
    n += sprintf(hdr + n, "Connection: close\r\n\r\n");

    return write_all(dev, hdr, n);
}

static int send_json(int dev, int code, const char *reason, const char *json)
{
    if (send_headers(dev, code, reason, "application/json",
                     (long)strlen(json), NULL) < 0)
        return -1;
    return write_all(dev, json, strlen(json));
}

static int send_json_error(int dev, int code, const char *reason,
                           const char *msg)
{
    char esc[192];
    char json[256];

    json_escape(msg, esc, sizeof(esc));
    sprintf(json, "{\"ok\":false,\"error\":\"%s\"}", esc);
    return send_json(dev, code, reason, json);
}

/* The job endpoint returns text/plain and carries the state in headers so
 * large command output never has to be JSON-escaped or buffered twice. */
static int send_job_headers(int dev, int code, const char *reason, long len,
                            const char *state, int trunc)
{
    char hdr[320];
    int n;

    n = sprintf(hdr, "HTTP/1.0 %d %s\r\n", code, reason);
    n += sprintf(hdr + n, "Content-Type: text/plain; charset=utf-8\r\n");
    n += sprintf(hdr + n, "Content-Length: %ld\r\n", len);
    n += sprintf(hdr + n, "X-Job-State: %s\r\n", state);
    n += sprintf(hdr + n, "X-Job-Truncated: %d\r\n", trunc ? 1 : 0);
    n += sprintf(hdr + n, "Cache-Control: no-store\r\n");
    n += sprintf(hdr + n, "Connection: close\r\n\r\n");

    return write_all(dev, hdr, n);
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

/* Decode %XX and '+' in a query value.  Rejects NUL bytes. */
static int url_decode(const char *src, char *dst, int dstlen)
{
    int o = 0;

    while (*src)
    {
        unsigned char c = (unsigned char)*src++;

        if (c == '+')
        {
            c = ' ';
        }
        else if (c == '%')
        {
            int hi = hexval((unsigned char)src[0]);
            int lo = hexval((unsigned char)src[1]);

            if (hi < 0 || lo < 0)
                return -1;
            src += 2;
            c = (unsigned char)((hi << 4) | lo);
            if (c == 0)
                return -1;
        }

        if (o >= dstlen - 1)
            return -1;
        dst[o++] = (char)c;
    }
    dst[o] = '\0';
    return 0;
}

/* Find "name=value" in a query string.  Returns 1 if found, 0 if not,
 * -1 on a malformed or overlong value. */
static int get_param(const char *query, const char *name,
                     char *out, int outlen)
{
    const char *p = query;
    int nlen = (int)strlen(name);

    while (p && *p)
    {
        const char *amp = strchr(p, '&');
        const char *end = amp ? amp : p + strlen(p);
        const char *eq = strchr(p, '=');

        if (eq && eq < end && (int)(eq - p) == nlen
            && strncmp(p, name, nlen) == 0)
        {
            int vlen = (int)(end - (eq + 1));
            char tmp[API_PARAM];

            if (vlen < 0 || vlen >= (int)sizeof(tmp))
                return -1;
            memcpy(tmp, eq + 1, vlen);
            tmp[vlen] = '\0';
            return url_decode(tmp, out, outlen) == 0 ? 1 : -1;
        }
        p = amp ? amp + 1 : NULL;
    }
    return 0;
}

static int resolve_path(const struct http_request *req, char *full, int fullsz)
{
    char raw[FS_BUF_LEN];
    char *p;
    int r;

    r = get_param(req->query, "path", raw, sizeof(raw));
    if (r < 0)
        return -1;
    if (r == 0)
        strcpy(raw, "/");
    if (raw[0] != '/')
        return -1;

    p = full_path(raw);
    if (!p || strlen(p) >= (size_t)fullsz)
        return -1;
    strcpy(full, p);
    return 0;
}

static void base_name(const char *path, char *out, int outlen)
{
    const char *p = strrchr(path, '/');
    int o = 0;

    p = p ? p + 1 : path;
    while (*p && o < outlen - 1)
    {
        char c = *p++;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')
            out[o++] = c;
        else
            out[o++] = '_';
    }
    if (o == 0)
    {
        strcpy(out, "download.bin");
        return;
    }
    out[o] = '\0';
}

static int api_files(int dev, const struct http_request *req)
{
    char full[FS_BUF_LEN];
    char esc[256];
    char item[384];
    FL_DIR dir;
    fl_dirent ent;
    int first = 1;

    if (resolve_path(req, full, sizeof(full)) < 0)
        return send_json_error(dev, 400, "Bad Request", "invalid path");
    if (!fl_opendir(full, &dir))
        return send_json_error(dev, 404, "Not Found", "cannot open directory");

    if (send_headers(dev, 200, "OK", "application/json", -1, NULL) < 0)
    {
        fl_closedir(&dir);
        return -1;
    }

    json_escape(full, esc, sizeof(esc));
    sprintf(item, "{\"path\":\"%s\",\"entries\":[", esc);
    if (write_all(dev, item, strlen(item)) < 0)
    {
        fl_closedir(&dir);
        return -1;
    }

    while (fl_readdir(&dir, &ent) == 0)
    {
        if (!strcmp(ent.filename, ".") || !strcmp(ent.filename, ".."))
            continue;

        json_escape(ent.filename, esc, sizeof(esc));
        sprintf(item, "%s{\"name\":\"%s\",\"dir\":%s,\"size\":%lu}",
                first ? "" : ",", esc,
                ent.is_dir ? "true" : "false",
                (unsigned long)ent.size);
        if (write_all(dev, item, strlen(item)) < 0)
        {
            fl_closedir(&dir);
            return -1;
        }
        first = 0;
    }

    fl_closedir(&dir);
    return write_all(dev, "]}", 2);
}

static int api_download(int dev, const struct http_request *req)
{
    char full[FS_BUF_LEN];
    char name[80];
    char buf[512];
    FL_FILE *f;
    long len;
    int n;

    if (resolve_path(req, full, sizeof(full)) < 0)
        return send_json_error(dev, 400, "Bad Request", "invalid path");
    if (fl_is_dir(full))
        return send_json_error(dev, 400, "Bad Request", "is a directory");

    f = fl_fopen(full, "r");
    if (!f)
        return send_json_error(dev, 404, "Not Found", "cannot open file");

    fl_fseek(f, 0, 2);
    len = fl_ftell(f);
    fl_fseek(f, 0, 0);
    base_name(full, name, sizeof(name));

    if (send_headers(dev, 200, "OK", "application/octet-stream", len, name) < 0)
    {
        fl_fclose(f);
        return -1;
    }

    while ((n = fl_fread(buf, 1, sizeof(buf), f)) > 0)
    {
        if (write_all(dev, buf, n) < 0)
            break;
    }

    fl_fclose(f);
    return 0;
}

static int api_upload(int dev, const struct http_request *req,
                      const char *prebody, int prelen, long bodylen)
{
    char full[FS_BUF_LEN];
    char esc[256];
    char json[384];
    char buf[API_BUF];
    FL_FILE *f;
    long written = 0;
    long remaining;
    int n;

    if (resolve_path(req, full, sizeof(full)) < 0)
        return send_json_error(dev, 400, "Bad Request", "invalid path");
    if (fl_is_dir(full))
        return send_json_error(dev, 409, "Conflict", "path is a directory");
    if (bodylen < 0)
        return send_json_error(dev, 411, "Length Required",
                               "Content-Length is required");
    if (bodylen > API_MAX_UPLOAD)
        return send_json_error(dev, 413, "Payload Too Large",
                               "file exceeds the 16 MB limit");

    f = fl_fopen(full, "w");
    if (!f)
        return send_json_error(dev, 500, "Internal Server Error",
                               "cannot create file");

    if (prelen < 0)
        prelen = 0;
    if ((long)prelen > bodylen)
        prelen = (int)bodylen;

    if (prelen > 0)
    {
        n = fl_fwrite(prebody, 1, prelen, f);
        if (n != prelen)
        {
            fl_fclose(f);
            fs_remove_file(full);
            return send_json_error(dev, 500, "Internal Server Error",
                                   "cannot write file");
        }
        written += prelen;
    }

    remaining = bodylen - written;
    while (remaining > 0)
    {
        int want = remaining > (long)sizeof(buf) ? (int)sizeof(buf)
                                                 : (int)remaining;

        n = read(dev, buf, want);
        if (n <= 0)
            break;
        if (fl_fwrite(buf, 1, n, f) != n)
        {
            fl_fclose(f);
            fs_remove_file(full);
            return send_json_error(dev, 500, "Internal Server Error",
                                   "cannot write file");
        }
        written += n;
        remaining -= n;
    }

    fl_fclose(f);

    if (written != bodylen)
    {
        fs_remove_file(full);
        return send_json_error(dev, 400, "Bad Request", "incomplete body");
    }

    json_escape(full, esc, sizeof(esc));
    sprintf(json, "{\"ok\":true,\"path\":\"%s\",\"size\":%ld}",
            esc, written);
    return send_json(dev, 201, "Created", json);
}

static int api_delete(int dev, const struct http_request *req)
{
    char full[FS_BUF_LEN];
    FL_FILE *probe;

    if (resolve_path(req, full, sizeof(full)) < 0)
        return send_json_error(dev, 400, "Bad Request", "invalid path");
    if (fl_is_dir(full))
        return send_json_error(dev, 409, "Conflict",
                               "path is a directory; use rmdir");

    probe = fl_fopen(full, "r");
    if (!probe)
        return send_json_error(dev, 404, "Not Found", "file not found");
    fl_fclose(probe);

    if (fs_remove_file(full) != SHELL_OK)
        return send_json_error(dev, 500, "Internal Server Error",
                               "cannot delete file");

    return send_json(dev, 200, "OK", "{\"ok\":true}");
}

static int api_mkdir(int dev, const struct http_request *req)
{
    char full[FS_BUF_LEN];

    if (resolve_path(req, full, sizeof(full)) < 0)
        return send_json_error(dev, 400, "Bad Request", "invalid path");
    if (fl_is_dir(full))
        return send_json_error(dev, 409, "Conflict", "directory exists");
    if (fl_createdirectory(full) != 1)
        return send_json_error(dev, 500, "Internal Server Error",
                               "cannot create directory");

    return send_json(dev, 201, "Created", "{\"ok\":true}");
}

static int api_rmdir(int dev, const struct http_request *req)
{
    char full[FS_BUF_LEN];

    if (resolve_path(req, full, sizeof(full)) < 0)
        return send_json_error(dev, 400, "Bad Request", "invalid path");
    if (!fl_is_dir(full))
        return send_json_error(dev, 400, "Bad Request", "not a directory");
    if (fl_removedirectory(full) != 0)
        return send_json_error(dev, 409, "Conflict",
                               "directory is not empty or cannot be removed");

    return send_json(dev, 200, "OK", "{\"ok\":true}");
}

static int api_exec(int dev, const struct http_request *req,
                    const char *prebody, int prelen, long bodylen)
{
    char cmd[WEB_CMD_MAX];
    char json[128];
    int n = 0;
    int r;
    int id;

    if (!webtermReady())
        return send_json_error(dev, 503, "Service Unavailable",
                               "terminal not ready");
    if (bodylen > WEB_CMD_MAX - 1)
        return send_json_error(dev, 413, "Payload Too Large",
                               "command exceeds 159 bytes");

    if (bodylen > 0)
    {
        if (prelen > bodylen)
            prelen = (int)bodylen;
        if (prelen > 0)
        {
            memcpy(cmd, prebody, (size_t)prelen);
            n = prelen;
        }
        while (n < (int)bodylen)
        {
            r = read(dev, cmd + n, (int)bodylen - n);
            if (r <= 0)
                break;
            n += r;
        }
        if (n != (int)bodylen)
            return send_json_error(dev, 400, "Bad Request",
                                   "incomplete body");
    }
    else
    {
        r = get_param(req->query, "cmd", cmd, sizeof(cmd));
        if (r != 1)
            return send_json_error(dev, 400, "Bad Request",
                                   "empty command");
        n = (int)strlen(cmd);
    }

    cmd[n] = '\0';
    while (n > 0 && (cmd[n - 1] == '\n' || cmd[n - 1] == '\r'
                     || cmd[n - 1] == ' ' || cmd[n - 1] == '\t'))
        cmd[--n] = '\0';

    {
        int s = 0;

        while (cmd[s] == ' ' || cmd[s] == '\t')
            s++;
        if (s > 0)
        {
            int k;

            for (k = 0; k <= n - s; k++)
                cmd[k] = cmd[k + s];
            n -= s;
        }
    }

    if (n == 0)
        return send_json_error(dev, 400, "Bad Request", "empty command");

    id = webtermSubmit(cmd, n);
    if (id < 0)
        return send_json_error(dev, 409, "Conflict", "terminal busy");

    sprintf(json, "{\"ok\":true,\"id\":%d,\"state\":\"running\"}", id);
    return send_json(dev, 202, "Accepted", json);
}

static int api_job(int dev, const struct http_request *req)
{
    char idbuf[16];
    const char *out;
    int id, state, len, trunc, olen, otrunc;

    if (!webtermReady())
        return send_json_error(dev, 503, "Service Unavailable",
                               "terminal not ready");
    if (get_param(req->query, "id", idbuf, sizeof(idbuf)) != 1)
        return send_json_error(dev, 400, "Bad Request", "missing id");

    id = atoi(idbuf);
    if (webtermStatus(id, &state, &len, &trunc) < 0)
        return send_json_error(dev, 404, "Not Found", "unknown job");

    (void)len;
    (void)trunc;

    if (state == WEB_STATE_RUNNING)
        return send_job_headers(dev, 202, "Accepted", 0, "running", 0);
    if (state != WEB_STATE_DONE)
        return send_json_error(dev, 404, "Not Found", "unknown job");

    if (webtermSnapshot(id, &out, &olen, &otrunc) < 0)
        return send_json_error(dev, 404, "Not Found", "unknown job");

    if (send_job_headers(dev, 200, "OK", olen, "done", otrunc) < 0)
        return -1;
    return write_all(dev, out, olen);
}

int httpApiHandle(int dev, const struct http_request *req,
                  const char *prebody, int prelen, long bodylen)
{
    if (!strcmp(req->path, "/api/files"))
    {
        if (!strcmp(req->method, "GET"))
            return api_files(dev, req);
        return send_json_error(dev, 405, "Method Not Allowed", "use GET");
    }

    if (!strcmp(req->path, "/api/download"))
    {
        if (!strcmp(req->method, "GET"))
            return api_download(dev, req);
        return send_json_error(dev, 405, "Method Not Allowed", "use GET");
    }

    if (!strcmp(req->path, "/api/exec"))
    {
        if (!strcmp(req->method, "POST"))
            return api_exec(dev, req, prebody, prelen, bodylen);
        return send_json_error(dev, 405, "Method Not Allowed", "use POST");
    }

    if (!strcmp(req->path, "/api/job"))
    {
        if (!strcmp(req->method, "GET"))
            return api_job(dev, req);
        return send_json_error(dev, 405, "Method Not Allowed", "use GET");
    }

    if (!strcmp(req->path, "/api/file"))
    {
        if (!strcmp(req->method, "PUT"))
            return api_upload(dev, req, prebody, prelen, bodylen);
        if (!strcmp(req->method, "DELETE"))
            return api_delete(dev, req);
        return send_json_error(dev, 405, "Method Not Allowed",
                               "use PUT or DELETE");
    }

    if (!strcmp(req->path, "/api/mkdir"))
    {
        if (!strcmp(req->method, "POST"))
            return api_mkdir(dev, req);
        return send_json_error(dev, 405, "Method Not Allowed", "use POST");
    }

    if (!strcmp(req->path, "/api/rmdir"))
    {
        if (!strcmp(req->method, "POST"))
            return api_rmdir(dev, req);
        return send_json_error(dev, 405, "Method Not Allowed", "use POST");
    }

    return send_json_error(dev, 404, "Not Found", "unknown endpoint");
}
