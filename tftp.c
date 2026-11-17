/*############################################################################################### 
 *
 *           Author         : SHUBHAM SHIVPUJIMATH
 *           Date           : 07-10-2026
 *           File           : tftp.c©
 *           Descriptions   : function to create/extract packets before/after sending/receving
 *           Objective      : Packet creation and extraction
 *           Usage          :
 *           Output         :
 *
###############################################################################################*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include "tftp.h"
#include "file.h"

/* tftp protocol implementation 
 * Both server and client must use this file for sen/receive data
 */
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include "udp.h"

int tftp_verbose = 0;
int tftp_progress = 0;   /* 1 = print one # per block */
static int hashes_printed = 0;

/* ------------------------------------------------------------------ */
/* Output helpers (client only: server stays silent)                    */
/* ------------------------------------------------------------------ */
static void progress(void)
{
    if (!tftp_verbose || !tftp_progress) return;
    putchar('#'); fflush(stdout);
    hashes_printed = 1;
}

static void progress_end(void)
{
    if (hashes_printed) { putchar('\n'); hashes_printed = 0; }
}

static void report_error(const char *msg)
{
    if (!tftp_verbose) return;
    progress_end();
    printf("ERROR: %s\n", msg);
}

/* ------------------------------------------------------------------ */
/* Packet formation                                                     */
/* ------------------------------------------------------------------ */
packetbuffer_t *append_to_packet(packetbuffer_t *dst, const void *src, size_t n)
{
    memcpy(dst, src, n);
    return dst + n;                      /* next free position */
}

static packetbuffer_t *append_u16(packetbuffer_t *dst, unsigned short v)
{
    unsigned short nv = htons(v);        /* all TFTP integers are big endian */
    return append_to_packet(dst, &nv, sizeof nv);
}

static packetbuffer_t *append_str(packetbuffer_t *dst, const char *s)
{
    return append_to_packet(dst, s, strlen(s) + 1);   /* include the '\0' */
}

static int form_rq(packetbuffer_t *buf, opcode_t op, const char *filename, const char *mode)
{
    packetbuffer_t *p;
    if (strlen(filename) > MAXFILENAME || strlen(mode) > 15) return -1;
    p = append_u16(buf, op);
    p = append_str(p, filename);
    p = append_str(p, mode);
    return (int)(p - buf);
}

int packet_form_rrq(packetbuffer_t *buf, const char *filename, const char *mode)
{ return form_rq(buf, OPCODE_RRQ, filename, mode); }

int packet_form_wrq(packetbuffer_t *buf, const char *filename, const char *mode)
{ return form_rq(buf, OPCODE_WRQ, filename, mode); }

int packet_form_data(packetbuffer_t *buf, bnum_t blk, const char *data, int len)
{
    packetbuffer_t *p = append_u16(buf, OPCODE_DATA);
    p = append_u16(p, blk);
    p = append_to_packet(p, data, len);
    return (int)(p - buf);
}

int packet_form_ack(packetbuffer_t *buf, bnum_t blk)
{
    packetbuffer_t *p = append_u16(buf, OPCODE_ACK);
    p = append_u16(p, blk);
    return (int)(p - buf);
}

int packet_form_error(packetbuffer_t *buf, ecode_t code, const char *msg)
{
    packetbuffer_t *p = append_u16(buf, OPCODE_ERROR);
    size_t n = strlen(msg);
    if (n > 200) n = 200;
    p = append_u16(p, code);
    p = append_to_packet(p, msg, n);
    *p++ = '\0';
    return (int)(p - buf);
}

void packet_free(packet_t *pkt)          /* nothing is malloc'd; just reset */
{
    memset(pkt, 0, sizeof *pkt);
}

/* ------------------------------------------------------------------ */
/* Packet parsing (never trusts the network: every length is checked)   */
/* ------------------------------------------------------------------ */
opcode_t packet_extract_opcode(const packetbuffer_t *buf)
{
    unsigned short v;
    memcpy(&v, buf, sizeof v);
    return ntohs(v);
}

