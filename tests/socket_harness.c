/*
 * SpanDSP - a series of DSP components for telephony
 *
 * socket_harness.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2007 Steve Underwood
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


/*! \page socket_harness_page Socket harness
\section socket_harness_page_sec_1 What does it do?

\section socket_harness_page_sec_2 How is it used?
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#define _GNU_SOURCE

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
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
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>

#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES

#include "spandsp.h"

#include "pseudo_terminals.h"
#include "socket_harness.h"

//#define SIMULATE_RING 1

#define CLOSE_COUNT_MAX 100

/* static data */
static int16_t inbuf[4096];
static int16_t outbuf[4096];

static volatile sig_atomic_t keep_running = true;
span_timestamp_t socket_harness_timer = 0;

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

int socket_harness_terminal_write(void *user_data, const uint8_t *buf, size_t len)
{
    socket_harness_state_t *s;
    
    s = (socket_harness_state_t *) user_data;
    return write(s->pty_fd, buf, len);
}
/*- End of function --------------------------------------------------------*/

int socket_harness_run(socket_harness_state_t *s, int kick)
{
    struct timeval tmo;
    struct timeval *tmo_ptr;
    fd_set rset;
    fd_set eset;
    struct termios termios;
    int max_fd;
    int count;
    int samples;
    int tx_samples;
    int ret;
    span_timestamp_t waiter;
    span_timestamp_t now;

    if (kick)
    {
        samples = 160;
        tx_samples = s->tx_callback(s->user_data, outbuf, samples);
        if (tx_samples < samples)
            memset(&outbuf[tx_samples], 0, (samples - tx_samples)*2);

        if ((count = write(s->net_fd, outbuf, samples*2)) < 0)
        {
            if (errno != EAGAIN)
            {
                fprintf(stderr, "Error: audio write: %s\n", strerror(errno));
                return -1;
            }
            /* TODO: */
        }
    }
    while (keep_running)
    {
        if (socket_harness_timer)
        {
            now = now_us();
            if (now >= socket_harness_timer)
                waiter = 1;
            else
                waiter = socket_harness_timer - now;
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
        /*endif*/
        if (ret == 0)
        {
            /* Timeout */
#if defined(SIMULATE_RING)
            if (!modem->modem->started)
            {
                rcount++;
                if (rcount <= RING_ON)
                    modem_ring(modem->modem);
                else if (rcount > RING_OFF)
                    rcount = 0;
                /*endif*/
            }
            /*endif*/
#else
            s->timer_callback(s->user_data);
#endif
            continue;
        }
        /*endif*/

        if (FD_ISSET(s->net_fd, &rset))
        {
            if ((count = read(s->net_fd, inbuf, sizeof(inbuf)/2)) < 0)
            {
                if (errno != EAGAIN)
                {
                    fprintf(stderr, "Error: audio read: %s\n", strerror(errno));
                    return -1;
                }
                /*endif*/
                count = 0;
            }
            /*endif*/
            if (count == 0)
            {
                fprintf(stderr, "Audio socket closed\n");
                return 0;
            }
            /*endif*/
            samples = count/2;
            usleep(125*samples);

            s->rx_callback(s->user_data, inbuf, samples);
            tx_samples = s->tx_callback(s->user_data, outbuf, samples);
            if (tx_samples < samples)
                memset(&outbuf[tx_samples], 0, (samples - tx_samples)*2);
            /*endif*/

            if ((count = write(s->net_fd, outbuf, samples*2)) < 0)
            {
                if (errno != EAGAIN)
                {
                    fprintf(stderr, "Error: audio write: %s\n", strerror(errno));
                    return -1;
                }
                /*endif*/
                /* TODO: */
            }
            /*endif*/
            if (count != samples*2)
                fprintf(stderr, "audio write = %d\n", count);
            /*endif*/
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
            if ((count = s->terminal_free_space_callback(s->user_data)))
            {
                if (count > sizeof(inbuf))
                    count = sizeof(inbuf);
                /*endif*/
                if ((count = read(s->pty_fd, inbuf, count)) < 0)
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
                                /*endif*/
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
                    if (count == 0)
                        fprintf(stderr, "pty read = 0\n");
                    /*endif*/
                    s->pty_closed = false;
                    s->terminal_callback(s->user_data, (uint8_t *) inbuf, count);
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

socket_harness_state_t *socket_harness_init(socket_harness_state_t *s,
                                            const char *socket_name,
                                            const char *tag,
                                            int caller,
                                            span_put_msg_func_t terminal_callback,
                                            termio_update_func_t termios_callback,
                                            span_modem_status_func_t hangup_callback,
                                            put_msg_free_space_func_t terminal_free_space_callback,
                                            span_rx_handler_t rx_callback,
                                            span_rx_fillin_handler_t rx_fillin_callback,
                                            span_tx_handler_t tx_callback,
                                            void *user_data)
{
    int sockfd;
    int listensockfd;
    struct sockaddr_un serv_addr;
    struct sockaddr_un cli_addr;
    socklen_t servlen;
    socklen_t clilen;

    if ((sockfd = socket(AF_LOCAL, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "Socket failed - errno = %d\n", errno);
        return NULL;
    }

    if (s == NULL)
    {
        if ((s = (socket_harness_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));

    signal(SIGINT, log_signal);
    signal(SIGTERM, log_signal);

    s->terminal_callback = terminal_callback;
    s->termios_callback = termios_callback;
    s->hangup_callback = hangup_callback;
    s->terminal_free_space_callback = terminal_free_space_callback;

    s->rx_callback = rx_callback;
    s->rx_fillin_callback = rx_fillin_callback;
    s->tx_callback = tx_callback;

    s->user_data = user_data;

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sun_family = AF_LOCAL;
    /* This is a generic Unix domain socket. */
    strcpy(serv_addr.sun_path, socket_name);
    printf("Creating socket '%s'\n", serv_addr.sun_path);
    servlen = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family) + 1;
    if (caller)
    {
        fprintf(stderr, "Connecting to '%s'\n", serv_addr.sun_path);
        if (connect(sockfd, (struct sockaddr *) &serv_addr, servlen) < 0)
        {
            fprintf(stderr, "Connect failed - errno = %d\n", errno);
            exit(2);
        }
        fprintf(stderr, "Connected to '%s'\n", serv_addr.sun_path);
    }
    else
    {
        fprintf(stderr, "Listening to '%s'\n", serv_addr.sun_path);
        listensockfd = sockfd;
        /* The file may or may not exist. Just try to delete it anyway. */
        unlink(serv_addr.sun_path);
        if (bind(listensockfd, (struct sockaddr *) &serv_addr, servlen) < 0)
        {
            fprintf(stderr, "Bind failed - errno = %d\n", errno);
            exit(2);
        }
        listen(listensockfd, 5);
        clilen = sizeof(cli_addr);
        if ((sockfd = accept(listensockfd, (struct sockaddr *) &cli_addr, &clilen)) < 0)
        {
            fprintf(stderr, "Accept failed - errno = %d", errno);
            exit(2);
        }
        fprintf(stderr, "Accepted on '%s'\n", serv_addr.sun_path);
    }
    if (pseudo_terminal_init(&s->pty) == NULL)
    {
        fprintf(stderr, "Failed to create pseudo TTY\n");
        exit(2);
    }
    s->net_fd = sockfd;
    s->pty_fd = s->pty.master_fd;
    return s;
}
/*- End of function --------------------------------------------------------*/

int socket_harness_release(socket_harness_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

int socket_harness_free(socket_harness_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
