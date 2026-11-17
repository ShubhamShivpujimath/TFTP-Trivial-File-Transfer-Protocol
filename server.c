/*############################################################################################### 
 *
 *           Author         : SHUBHAM SHIVPUJIMATH
 *           Date           : 07-10-2026
 *           File           : server.c©
 *           Descriptions   : Server side of tftp protocol
 *           Objective      : Ready to recive/send data to client 
 *           Usage          : ./server <port number>
 *           Output         :
 *
###############################################################################################*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include "file.h"
#include "udp.h"
#include "tftp.h"

/* Server code for tftp Protocol
 *
 * The parent only ever listens on the well-known port and accepts the first
 * packet (RRQ/WRQ).  For every request it forks a child that opens a NEW
 * socket on a private port (20000, 20001, ...) - that port is the server's
 * TID - and runs the whole transfer there.  The parent goes straight back to
 * listening, so many clients can be served at once.
 */

static void serve_client(const struct sockaddr_in *client, packet_t *req, int port)
{
    peer_t peer;
    int sock = udp_rebind_server(&port);
    if (sock < 0) _exit(1);

    peer.addr   = *client;
    peer.locked = 1;                         /* client TID is already known */

    if (IS_RRQ(req->opcode)) packet_receive_rrq(sock, &peer, req);
    else                     packet_receive_wrq(sock, &peer, req);

    close(sock);
    _exit(0);
}

int main(int argc, char *argv[])
{
    int sock, port, next_tid = FIRST_TID_PORT;

    if (argc > 2)
    {
        fprintf(stderr, "Usage: %s [port number]   (default %d)\n", argv[0], DEFAULT_PORT);
        return 1;
    }
    port = (argc == 2) ? atoi(argv[1]) : DEFAULT_PORT;   /* requirements: server waits at port 69 */
    if (port < 1 || port > 65535)
    {
        fprintf(stderr, "Invalid port: %s\n", argv[1]);
        return 1;
    }

    sock = udp_bind_server(port);
    if (sock < 0) return 1;

    signal(SIGCHLD, SIG_IGN);                /* finished children are reaped automatically */
    printf("INFO: TFTP server waiting on port %d\n", port);
    fflush(stdout);

    for (;;)                                 /* the server never exits on its own */
    {
        packetbuffer_t buf[PACKETSIZE];
        struct sockaddr_in client;
        socklen_t cl = sizeof client;
        packet_t pkt;
        pid_t pid;
        int n = recvfrom(sock, buf, sizeof buf, 0, (struct sockaddr *)&client, &cl);

        if (n < 0)
        {
            if (errno != EINTR) perror("recvfrom");
            continue;
        }
        if (packet_parse(buf, n, &pkt) < 0 || !(IS_RRQ(pkt.opcode) || IS_WRQ(pkt.opcode)))
        {
            if (n < 2 || packet_extract_opcode(buf) != OPCODE_ERROR)   /* never answer an ERROR */
                tftp_send_error(sock, &client, ECODE_4, ESTRING_4);
            continue;
        }

        pid = fork();
        if (pid < 0)
        {
            perror("fork");
            tftp_send_error(sock, &client, ECODE_0, "Server busy.");
        }
        else if (pid == 0)
        {
            close(sock);                     /* child must not touch the public socket */
            serve_client(&client, &pkt, next_tid);
        }
        else
        {
            next_tid++;                      /* next request gets the next private port */
            if (next_tid > 65000) next_tid = FIRST_TID_PORT;
        }
    }
}