int packet_parse_rq(const packetbuffer_t *buf, int len, packet_t *pkt)
{
    const char *p = buf + 2, *end = buf + len, *nul;
    size_t n;

    nul = memchr(p, '\0', end - p);                 /* filename */
    if (!nul) return -1;
    n = nul - p;
    if (n == 0 || n > MAXFILENAME) return -1;
    memcpy(pkt->filename, p, n);

    p = nul + 1;
    nul = memchr(p, '\0', end - p);                 /* mode */
    if (!nul) return -1;
    n = nul - p;
    if (n == 0 || n > 15) return -1;
    memcpy(pkt->mode, p, n);
    for (char *c = pkt->mode; *c; c++)              /* modes are case-insensitive */
        if (*c >= 'A' && *c <= 'Z') *c += 'a' - 'A';
    return 0;
}

int packet_parse_data(const packetbuffer_t *buf, int len, packet_t *pkt)
{
    unsigned short b;
    if (len < 4 || len > PACKETSIZE) return -1;
    memcpy(&b, buf + 2, sizeof b);
    pkt->blocknum    = ntohs(b);
    pkt->data_length = len - 4;
    memcpy(pkt->data, buf + 4, pkt->data_length);
    return 0;
}

int packet_parse_ack(const packetbuffer_t *buf, int len, packet_t *pkt)
{
    unsigned short b;
    if (len < 4) return -1;
    memcpy(&b, buf + 2, sizeof b);
    pkt->blocknum = ntohs(b);
    return 0;
}

int packet_parse_error(const packetbuffer_t *buf, int len, packet_t *pkt)
{
    unsigned short c;
    int n;
    if (len < 4) return -1;
    memcpy(&c, buf + 2, sizeof c);
    pkt->ecode = ntohs(c);
    n = len - 4;
    if (n > 0 && buf[len - 1] == '\0') n--;         /* drop terminator */
    if (n > PACKETSIZE - 1) n = PACKETSIZE - 1;
    memcpy(pkt->estring, buf + 4, n);
    pkt->estring[n] = '\0';
    pkt->estring_length = n;
    return 0;
}

int packet_parse(const packetbuffer_t *buf, int len, packet_t *pkt)
{
    memset(pkt, 0, sizeof *pkt);
    if (len < 2) return -1;
    pkt->opcode = packet_extract_opcode(buf);
    switch (pkt->opcode)
    {
        case OPCODE_RRQ:
        case OPCODE_WRQ:   return packet_parse_rq(buf, len, pkt);
        case OPCODE_DATA:  return packet_parse_data(buf, len, pkt);
        case OPCODE_ACK:   return packet_parse_ack(buf, len, pkt);
        case OPCODE_ERROR: return packet_parse_error(buf, len, pkt);
        default:           return -1;
    }
}

/* ------------------------------------------------------------------ */
/* Packet receipt                                                       */
/* ------------------------------------------------------------------ */
void tftp_send_error(int sock, const struct sockaddr_in *to, ecode_t code, const char *msg)
{
    packetbuffer_t eb[PACKETSIZE];
    int n = packet_form_error(eb, code, msg);
    udp_send_packet(sock, eb, n, to);
}

/* Is this packet from the right TID?  The first reply to a request comes
 * from a new port, so an unlocked peer adopts it (same IP required). */
static int check_tid(int sock, peer_t *peer, const struct sockaddr_in *from)
{
    if (!peer->locked)
    {
        if (from->sin_addr.s_addr != peer->addr.sin_addr.s_addr) return 0;
        peer->addr   = *from;
        peer->locked = 1;
        return 1;
    }
    if (from->sin_addr.s_addr == peer->addr.sin_addr.s_addr &&
        from->sin_port == peer->addr.sin_port)
        return 1;

    tftp_send_error(sock, from, ECODE_5, ESTRING_5);   /* stranger: do not abort transfer */
    return 0;
}

/* Receive and parse one packet from the peer. */
static int recv_packet(int sock, peer_t *peer, packet_t *pkt)
{
    packetbuffer_t buf[PACKETSIZE];
    for (;;)
    {
        struct sockaddr_in from;
        socklen_t fl = sizeof from;
        int n = recvfrom(sock, buf, sizeof buf, 0, (struct sockaddr *)&from, &fl);
        if (n < 0)
        {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) return RECV_TIMEOUT;
            perror("recvfrom");
            return RECV_ERR;
        }
        if (!check_tid(sock, peer, &from)) continue;
        if (packet_parse(buf, n, pkt) < 0) return packet_receive_invalid(sock, peer);
        return RECV_OK;
    }
}

