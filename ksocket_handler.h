#pragma once

int open_lsocket(struct socket **socket, int port);
void close_lsocket(struct socket *socket);