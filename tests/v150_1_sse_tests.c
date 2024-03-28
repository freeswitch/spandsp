/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v150_1_sse_tests.c - Test V.150_1 SSE processing.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2022 Steve Underwood
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

/*! \file */

/*! \page v150_1_sse_tests_page V.150_1 tests
\section v150_1_sse_tests_page_sec_1 What does it do?
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <memory.h>
#include <ctype.h>
#include <termios.h>
#include <errno.h>

#include "spandsp.h"
#include "spandsp-sim.h"

#include "pseudo_terminals.h"
#include "socket_dgram_harness.h"

typedef struct
{
    uint8_t v;
    uint8_t p;
    uint8_t x;
    uint8_t cc;
    uint8_t m;
    uint8_t pt;
    uint16_t seq_no;
    uint32_t time_stamp;
    uint32_t ssrc;
} rtp_t;

rtp_t tx_sse_rtp;

#define PACKET_TYPE     118

socket_dgram_harness_state_t *dgram_state;
v150_1_state_t *v150_1_state;

int pace_no = 0;

span_timestamp_t pace_timer = 0;
span_timestamp_t app_timer = 0;

bool send_messages = false;

int seq = 0;
int time_stamp = 0;

/* Crude RTP routines */
static int rtp_fill(rtp_t *rtp, uint8_t *buf, int max_len, int pt, const uint8_t *signal, int signal_len, uint32_t advance)
{
    buf[0] = ((rtp->v & 0x03) << 6) | ((rtp->p & 0x01) << 5) | ((rtp->x & 0x01) << 4) | (rtp->cc & 0x0F);
    buf[1] = ((rtp->m & 0x01) << 7) | pt;
    put_net_unaligned_uint16(&buf[2], rtp->seq_no);
    put_net_unaligned_uint32(&buf[4], rtp->time_stamp);
    put_net_unaligned_uint32(&buf[8], rtp->ssrc);
    memcpy(&buf[12], signal, signal_len);
    if (advance)
        rtp->seq_no++;
    /*endif*/
    rtp->time_stamp += advance;
    return 12 + signal_len;
}
/*- End of function --------------------------------------------------------*/

static int rtp_extract(rtp_t *rtp, uint8_t *signal, int max_len, const uint8_t *buf, int len)
{
    rtp->v = (buf[0] >> 6) & 0x03;
    rtp->p = (buf[0] >> 5) & 0x01;
    rtp->x = (buf[0] >> 4) & 0x01;
    rtp->cc = buf[0] & 0x0F;
    rtp->m = (buf[1] >> 7) & 0x01;
    rtp->pt = buf[1] & 0x7F;
    rtp->seq_no = get_net_unaligned_uint16(&buf[2]);
    rtp->time_stamp = get_net_unaligned_uint32(&buf[4]);
    rtp->ssrc = get_net_unaligned_uint32(&buf[8]);
    if (signal)
        memcpy(signal, &buf[12], len - 12);
    /*endif*/
    return len - 12;
}
/*- End of function --------------------------------------------------------*/

static void terminal_callback(void *user_data, const uint8_t msg[], int len)
{
    int i;

    printf("terminal callback %d\n", len);
    for (i = 0;  i < len;  i++)
        printf("0x%x ", msg[i]);
    /*endfor*/
    printf("\n");
    /* TODO: connect AT input to V.150.1 SSE */
}
/*- End of function --------------------------------------------------------*/

