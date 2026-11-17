/*############################################################################################### 
 *
 *           Author         : SHUBHAM SHIVPUJIMATH
 *           Date           : 07-10-2026
 *           File           : file.c©
 *           Descriptions   : All file operations
 *           Objective      : To read/write from/to file in both server and client side
 *           Usage          :
 *           Output         :
 *
###############################################################################################*/

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "tftp.h"
#include <sys/stat.h>
#include "file.h"

/* Open an existing regular file for reading. */
int file_open_read(const char *name)
{
    struct stat st;
    int fd = open(name, O_RDONLY);
    if (fd < 0) return -1;
    if (fstat(fd, &st) < 0 || !S_ISREG(st.st_mode))
    {
        close(fd);
        errno = EACCES;
        return -1;
    }
    return fd;
}

/* Read up to BLIMIT bytes starting at byte offset pos.  Returns bytes read
 * (0 at EOF) or -1.  Using an offset means a retransmit just re-reads. */
int file_buffer_from_pos(int fd, char *buf, off_t pos)
{
    int total = 0;
    while (total < BLIMIT)
    {
        ssize_t n = pread(fd, buf + total, BLIMIT - total, pos + total);
        if (n < 0) { if (errno == EINTR) continue; return -1; }
        if (n == 0) break;
        total += n;
    }
    return total;
}

/* Create a NEW file for writing; O_EXCL makes "already exists" detectable. */
int file_open_write(const char *name)
{
    return open(name, O_WRONLY | O_CREAT | O_EXCL, 0644);
}

/* Write all len bytes at the current position. */
int file_append_from_buffer(int fd, const char *buf, int len)
{
    int done = 0;
    while (done < len)
    {
        ssize_t n = write(fd, buf + done, len - done);
        if (n < 0) { if (errno == EINTR) continue; return -1; }
        done += n;
    }
    return done;
}

int file_close(int fd)
{
    return close(fd);
}