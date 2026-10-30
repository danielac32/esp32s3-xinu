/**
 * @file webterm.h
 *
 * A small job runner for the browser terminal.  The HTTP layer submits a
 * command and polls the job; the actual shell procedure runs in the
 * dedicated "webshell" thread with stdout/stderr pointed at MEMTTY0.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#ifndef _WEBTERM_H_
#define _WEBTERM_H_

#define WEB_CMD_MAX       160

#define WEB_STATE_IDLE    0
#define WEB_STATE_RUNNING 1
#define WEB_STATE_DONE    2

int webtermInit(void);
int webtermReady(void);

/* Submit a command.  Returns the new job id, or -1 if the terminal is not
 * ready or another command is still running. */
int webtermSubmit(const char *cmd, int len);

/* Query a job.  Returns 0 on success, -1 if the id is unknown. */
int webtermStatus(int id, int *state, int *len, int *trunc);

/* Copy the finished output into the snapshot buffer and return a pointer to
 * it.  The pointer stays valid until the next snapshot or a new job. */
int webtermSnapshot(int id, const char **buf, int *len, int *trunc);

#endif /* _WEBTERM_H_ */
