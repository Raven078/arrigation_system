#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <stdint.h>
#include <stdbool.h>

void tcp_server_start(uint16_t port);
void tcp_send_command(const char *client_name, const char *cmd);
void tcp_register_client(const char *name, const char *ip, int cmd_port);

#endif