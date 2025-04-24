#pragma once
#include <linux/tcp.h>

static int open_listen_socket(struct socket **socket, int port);