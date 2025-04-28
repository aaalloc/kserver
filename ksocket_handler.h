#pragma once

int ksocket_read(struct socket *sock, unsigned char *buf, int len);
int open_lsocket(struct socket **socket, int port);
int close_lsocket(struct socket *socket);