int packet_receive_error(const packet_t *pkt)
{
    char msg[PACKETSIZE];
    size_t n;
    snprintf(msg, sizeof msg, "%s", pkt->estring_length ? pkt->estring : ESTRING_0);
    n = strlen(msg);
    if (n && msg[n - 1] == '.') msg[n - 1] = '\0';   /* "File not found." -> "File not found" */
    if (tftp_verbose)
    {
        progress_end();
        printf("TFTP error %d: %s\n", pkt->ecode, msg);
    }
    return RECV_ERR;
}

int packet_receive_invalid(int sock, peer_t *peer)
{
    tftp_send_error(sock, &peer->addr, ECODE_4, ESTRING_4);
    report_error("Illegal TFTP operation");
    return RECV_ERR;
}

/* Wait for ACK number blk.  Stale ACKs are ignored (NOT answered by a resend:
 * that would double every packet - the "sorcerer's apprentice" bug). */
int packet_receive_ack(int sock, peer_t *peer, bnum_t blk, packet_t *pkt)
{
    for (;;)
    {
        int r = recv_packet(sock, peer, pkt);
        if (r != RECV_OK) return r;
        if (IS_ERROR(pkt->opcode)) return packet_receive_error(pkt);
        if (!IS_ACK(pkt->opcode))  return packet_receive_invalid(sock, peer);
        if (pkt->blocknum == blk)  return RECV_OK;
    }
}

/* Wait for DATA number blk.  A repeat of the previous block is reported as
 * RECV_DUP so the caller can re-send its ACK. */
int packet_receive_data(int sock, peer_t *peer, bnum_t blk, packet_t *pkt)
{
    for (;;)
    {
        int r = recv_packet(sock, peer, pkt);
        if (r != RECV_OK) return r;
        if (IS_ERROR(pkt->opcode)) return packet_receive_error(pkt);
        if (!IS_DATA(pkt->opcode)) return packet_receive_invalid(sock, peer);
        if (pkt->blocknum == blk)  return RECV_OK;
        if (pkt->blocknum == (bnum_t)(blk - 1)) return RECV_DUP;
    }
}

/* ------------------------------------------------------------------ */
/* Transfer engines                                                     */
/* ------------------------------------------------------------------ */

/* Send pkt and wait for ACK blk, retransmitting on timeout. */
int tftp_send_and_wait_ack(int sock, peer_t *peer, const packetbuffer_t *pkt, int len, bnum_t blk)
{
    packet_t reply;
    int tries = 0;
    for (;;)
    {
        int r;
        udp_send_packet(sock, pkt, len, &peer->addr);
        r = packet_receive_ack(sock, peer, blk, &reply);
        if (r == RECV_OK) return 0;
        if (r == RECV_TIMEOUT)
        {
            if (++tries >= TIMEOUT_LIMIT) { report_error("Timeout, no response from peer"); return -1; }
            continue;                                /* resend */
        }
        return -1;
    }
}

/* Send a whole file as DATA blocks 1..n.  The last block is < 512 bytes
 * (possibly 0 bytes if the file size is a multiple of 512). */
int tftp_send_file(int sock, peer_t *peer, int fd)
{
    packetbuffer_t out[PACKETSIZE];
    char data[BLIMIT];
    unsigned long blk = 1;

    for (;;)
    {
        int n = file_buffer_from_pos(fd, data, (off_t)(blk - 1) * BLIMIT);
        int len;
        if (n < 0)
        {
            tftp_send_error(sock, &peer->addr, ECODE_0, "Read error");
            report_error("Read error");
            return -1;
        }
        len = packet_form_data(out, (bnum_t)blk, data, n);
        if (tftp_send_and_wait_ack(sock, peer, out, len, (bnum_t)blk) < 0)
        {
            progress_end();
            return -1;
        }
        progress();
        if (n < BLIMIT) break;
        blk++;
    }
    progress_end();
    return 0;
}

/* Receive a whole file.  'first' is the packet that starts the exchange (RRQ
 * for a client get, ACK 0 for a server write); it is sent now and, on
 * timeout, the most recent packet we sent is sent again. */
