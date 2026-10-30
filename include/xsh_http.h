/**
 * @file xsh_http.h
 *
 * Shared definitions for the small HTTP API served by httpd.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#ifndef _XSH_HTTP_H_
#define _XSH_HTTP_H_

struct http_request
{
    char method[8];
    char path[192];
    char query[192];
};

/**
 * Handle one /api/... request.
 *
 * @param dev      TCP device to read the body from and write the response to
 * @param req      parsed method, path and query string
 * @param prebody  bytes already read after the header terminator
 * @param prelen   number of valid bytes in @p prebody
 * @param bodylen  Content-Length, or -1 when the header is absent
 * @return OK on success, SYSERR on a write error
 */
int httpApiHandle(int dev, const struct http_request *req,
                  const char *prebody, int prelen, long bodylen);

#endif /* _XSH_HTTP_H_ */
