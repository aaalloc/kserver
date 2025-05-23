#include <linux/init.h>   // For __init and __exit macros
#include <linux/kernel.h> // For KERN_INFO, KERN_WARNING, and KERN_ERR
#include <linux/list.h>   // For linked list operations
#include <linux/module.h> // For module macros
#include <linux/slab.h>   // For memory allocation functions
#include <linux/tcp.h>    // For socket
#include <linux/thread_info.h>
#include <linux/types.h>         // For data type definitions
#include <linux/workqueue_api.h> // For deferred work
#include <net/sock.h>

#include "ksocket_handler.h"
#include <linux/errno.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/signal.h>

int ksocket_read(struct ksocket_handler handler)
{
    struct socket *sock = handler.sock;
    unsigned char *buf = handler.buf;
    int len = handler.len;

    struct msghdr msg = {0};
    struct kvec vec;
    int ret;

    // don't know if its worth it do that, user has just to use wisely the len
    // memset(buf, 0, len);

    vec.iov_base = buf;
    vec.iov_len = len;

    ret = kernel_recvmsg(sock, &msg, &vec, 1, len, msg.msg_flags);
    if (ret < 0)
        pr_err("%s: kernel_recvmsg failed: %d\n", THIS_MODULE->name, ret);
    return ret;
}

int ksocket_write(struct ksocket_handler handler)
{
    struct socket *sock = handler.sock;
    unsigned char *buf = handler.buf;
    int len = handler.len;

    struct msghdr msg = {0};
    struct kvec vec;
    int ret;

    vec.iov_base = buf;
    vec.iov_len = len;

    ret = kernel_sendmsg(sock, &msg, &vec, 1, len);
    if (ret < 0)
        pr_err("%s: kernel_sendmsg failed: %d\n", THIS_MODULE->name, ret);
    return ret;
}

int open_lsocket(struct socket **result, int port)
{
    struct socket *sock;
    struct sockaddr_in addr;
    int error;
    int opt = 1;
    sockptr_t kopt = {.kernel = (char *)&opt, .is_kernel = 1};

    struct net *net = current->nsproxy->net_ns;

    // IPv4, TCP
    error = sock_create_kern(net, PF_INET, SOCK_STREAM, IPPROTO_TCP, &sock);
    if (error < 0)
    {
        pr_err("%s: socket_create failed: %d\n", THIS_MODULE->name, error);
        return error;
    }

    error = sock_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, kopt, sizeof(opt));
    if (error < 0)
    {
        pr_err("%s: kernel_setsockopt failed: %d\n", THIS_MODULE->name, error);
        goto err_setsockopt;
    }

    error = sock_setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, kopt, sizeof(opt));
    if (error < 0)
    {
        pr_err("%s: kernel_setsockopt failed: %d\n", THIS_MODULE->name, error);
        goto err_setsockopt;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    // TODO: use in4_pton with a addr_str
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    error = kernel_bind(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (error < 0)
    {
        pr_err("%s: kernel_bind failed: %d\n", THIS_MODULE->name, error);
        goto err_bind;
    }

    error = kernel_listen(sock, 128);
    if (error < 0)
    {
        pr_err("%s: kernel_listen failed: %d\n", THIS_MODULE->name, error);
        goto err_bind;
    }

    pr_info("%s: binding to port %d\n", THIS_MODULE->name, port);
    *result = sock;
    return 0;
err_bind:
    kernel_sock_shutdown(sock, SHUT_RDWR);
err_setsockopt:
    sock_release(sock);
    return error;
}

int close_lsocket(struct socket *sock)
{
    int res;
    if (sock)
    {
        res = kernel_sock_shutdown(sock, SHUT_RDWR);
        if (res < 0)
        {
            pr_err("%s: kernel_sock_shutdown failed: %d\n", THIS_MODULE->name, res);
            return res;
        }
        sock_release(sock);
    }
    return 0;
}