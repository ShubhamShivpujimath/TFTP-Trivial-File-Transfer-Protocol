#ifndef TFTP_UDP
#define TFTP_UDP

#include <sys/socket.h>
#include <netinet/in.h>
#include "tftp.h"

int udp_bind_server(int port);          /* well-known socket, blocking            */
int udp_rebind_server(int *port);       /* private socket, first free port >= *port */
int udp_bind_client(void);              /* ephemeral socket with receive timeout  */
int udp_send_packet(int sock, const packetbuffer_t *buf, int len, const struct sockaddr_in *to);

#endif