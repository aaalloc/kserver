#pragma once

int open_lsocket(struct socket **socket, int port);
int close_lsocket(struct socket *socket);