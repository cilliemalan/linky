#include <stdlib.h>
#include <stdio.h>

#include "config.h"
#include "logging.h"

#include <sys/epoll.h>
#include <sys/types.h> /* See NOTES */
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/un.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>

#define MAX_EVENTS 128
#define MAX_BACKLOG 128

static bool active = false;

static void signal_hanlder(int signal)
{
    // TODO: we need a dummy file handle
    // to ping the epoll waiting instance
    // and make it unblock to exit
    exit(1);
    // info("Termination signal received, exiting gracefully...");
    // active = false;
}

static bool socket_read_all(int sfd)
{
    static uint8_t buffer[2048];
    int amt = 0;
    do
    {
        amt = read(sfd, buffer, sizeof(buffer) - 1);
        if (amt > 0)
        {
            buffer[amt] = 0;
            debugf("received: %s", buffer);
        }
    } while (amt > 0);

    bool result;
    switch (errno)
    {
    case EAGAIN:
        result = true;
        break;
    default:
        warn("Socket write error");
        warnp();
        result = false;
    }

    return result;
}

static bool socket_write_all(int sfd)
{
    return true;
}

static bool open_socket_listen(int port, int *psfd)
{
    // create the socket
    int sfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (sfd == -1)
    {
        error("Could not create socket");
        critical_errorp();
        return false;
    }

    // load port from configuration
    if (port <= 0 || port > 65535)
    {
        critical_errorf("The port %d is invalid", port);
        return false;
    }

    // create listen address structure
    struct sockaddr_in listen_address = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr = {
            .s_addr = htonl(INADDR_ANY),
        },
    };

    // set socket to reuse the address as they linger sometimes
    int opt = 1;
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, (socklen_t)sizeof(opt)) == -1)
    {
        errorf("Could not set SO_REUSEADDR on socket");
        critical_errorp();
        return false;
    }

    // bind the socket to the address
    if (bind(sfd, (struct sockaddr *)&listen_address, sizeof(listen_address)) == -1)
    {
        errorf("Could not bind socket to address 0.0.0.0:%d", port);
        critical_errorp();
        return false;
    }

    // listen for connections
    if (listen(sfd, MAX_BACKLOG) == -1)
    {
        errorf("Could not listen on 0.0.0.0:%d", port);
        critical_errorp();
        return false;
    }

    infof("Listening on port %d", port);
    if (psfd)
    {
        *psfd = sfd;
    }

    return true;
}

bool linky_listen()
{
    // make it catch signals
    active = true;
    signal(SIGINT, signal_hanlder);
    signal(SIGHUP, signal_hanlder);

    // create epoll structure
    int epollfd = epoll_create1(0);
    if (epollfd == -1)
    {
        error("Could not create epoll structure");
        critical_errorp();
        return false;
    }

    // get port config
    const config_t *config = config_get();
    int port_http = strtol(config->port, NULL, 10);
    int port_https = strtol(config->secure_port, NULL, 10);

    // open listen sockets
    int listen_socket_http;
    int listen_socket_https;
    if (!open_socket_listen(port_http, &listen_socket_http))
    {
        return false;
    }
    if (port_https)
    {
        if (!open_socket_listen(port_https, &listen_socket_https))
        {
            return false;
        }
    }

    // attach the listening sockets to epoll
    struct epoll_event evt = {
        .events = EPOLLIN,
        .data = {
            .fd = listen_socket_http,
        }};
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_socket_http, &evt) == -1)
    {
        error("Could not register listening socket with epoll");
        critical_errorp();
        return false;
    }
    if (port_https)
    {
        evt.data.fd = listen_socket_https;
        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_socket_https, &evt) == -1)
        {
            error("Could not register listening socket with epoll");
            critical_errorp();
            return false;
        }
    }

    // "poll" for events
    struct epoll_event events[MAX_EVENTS];
    while (active)
    {
        int nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        debugf("got %d events", nfds);

        for (int i = 0; active && i < nfds; i++)
        {
            struct epoll_event *evt = &events[i];
            // check if this is an accept
            if (evt->data.fd == listen_socket_http || evt->data.fd == listen_socket_https)
            {
                struct sockaddr_in client_addr;
                size_t client_len = sizeof(client_addr);

                int connection = accept(
                    evt->data.fd,
                    (struct sockaddr *)&client_addr,
                    (socklen_t *)&client_len);
                if (connection != -1)
                {
                    // set the socket as nonblocking
                    int err = fcntl(connection,
                                    F_SETFL,
                                    fcntl(connection, F_GETFL, 0) | O_NONBLOCK);
                    if (err == -1)
                    {
                        warn("Could not set socket as nonblocking");
                        warnp();
                    }

                    if (err != -1)
                    {
                        int flag = 1;
                        err = setsockopt(connection, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
                        if (err == -1)
                        {
                            warn("Could not set TCP_NODELAY on socket");
                            warnp();
                        }
                    }

                    // poll for events for this socket
                    struct epoll_event nevt = {
                        .events = EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLERR | EPOLLRDHUP | EPOLLPRI | EPOLLET,
                        .data = {
                            .fd = connection}};
                    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, connection, &nevt) != -1)
                    {
                        // TODO: log connection info
                        debug("connection accepted");
                    }
                    else
                    {
                        warn("Could not register new socket with epoll");
                        warnp();
                    }
                }
                else
                {
                    warn("could not accept incoming connection");
                    warnp();
                }
            }
            else
            {
                // otherwise this is data or something to do
                // with a client connection
                if (evt->events & EPOLLIN)
                {
                    // data is ready to be read
                    if (!socket_read_all(evt->data.fd))
                    {
                        close(evt->data.fd);
                    }
                }
                if (evt->events & EPOLLOUT)
                {
                    // the socket is ready for sending now
                    if (!socket_write_all(evt->data.fd))
                    {
                        close(evt->data.fd);
                    }
                }
                if (evt->events & EPOLLRDHUP)
                {
                    // the socket is ready for sending now
                    debug("Socket remote hangup");
                    close(evt->data.fd);
                }
                if (evt->events & EPOLLHUP)
                {
                    // the socket is ready for sending now
                    debug("Socket hangup");
                    close(evt->data.fd);
                }
                if (evt->events & EPOLLERR)
                {
                    // the socket is ready for sending now
                    debug("Socket error");
                    close(evt->data.fd);
                }
                if (evt->events & EPOLLPRI)
                {
                    // the socket is ready for sending now
                    debug("Socket exceptional condition");
                    close(evt->data.fd);
                }
            }
        }
    }

    // stop epolling
    close(epollfd);
    // stop listening
    close(listen_socket_http);
    close(listen_socket_https);
    // TODO: close all connections

    return true;
}