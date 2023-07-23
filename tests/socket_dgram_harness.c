/*
 * SpanDSP - a series of DSP components for telephony
 *
 * socket_dgram_harness.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2007, 2012, 2022 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


/*! \page socket_dgram_harness_page Socket harness
\section socket_dgram_harness_page_sec_1 What does it do?

\section socket_dgram_harness_page_sec_2 How is it used?
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#define _GNU_SOURCE

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>

#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES

#include "spandsp.h"

#include "pseudo_terminals.h"
#include "socket_dgram_harness.h"

#define CLOSE_COUNT_MAX 100

/* static data */
static uint8_t inbuf[4096];

static volatile sig_atomic_t keep_running = true;
span_timestamp_t socket_dgram_harness_timer = 0;

span_timestamp_t now_us(void)
{
    span_timestamp_t now;
    struct timeval tm;

    gettimeofday(&tm, NULL);
    now = tm.tv_sec*1000000 + tm.tv_usec;
    return now;
}
/*- End of function --------------------------------------------------------*/

static void log_signal(int signum)
{
    fprintf(stderr, "Signal %d: mark termination.\n", signum);
    keep_running = false;
    exit(2);
}
/*- End of function --------------------------------------------------------*/

int socket_dgram_harness_terminal_write(void *user_data, const uint8_t *buf, size_t len)
{
    socket_dgram_harness_state_t *s;
    
    s = (socket_dgram_harness_state_t *) user_data;
    return write(s->pty_fd, buf, len);
}
/*- End of function --------------------------------------------------------*/

int socket_dgram_harness_run(socket_dgram_harness_state_t *s)
{
    struct timeval tmo;
    struct timeval *tmo_ptr;
    fd_set rset;
    fd_set eset;
    struct termios termios;
    int max_fd;
    int len;
    int ret;
    struct sockaddr_un far_addr;
    socklen_t far_addr_len;
    span_timestamp_t waiter;
    span_timestamp_t now;
    uint8_t pkt[4096];

    while (keep_running)
    {
        if (socket_dgram_harness_timer)
        {
            now = now_us();
            if (now >= socket_dgram_harness_timer)
                waiter = 1;
            else
                waiter = socket_dgram_harness_timer - now;
            /*endif*/
            tmo.tv_sec = waiter/1000000;
            tmo.tv_usec = waiter%1000000;
            tmo_ptr = &tmo;
        }
        else
        {
            tmo_ptr = NULL;
        }
        /*endif*/
        max_fd = 0;
        FD_ZERO(&rset);
        FD_ZERO(&eset);
        FD_SET(s->net_fd, &rset);
        FD_SET(s->net_fd, &eset);
        FD_SET(s->pty_fd, &rset);
        FD_SET(s->pty_fd, &eset);
        if (s->net_fd > max_fd)
            max_fd = s->net_fd;
        /*endif*/
        if (s->pty_fd > max_fd)
            max_fd = s->pty_fd;
        /*endif*/
        if (s->pty_closed  &&  s->close_count)
        {
            if (!s->started  ||  s->close_count++ > CLOSE_COUNT_MAX)
                s->close_count = 0;
            /*endif*/
        }
        else if (s->terminal_free_space_callback(s->user_data))
        {
            FD_SET(s->pty_fd, &rset);
            if (s->pty_fd > max_fd)
                max_fd = s->pty_fd;
            /*endif*/
        }
        /*endif*/
        if ((ret = select(max_fd + 1, &rset, NULL, &eset, tmo_ptr)) < 0)
        {
            if (errno == EINTR)
                continue;
            /*endif*/
            fprintf(stderr, "Error: select: %s\n", strerror(errno));
            return ret;
        }
        if (ret == 0)
        {
            /* Timeout */
            s->timer_callback(s->user_data);
            continue;
        }
        /*endif*/

        if (FD_ISSET(s->net_fd, &rset))
        {
            if ((len = recvfrom(s->net_fd, pkt, sizeof(pkt), 0, (struct sockaddr *) &far_addr, &far_addr_len)) < 0)
            {
                if (errno != EAGAIN)
                {
                    fprintf(stderr, "Error: net read: %s\n", strerror(errno));
                    return -1;
                }
                /*endif*/
                len = 0;
            }
            /*endif*/
            if (len == 0)
            {
                fprintf(stderr, "Net socket closed\n");
                return 0;
            }
            /*endif*/

            s->rx_callback(s->user_data, pkt, len);
        }
        /*endif*/

        if (FD_ISSET(s->pty_fd, &rset))
        {
            /* Check termios */
            tcgetattr(s->pty_fd, &termios);
            if (memcmp(&termios, &s->termios, sizeof(termios)))
                s->termios_callback(s->user_data, &termios);
            /*endif*/
            /* Read data */
            if ((len = s->terminal_free_space_callback(s->user_data)))
            {
                if (len > sizeof(inbuf))
                    len = sizeof(inbuf);
                /*endif*/
                if ((len = read(s->pty_fd, inbuf, len)) < 0)
                {
                    if (errno == EAGAIN)
                    {
                        fprintf(stderr, "pty read, errno = EAGAIN\n");
                    }
                    else
                    {
                        if (errno == EIO)
                        {
                            if (!s->pty_closed)
                            {
                                fprintf(stderr, "pty closed.\n");
                                s->pty_closed = 1;
                                if ((termios.c_cflag & HUPCL))
                                    s->hangup_callback(s->user_data, 0);
                            }
                            /*endif*/
                            s->close_count = 1;
                        }
                        else
                        {
                            fprintf(stderr, "Error: pty read: %s\n", strerror(errno));
                            return -1;
                        }
                        /*endif*/
                    }
                    /*endif*/
                }
                else
                {
                    if (len == 0)
                        fprintf(stderr, "pty read = 0\n");
                    /*endif*/
                    s->pty_closed = false;
                    s->terminal_callback(s->user_data, (uint8_t *) inbuf, len);
                }
                /*endif*/
            }
            /*endif*/
        }
        /*endif*/
    }
    /*endwhile*/

    return 0;
}
/*- End of function --------------------------------------------------------*/