static int termios_callback(void *user_data, struct termios *termios)
{
    //data_modems_state_t *s;

    //s = (data_modems_state_t *) user_data;
    printf("termios callback\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void hangup_callback(void *user_data, int status)
{
}
/*- End of function --------------------------------------------------------*/

static int terminal_free_space_callback(void *user_data)
{
    return 42;
}
/*- End of function --------------------------------------------------------*/

static void rx_callback(void *user_data, const uint8_t buf[], int len)
{
    rtp_t rtp;
    int signal_len;
    uint8_t signal[160];

    signal_len = rtp_extract(&rtp, signal, 160, buf, len);
    if (rtp.pt == PACKET_TYPE)
    {
        v150_1_sse_rx_packet((v150_1_state_t *) user_data, rtp.seq_no, rtp.time_stamp, signal, signal_len);
    }
    /*endif*/
    seq = rtp.seq_no + 1;
    time_stamp = rtp.time_stamp + 160;
}
/*- End of function --------------------------------------------------------*/

static int tx_callback(void *user_data, uint8_t buff[], int len)
{
    int res;

    res = v150_1_sse_tx_packet((v150_1_state_t *) user_data, V150_1_MEDIA_STATE_MODEM_RELAY, V150_1_SSE_RIC_V32BIS_AA, 0);
    return res;
}
/*- End of function --------------------------------------------------------*/

static int tx_packet_handler(void *user_data, bool repeat, const uint8_t pkt[], int len)
{
    int i;
    //int n;
    socket_dgram_harness_state_t *s;
    int sent_len;
    uint8_t buf[256];
    int len2;
    
    //n = (int) (intptr_t) user_data;
    fprintf(stderr, "Tx message");
    for (i = 0;  i < len;  i++)
        fprintf(stderr, " %02x", pkt[i]);
    /*endfor*/
    fprintf(stderr, "\n");

    len2 = rtp_fill(&tx_sse_rtp, buf, 256, PACKET_TYPE, pkt, len, (repeat)  ?  0  :  160);

    s = (socket_dgram_harness_state_t *) user_data;
    /* We need a packet loss mechanism here */
    //if ((rand() % 20) != 0)
    {
        //fprintf(stderr, "Pass\n");
        if ((sent_len = sendto(s->net_fd, buf, len2, 0, (struct sockaddr *) &s->far_addr, s->far_addr_len)) < 0)
        {
            if (errno != EAGAIN)
            {
                fprintf(stderr, "Error: Net write: %s\n", strerror(errno));
                return -1;
            }
            /*endif*/
            /* TODO: */
        }
        /*endif*/
        if (sent_len != len)
            fprintf(stderr, "Net write = %d\n", sent_len);
        /*endif*/
    }
    //else
    //{
    //    fprintf(stderr, "Block\n");
    //}
    /*endif*/

    return 0;
}
/*- End of function --------------------------------------------------------*/

static void paced_operations(void)
{
    //fprintf(stderr, "Pace at %lu\n", now_us());

    if (send_messages  &&  (pace_no & 0x3F) == 0)
    {
        fprintf(stderr, "Sending paced message\n");
        if (v150_1_sse_tx_packet(v150_1_state, V150_1_MEDIA_STATE_MODEM_RELAY, V150_1_SSE_RIC_V32BIS_AA, 0) != 0)
            fprintf(stderr, "ERROR: Failed to send message\n");
        /*endif*/

        //v150_1_sse_tx_packet(v150_1_sse_state, V150_1_SSE_MEDIA_STATE_MODEM_RELAY, V150_1_SSE_RIC_V8_CM, 0x123);

        //v150_1_sse_tx_packet(v150_1_sse_state, V150_1_SSE_MEDIA_STATE_MODEM_RELAY, V150_1_SSE_RIC_CLEARDOWN, V150_1_SSE_RIC_INFO_CLEARDOWN_ON_HOOK << 8);
    }
    /*endif*/

    pace_no++;
}
/*- End of function --------------------------------------------------------*/

static void timer_callback(void *user_data)
{
    span_timestamp_t now;

    now = now_us();
    if (now >= pace_timer)
    {
        //fprintf(stderr, "Pace timer expired at %lu\n", now);
        paced_operations();
        pace_timer += 20000;
    }
    /*endif*/
    if (app_timer  &&  now >= app_timer)
    {
        //fprintf(stderr, "V.150.1 SSE timer expired at %lu\n", now);
        app_timer = 0;
        v150_1_sse_timer_expired((v150_1_state_t *) user_data, now);
    }
    /*endif*/
    if (app_timer  &&  app_timer < pace_timer)
        socket_dgram_harness_timer = app_timer;
    else
        socket_dgram_harness_timer = pace_timer;
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static int v150_1_sse_tests(bool calling_party)
{
    logging_state_t *logging;

    send_messages = true; //calling_party;

    memset(&tx_sse_rtp, 0, sizeof(tx_sse_rtp));

    if ((dgram_state = socket_dgram_harness_init(NULL,
                                                 (calling_party)  ?  "/tmp/sse_socket_a"  :  "/tmp/sse_socket_b",
                                                 (calling_party)  ?  "/tmp/sse_socket_b"  :  "/tmp/sse_socket_a",
                                                 (calling_party)  ?  "C"  :  "A",
                                                 calling_party,
                                                 terminal_callback,
                                                 termios_callback,
                                                 hangup_callback,
                                                 terminal_free_space_callback,
                                                 rx_callback,
                                                 tx_callback,
                                                 timer_callback,
                                                 v150_1_state)) == NULL)
    {
        fprintf(stderr, "    Cannot start the socket harness\n");
        exit(2);
    }
    /*endif*/

    if ((v150_1_state = v150_1_sse_init(NULL,
                                        tx_packet_handler,
                                        dgram_state)) == NULL)
    {
        fprintf(stderr, "    Cannot start V.150.1 SSE\n");
        exit(2);
    }
    /*endif*/
    socket_dgram_harness_set_user_data(dgram_state, v150_1_state);

    logging = v150_1_get_logging_state(v150_1_state);
    span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_DATE);
    span_log_set_tag(logging, (calling_party)  ?  "C"  :  "A");

    v150_1_sse_set_reliability_method(v150_1_state, V150_1_SSE_RELIABILITY_BY_REPETITION, 3, 20000, 0);
    //v150_1_sse_set_reliability_method(v150_1_sse_state,
    //                                  V150_1_SSE_RELIABILITY_BY_EXPLICIT_ACK,
    //                                  V150_1_SSE_DEFAULT_N0,
    //                                  V150_1_SSE_DEFAULT_T0,
    //                                  V150_1_SSE_DEFAULT_T1);

    socket_dgram_harness_timer = 
    pace_timer = now_us() + 20000;
    socket_dgram_harness_run(dgram_state);

    return 0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int opt;
    bool calling_party;

    calling_party = false;
    while ((opt = getopt(argc, argv, "ac")) != -1)
    {
        switch (opt)
        {
        case 'a':
            calling_party = false;
            break;
        case 'c':
            calling_party = true;
            break;
        default:
            //usage();
            exit(2);
            break;
        }
        /*endswitch*/
    }
    /*endwhile*/

    if (v150_1_sse_tests(calling_party))
        exit(2);
    /*endif*/
    printf("Tests passed\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
