/*
 * SpanDSP - a series of DSP components for telephony
 *
 * socket_dgram_harness.h
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2012, 2022 Steve Underwood
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

#include <sys/socket.h>
#include <sys/un.h>

typedef int (*termio_update_func_t)(void *user_data, struct termios *termios);

typedef int (*put_msg_free_space_func_t)(void *user_data);

typedef void (*span_timer_handler_t)(void *user_data);

typedef struct socket_dgram_harness_state_s
{
    void *user_data;

    span_put_msg_func_t terminal_callback;
    termio_update_func_t termios_callback;
    span_modem_status_func_t hangup_callback;
    put_msg_free_space_func_t terminal_free_space_callback;

    span_put_msg_func_t  rx_callback;
    span_get_msg_func_t  tx_callback;
    span_timer_handler_t timer_callback;

    int net_fd;
    int pty_fd;
    logging_state_t logging;
    struct termios termios;

    struct sockaddr_un local_addr;
    socklen_t local_addr_len;
    struct sockaddr_un far_addr;
    socklen_t far_addr_len;

    unsigned int delay;
    unsigned int started;
    unsigned pty_closed;
    unsigned close_count;
    
    pseudo_terminal_state_t pty;
} socket_dgram_harness_state_t;

extern span_timestamp_t socket_dgram_harness_timer;

span_timestamp_t now_us(void);

int socket_dgram_harness_run(socket_dgram_harness_state_t *s);

int socket_dgram_harness_terminal_write(void *user_data, const uint8_t *buf, size_t len);

int socket_dgram_harness_set_user_data(socket_dgram_harness_state_t *s, void *user_data);

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
                                                        void *user_data);

int socket_dgram_harness_release(socket_dgram_harness_state_t *s);

int socket_dgram_harness_free(socket_dgram_harness_state_t *s);
