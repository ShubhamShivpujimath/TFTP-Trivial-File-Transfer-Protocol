#ifndef TFTP_FILE
#define TFTP_FILE
#include <sys/types.h>
#include "tftp.h"
/* Function design for file operation functions */
int file_open_read(const char *name);                          /* fd or -1 (regular files only) */
int file_buffer_from_pos(int fd, char *buf, off_t pos);        /* read up to BLIMIT bytes at pos */
int file_open_write(const char *name);                         /* create new file, fails if it exists */
int file_append_from_buffer(int fd, const char *buf, int len); /* write all len bytes */
int file_close(int fd);

#endif