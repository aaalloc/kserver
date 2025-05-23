#pragma once

struct ksocket_handler
{
    struct socket *sock;
    unsigned char *buf;
    int len;
};

int ksocket_write(struct ksocket_handler handler);
int ksocket_read(struct ksocket_handler handler);
int open_lsocket(struct socket **socket, int port);
int close_lsocket(struct socket *socket);