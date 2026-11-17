/*############################################################################################### 
 *
 *           Author         : SHUBHAM SHIVPUJIMATH
 *           Date           : 07-10-2026
 *           File           : client.c©
 *           Descriptions   : Client side of tftp protocol
 *           Objective      : To send/recive file to/from server
 *           Usage          : ./client <server ip> <port> <filename> <output filename> [get|put]
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
#include "udp.h"
#include "tftp.h"

/* Client code for tftp Protocol */

#include <strings.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "file.h"

/* Client code for tftp Protocol: an interactive "tftp>" prompt. */

static struct
{
    int connected;
    struct sockaddr_in server;
    char mode[16];
} state = { 0, {0}, MODE_NETASCII };

static void print_help(void)
{
    printf("Commands:\n"
           "  connect <ip> [port]      Connect to server\n"
           "  get <file> [local]       Receive file from server\n"
           "  put <file> [remote]      Send file to server\n"
           "  mode <octet|netascii>    Set transfer mode\n"
           "  bye / quit               Exit client\n"
           "  help                     Show this help\n");
}

static int not_connected(void)
{
    if (state.connected) return 0;
    printf("Not connected. Use: connect <ip> [port]\n");
    return 1;
}

static void cmd_connect(const char *host, const char *portstr)
{
    struct addrinfo hints, *res;
    int port = portstr ? atoi(portstr) : DEFAULT_PORT;

    if (!host) { printf("Usage: connect <ip> [port]\n"); return; }
    if (port < 1 || port > 65535) { printf("Invalid port\n"); return; }

    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(host, NULL, &hints, &res) != 0)
    {
        printf("Invalid server address\n");
        return;
    }
    state.server = *(struct sockaddr_in *)res->ai_addr;
    state.server.sin_port = htons(port);
    freeaddrinfo(res);
    state.connected = 1;                 /* UDP: nothing is sent until a transfer starts */
    printf("Connected to %s:%d\n", inet_ntoa(state.server.sin_addr), port);
}

/* get <remote> [local]: download into "<local>.tmp" and rename on success,
 * so a failed transfer never destroys an existing local file. */
static void cmd_get(const char *remote, const char *local)
{
    packetbuffer_t rrq[PACKETSIZE];
    char tmp[MAXFILENAME + 8];
    peer_t peer;
    int fd, sock, len, r;

    if (not_connected()) return;
    if (!remote || strlen(remote) > MAXFILENAME) { printf("Usage: get <file> [local]\n"); return; }
    if (!local) local = remote;
    if (strlen(local) > MAXFILENAME) { printf("Local name too long\n"); return; }

    snprintf(tmp, sizeof tmp, "%s.tmp", local);
    unlink(tmp);                                   /* stale leftover from an earlier failure */
    fd = file_open_write(tmp);
    if (fd < 0) { printf("Cannot create %s: %s\n", local, strerror(errno)); return; }

    sock = udp_bind_client();
    if (sock < 0) { file_close(fd); unlink(tmp); return; }

    peer.addr   = state.server;          /* well-known port; adopts the server's TID on reply */
    peer.locked = 0;
    len = packet_form_rrq(rrq, remote, state.mode);
    r = tftp_receive_file(sock, &peer, fd, rrq, len);

    file_close(fd);
    close(sock);
    if (r == 0 && rename(tmp, local) == 0) printf("File received successfully\n");
    else                                    unlink(tmp);
}

static void cmd_put(const char *local, const char *remote)
{
    packetbuffer_t wrq[PACKETSIZE];
    peer_t peer;
    int fd, sock, len;

    if (not_connected()) return;
    if (!local || strlen(local) > MAXFILENAME) { printf("Usage: put <file> [remote]\n"); return; }
    if (!remote) remote = local;
    if (strlen(remote) > MAXFILENAME) { printf("Remote name too long\n"); return; }

    fd = file_open_read(local);
    if (fd < 0) { printf("Cannot open %s: %s\n", local, strerror(errno)); return; }

    sock = udp_bind_client();
    if (sock < 0) { file_close(fd); return; }

    peer.addr   = state.server;
    peer.locked = 0;
    len = packet_form_wrq(wrq, remote, state.mode);

    /* WRQ is answered by ACK 0 from the server's private port, then DATA 1... */
    if (tftp_send_and_wait_ack(sock, &peer, wrq, len, 0) == 0 &&
        tftp_send_file(sock, &peer, fd) == 0)
        printf("File sent successfully\n");

    file_close(fd);
    close(sock);
}

static void cmd_mode(const char *m)
{
    if (!m) { printf("Current mode: %s\nUsage: mode <octet|netascii>\n", state.mode); return; }
    if (!strcasecmp(m, MODE_NETASCII) || !strcasecmp(m, MODE_OCTET))
    {
        snprintf(state.mode, sizeof state.mode, "%s", !strcasecmp(m, MODE_OCTET) ? MODE_OCTET : MODE_NETASCII);
        printf("Mode set to %s\n", state.mode);
    }
    else printf("Unknown mode. Use octet or netascii\n");
}

int main(int argc, char *argv[])
{
    char line[1024];

    tftp_verbose = 1;                    /* show TFTP error / timeout messages */
    print_help();

    if (argc >= 2) cmd_connect(argv[1], argc >= 3 ? argv[2] : NULL);

    for (;;)
    {
        char *cmd, *arg1, *arg2;

        printf("tftp> "); fflush(stdout);
        if (!fgets(line, sizeof line, stdin)) { printf("\n"); break; }   /* EOF */

        cmd = strtok(line, " \t\r\n");
        if (!cmd) continue;
        arg1 = strtok(NULL, " \t\r\n");
        arg2 = strtok(NULL, " \t\r\n");

        if      (!strcmp(cmd, "connect")) cmd_connect(arg1, arg2);
        else if (!strcmp(cmd, "get"))     cmd_get(arg1, arg2);
        else if (!strcmp(cmd, "put"))     cmd_put(arg1, arg2);
        else if (!strcmp(cmd, "mode"))    cmd_mode(arg1);
        else if (!strcmp(cmd, "help"))    print_help();
        else if (!strcmp(cmd, "bye") || !strcmp(cmd, "quit")) break;
        else printf("Unknown command. Type help.\n");
    }
    return 0;
}