/**
 * @file xsh_fs.h
 *
 * Shared helpers for the FAT filesystem shell commands.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#ifndef _XSH_FS_H_
#define _XSH_FS_H_

#include <fat_filelib.h>

#define FS_BUF_LEN 128

/* Current working directory, shared by cd, pwd, ls and friends. */
extern char fs_curdir[FS_BUF_LEN];

/* Resolve @p name against the current directory.  The result points into
 * a static buffer that the next call overwrites. */
char *full_path(const char *name);

/* Make sure the FAT filesystem is mounted.  Returns TRUE when it is
 * usable, FALSE (after printing a message) when it is not. */
int fs_need(void);

/* Open @p name through the current directory, reporting an error with
 * @p cmd's name on failure. */
FL_FILE *fs_open(const char *cmd, const char *name, const char *mode);

/* Delete a file and free its cluster chain.  The directory entry is
 * removed first and the handle is always closed, so a failure cannot
 * leak one of the library's file handles.  Returns SHELL_OK/SHELL_ERROR. */
int fs_remove_file(const char *path);

#endif /* _XSH_FS_H_ */
