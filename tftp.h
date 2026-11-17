#ifndef TFTP
#define TFTP

#include <stddef.h>
#include <netinet/in.h>

#define BLIMIT      512 /*Maximum amount of file contents that can be buffered*/
#define PACKETSIZE  516 /*Total size of TFTP packet*/

typedef unsigned short opcode_t; /*TFTP Operation Code*/
#define OPCODE_RRQ   1
#define IS_RRQ(op)   ((op) == OPCODE_RRQ) /*Evaluates as true or false*/
#define OPCODE_WRQ   2
#define IS_WRQ(op)   ((op) == OPCODE_WRQ)
#define OPCODE_DATA  3
#define IS_DATA(op)  ((op) == OPCODE_DATA)
#define OPCODE_ACK   4
#define IS_ACK(op)   ((op) == OPCODE_ACK)
#define OPCODE_ERROR 5
#define IS_ERROR(op) ((op) == OPCODE_ERROR)

typedef unsigned short ecode_t; /*TFTP Error code*/
#define ECODE_NONE  8
#define ECODE_0     0
#define IS_ECODE_0(ec)  ((ec) == ECODE_0)
#define ESTRING_0   "Not defined, see error message(if any)."
#define ECODE_1     1
#define IS_ECODE_1(ec)  ((ec) == ECODE_1)
#define ESTRING_1   "File not found."
#define ECODE_2     2
#define IS_ECODE_2(ec)  ((ec) == ECODE_2)
#define ESTRING_2   "Access violation."
#define ECODE_3     3
#define IS_ECODE_3(ec)  ((ec) == ECODE_3)
#define ESTRING_3   "Disk full or allocation exceeded."
#define ECODE_4     4
#define IS_ECODE_4(ec)  ((ec) == ECODE_4)
#define ESTRING_4   "Illegal TFTP operation."
#define ECODE_5     5
#define IS_ECODE_5(ec)  ((ec) == ECODE_5)
#define ESTRING_5   "Unknown transfer ID."
#define ECODE_6     6
#define IS_ECODE_6(ec)  ((ec) == ECODE_6)
#define ESTRING_6   "File already exists."
#define ECODE_7     7
#define IS_ECODE_7(ec)  ((ec) == ECODE_7)
#define ESTRING_7   "No such user."

/*Transfer operation modes*/
#define MODE_NETASCII "netascii" 
#define MODE_OCTET    "octet"
#define MODE_MAIL     "mail"

#define TIMEOUT         1 /*Socket receive timeout*/
#define TIMEOUT_LIMIT   10 /*Retransmissions before giving up*/

#define DEFAULT_PORT    69    /*Well known TFTP port (client default)*/
#define FIRST_TID_PORT  20000 /*First private server port handed to a client*/
#define MAXFILENAME     255

/*Return codes of the packet_receive_* functions*/
#define RECV_OK       0
#define RECV_TIMEOUT  1
#define RECV_DUP      2  /*Duplicate DATA: our ACK was probably lost*/
#define RECV_ERR     -1

typedef unsigned short bnum_t; /*TFTP Block number, for data packets*/
typedef char packetbuffer_t; /*Packet buffer*/

/*Struct for storage of the data extracted from a packet buffer*/
typedef struct tftp_packet
{
    char filename[PACKETSIZE];
    opcode_t opcode;
    char mode[PACKETSIZE];
    char data[BLIMIT];
    int data_length;
    bnum_t blocknum;
    ecode_t ecode;
    char estring[PACKETSIZE];
    int estring_length;
}packet_t;

/* The remote end of a transfer.  'locked' becomes 1 once the peer's TID
 * (ip:port) is known; packets from any other source are answered with ERROR 5. */
typedef struct tftp_peer
{
    struct sockaddr_in addr;
    int locked;
}peer_t;

extern int tftp_progress; /*1 = also print # per block*/
extern int tftp_verbose; /*1 = client: print '#' progress and ERROR lines*/

/* Packet Parsing (return 0 on success, -1 on a malformed packet) */
int  packet_parse(const packetbuffer_t *buf, int len, packet_t *pkt);
opcode_t packet_extract_opcode(const packetbuffer_t *buf);
int  packet_parse_rq(const packetbuffer_t *buf, int len, packet_t *pkt);
int  packet_parse_data(const packetbuffer_t *buf, int len, packet_t *pkt);
int  packet_parse_ack(const packetbuffer_t *buf, int len, packet_t *pkt);
int  packet_parse_error(const packetbuffer_t *buf, int len, packet_t *pkt);

/* Packet Formation: write into caller's PACKETSIZE buffer, return length */
packetbuffer_t *append_to_packet(packetbuffer_t *dst, const void *src, size_t n);
int  packet_form_rrq(packetbuffer_t *buf, const char *filename, const char *mode);
int  packet_form_wrq(packetbuffer_t *buf, const char *filename, const char *mode);
int  packet_form_data(packetbuffer_t *buf, bnum_t blk, const char *data, int len);
int  packet_form_error(packetbuffer_t *buf, ecode_t code, const char *msg);
int  packet_form_ack(packetbuffer_t *buf, bnum_t blk);
void packet_free(packet_t *pkt);

/* Packet Receipt */
int  packet_receive_rrq(int sock, peer_t *peer, packet_t *pkt);  /*server: serve a read request*/
int  packet_receive_wrq(int sock, peer_t *peer, packet_t *pkt);  /*server: serve a write request*/
int  packet_receive_data(int sock, peer_t *peer, bnum_t blk, packet_t *pkt); /*wait for DATA blk*/
int  packet_receive_ack(int sock, peer_t *peer, bnum_t blk, packet_t *pkt);  /*wait for ACK blk*/
int  packet_receive_error(const packet_t *pkt);                  /*report a received ERROR*/
int  packet_receive_invalid(int sock, peer_t *peer);             /*answer a bad packet with ERROR 4*/

/* Transfer engines shared by client and server */
int  tftp_send_and_wait_ack(int sock, peer_t *peer, const packetbuffer_t *pkt, int len, bnum_t blk);
int  tftp_send_file(int sock, peer_t *peer, int fd);
int  tftp_receive_file(int sock, peer_t *peer, int fd, const packetbuffer_t *first, int firstlen);
void tftp_send_error(int sock, const struct sockaddr_in *to, ecode_t code, const char *msg);

#endif