int socket_dgram_harness_set_user_data(socket_dgram_harness_state_t *s, void *user_data)
{
    s->user_data = user_data;
    return 0;
}
/*- End of function --------------------------------------------------------*/

socket_dgram_harness_state_t *socket_dgram_harness_init(socket_dgram_harness_state_t *s,
                                                        const char *local_socket_name,
                                                        const char *far_socket_name,
                                                        const char *tag,
                                                        int caller,
                                                        span_put_msg_func_t terminal_callback,
                                                        termio_update_func_t termios_callback,
                                                        span_modem_status_func_t hangup_callback,
                                                        put_msg_free_space_func_t terminal_free_space_callback,
                                                        span_put_msg_func_t rx_callback,
                                                        span_get_msg_func_t tx_callback,
                                                        span_timer_handler_t timer_callback,
                                                        void *user_data)
{
    if (s == NULL)
    {
        if ((s = (socket_dgram_harness_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
        /*endif*/
    }
    /*endif*/
    memset(s, 0, sizeof(*s));

    signal(SIGINT, log_signal);
    signal(SIGTERM, log_signal);

    s->terminal_callback = terminal_callback;
    s->termios_callback = termios_callback;
    s->hangup_callback = hangup_callback;
    s->terminal_free_space_callback = terminal_free_space_callback;

    s->rx_callback = rx_callback;
    s->tx_callback = tx_callback;
    s->timer_callback = timer_callback;

    s->user_data = user_data;

    memset((char *) &s->local_addr, 0, sizeof(s->local_addr));
    s->local_addr.sun_family = AF_LOCAL;
    strncpy(s->local_addr.sun_path, local_socket_name, sizeof(s->local_addr.sun_path));
    s->local_addr.sun_path[sizeof(s->local_addr.sun_path) - 1] = '\0';
    s->local_addr_len = offsetof(struct sockaddr_un, sun_path) + strlen(s->local_addr.sun_path);

    memset((char *) &s->far_addr, 0, sizeof(s->far_addr));
    s->far_addr.sun_family = AF_LOCAL;
    strncpy(s->far_addr.sun_path, far_socket_name, sizeof(s->far_addr.sun_path));
    s->far_addr.sun_path[sizeof(s->far_addr.sun_path) - 1] = '\0';
    s->far_addr_len = offsetof(struct sockaddr_un, sun_path) + strlen(s->far_addr.sun_path);

    if ((s->net_fd = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0)
    {
        fprintf(stderr, "Socket failed - errno = %d\n", errno);
        return NULL;
    }
    /*endif*/
    /* Bind a name to the socket. */
    unlink(s->local_addr.sun_path);
    if (bind(s->net_fd, (struct sockaddr *) &s->local_addr, s->local_addr_len) < 0)
    {
        fprintf(stderr, "Bind failed - errno = %d\n", errno);
        exit(2);
    }
    /*endif*/
    if (pseudo_terminal_init(&s->pty) == NULL)
    {
        fprintf(stderr, "Failed to create pseudo TTY\n");
        exit(2);
    }
    /*endif*/
    s->pty_fd = s->pty.master_fd;
    return s;
}
/*- End of function --------------------------------------------------------*/

int socket_dgram_harness_release(socket_dgram_harness_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

int socket_dgram_harness_free(socket_dgram_harness_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
