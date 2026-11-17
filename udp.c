/*############################################################################################### 
 *
 *           Author         : SHUBHAM SHIVPUJIMATH
 *           Date           : 07-10-2026
 *           File           : udp.c©
 *           Descriptions   : Functions to bind udp sockets for server and clients.
 *           Objective      : Create a UDP socket and bind with given IP address and PORT number
 *           Usage          :
 *           Output         :
 *
###############################################################################################*/

#include <netinet/in.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "tftp.h"

/* To create sockets and binding for server and clients*/
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include "udp.h"

/* Socket receive timeout: recvfrom() fails with EAGAIN after TIMEOUT seconds,
 * which the protocol layer treats as "packet lost, retransmit". */
static int set_timeout(int sock)
{
    struct timeval tv = { TIMEOUT, 0 };
    return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
}

/* Server: well-known port, listens for RRQ/WRQ from anyone. */
int udp_bind_server(int port)
{
    struct sockaddr_in addr;
    int on = 1;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return -1; }
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);

    memset(&addr, 0, sizeof addr);
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port);
    if (bind(sock, (struct sockaddr *)&addr, sizeof addr) < 0)
    {
        perror("bind");
        close(sock);
        return -1;
    }
    return sock;
}

/* Child server: new socket (= new TID) on the first free port >= *port.
 * *port is updated with the port that was actually bound. */
int udp_rebind_server(int *port)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return -1; }

    for (; *port <= 65535; (*port)++)
    {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof addr);
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port        = htons(*port);
        if (bind(sock, (struct sockaddr *)&addr, sizeof addr) == 0)
        {
            set_timeout(sock);
            return sock;
        }
        if (errno != EADDRINUSE) break;
    }
    perror("bind (private port)");
    close(sock);
    return -1;
}

/* Client: the OS picks the source port, which becomes the client's TID. */
int udp_bind_client(void)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return -1; }
    set_timeout(sock);
    return sock;
}

int udp_send_packet(int sock, const packetbuffer_t *buf, int len, const struct sockaddr_in *to)
{
#ifdef TFTP_TEST_LOSS
    /* Test aid: build with -DTFTP_TEST_LOSS=N to drop ~1 in N outgoing packets. */
    static pid_t seeded = 0;
    if (seeded != getpid()) { srand(time(NULL) ^ getpid()); seeded = getpid(); }
    if (rand() % TFTP_TEST_LOSS == 0) return len;
#endif
    int n = sendto(sock, buf, len, 0, (const struct sockaddr *)to, sizeof *to);
    if (n < 0) perror("sendto");
    return n;
}