int tftp_receive_file(int sock, peer_t *peer, int fd, const packetbuffer_t *first, int firstlen)
{
    packetbuffer_t last[PACKETSIZE];
    packet_t pkt;
    int lastlen = firstlen, tries = 0;
    unsigned long blk = 1;

    memcpy(last, first, firstlen);
    udp_send_packet(sock, last, lastlen, &peer->addr);

    for (;;)
    {
        int r = packet_receive_data(sock, peer, (bnum_t)blk, &pkt);
        if (r == RECV_TIMEOUT)
        {
            if (++tries >= TIMEOUT_LIMIT) { report_error("Timeout, no response from peer"); return -1; }
            udp_send_packet(sock, last, lastlen, &peer->addr);
            continue;
        }
        if (r == RECV_DUP)
        {
            udp_send_packet(sock, last, lastlen, &peer->addr);
            continue;
        }
        if (r < 0) { progress_end(); return -1; }

        tries = 0;
        if (file_append_from_buffer(fd, pkt.data, pkt.data_length) < 0)
        {
            int full = (errno == ENOSPC || errno == EDQUOT);
            tftp_send_error(sock, &peer->addr, full ? ECODE_3 : ECODE_0, full ? ESTRING_3 : strerror(errno));
            report_error(full ? "Disk full" : "Write error");
            return -1;
        }
        lastlen = packet_form_ack(last, (bnum_t)blk);
        udp_send_packet(sock, last, lastlen, &peer->addr);
        progress();

        if (pkt.data_length < BLIMIT) break;          /* short block = end of file */
        blk++;
    }
    progress_end();
    return 0;
}

/* ------------------------------------------------------------------ */
/* Server side handling of one request (runs in the forked child)       */
/* ------------------------------------------------------------------ */
static int mode_ok(const char *m)       /* "mail" is obsolete: deliberately refused */
{
    return !strcasecmp(m, MODE_NETASCII) || !strcasecmp(m, MODE_OCTET);
}

static int safe_filename(const char *f)  /* stay inside the server directory */
{
    size_t n = strlen(f);
    if (f[0] == '/') return 0;
    if (!strcmp(f, "..") || !strncmp(f, "../", 3) || strstr(f, "/../")) return 0;
    if (n >= 3 && !strcmp(f + n - 3, "/..")) return 0;
    return 1;
}

static void reply_errno(int sock, const peer_t *peer)
{
    switch (errno)
    {
        case ENOENT: tftp_send_error(sock, &peer->addr, ECODE_1, ESTRING_1); break;
        case EACCES:
        case EPERM:  tftp_send_error(sock, &peer->addr, ECODE_2, ESTRING_2); break;
        case EEXIST: tftp_send_error(sock, &peer->addr, ECODE_6, ESTRING_6); break;
        case ENOSPC:
        case EDQUOT: tftp_send_error(sock, &peer->addr, ECODE_3, ESTRING_3); break;
        default:     tftp_send_error(sock, &peer->addr, ECODE_0, strerror(errno)); break;
    }
}

int packet_receive_rrq(int sock, peer_t *peer, packet_t *pkt)
{
    int fd, r;
    if (!mode_ok(pkt->mode))
    { tftp_send_error(sock, &peer->addr, ECODE_4, "Unsupported transfer mode."); return RECV_ERR; }
    if (!safe_filename(pkt->filename))
    { tftp_send_error(sock, &peer->addr, ECODE_2, ESTRING_2); return RECV_ERR; }

    fd = file_open_read(pkt->filename);
    if (fd < 0) { reply_errno(sock, peer); return RECV_ERR; }

    r = tftp_send_file(sock, peer, fd);
    file_close(fd);
    return r;
}

int packet_receive_wrq(int sock, peer_t *peer, packet_t *pkt)
{
    packetbuffer_t ack[PACKETSIZE];
    int fd, len, r;
    if (!mode_ok(pkt->mode))
    { tftp_send_error(sock, &peer->addr, ECODE_4, "Unsupported transfer mode."); return RECV_ERR; }
    if (!safe_filename(pkt->filename))
    { tftp_send_error(sock, &peer->addr, ECODE_2, ESTRING_2); return RECV_ERR; }

    fd = file_open_write(pkt->filename);          /* O_EXCL: never overwrite */
    if (fd < 0) { reply_errno(sock, peer); return RECV_ERR; }

    len = packet_form_ack(ack, 0);                /* ACK 0 = "go ahead" */
    r = tftp_receive_file(sock, peer, fd, ack, len);
    file_close(fd);
    if (r < 0) unlink(pkt->filename);             /* don't leave a partial file */
    return r;
}