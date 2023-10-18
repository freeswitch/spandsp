/*
 * SpanDSP - a series of DSP components for telephony
 *
 * sprt_tests.c - Tests for the V.150.1 SPRT protocol connected together by sockets.
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

/*! \page sprt_tests_page SPRT tests
\section sprt_tests_page_sec_1 What does it do?
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <termios.h>
#include <sndfile.h>
#include <errno.h>

//#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES

#include "spandsp.h"
#include "spandsp-sim.h"

#include "pseudo_terminals.h"
#include "socket_dgram_harness.h"

#define bump_sprt_seq_no(x) ((x) = ((x) + 1) & 0x3FFF)

sprt_state_t *sprt_state;
socket_dgram_harness_state_t *dgram_state;

uint16_t tx_seq_no[SPRT_CHANNELS];
uint16_t rx_seq_no[SPRT_CHANNELS];

int max_payloads[SPRT_CHANNELS];

int pace_no = 0;

span_timestamp_t pace_timer = 0;
span_timestamp_t sprt_timer = 0;

bool send_messages = false;

static void terminal_callback(void *user_data, const uint8_t msg[], int len)
{
    int i;

    printf("terminal callback %d\n", len);
    for (i = 0;  i < len;  i++)
    {
        printf("0x%x ", msg[i]);
    }
    printf("\n");
    /* TODO: connect AT input to SPRT */
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

static void rx_callback(void *user_data, const uint8_t buff[], int len)
{
    sprt_rx_packet((sprt_state_t *) user_data, buff, len);
}
/*- End of function --------------------------------------------------------*/

static int tx_callback(void *user_data, uint8_t buff[], int len)
{
    int res;

    res = sprt_tx((sprt_state_t *) user_data, SPRT_TCID_RELIABLE_SEQUENCED, buff, len);
    return res;
}
/*- End of function --------------------------------------------------------*/

static int tx_packet_handler(void *user_data, const uint8_t pkt[], int len)
{
    socket_dgram_harness_state_t *s;
    int sent_len;

    s = (socket_dgram_harness_state_t *) user_data;
    /* We need a packet loss mechanism here */
    if ((rand() % 20) != 0)
    {
        fprintf(stderr, "Pass\n");
        if ((sent_len = sendto(s->net_fd, pkt, len, 0, (struct sockaddr *) &s->far_addr, s->far_addr_len)) < 0)
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
    else
    {
        fprintf(stderr, "Block\n");
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int rx_delivery_handler(void *user_data, int channel, int seq_no, const uint8_t msg[], int len)
{
    uint8_t buf[2000];
    int len2;

    printf("Delivered %d, %d, %d - '%s'\n", channel, seq_no, len, msg);
    fprintf(stderr, "Delivered %d, %d, %d - '%s'\n", channel, seq_no, len, msg);
    switch (channel)
    {
    case SPRT_TCID_UNRELIABLE_UNSEQUENCED:
        bump_sprt_seq_no(rx_seq_no[SPRT_TCID_UNRELIABLE_UNSEQUENCED]);
        if (seq_no != 0)
        {
            fprintf(stderr, "ERROR: Unsequenced channel packet received with a non-zero sequence number - %d\n", seq_no);
        }
        /*endif*/
        len2 = sprintf((char *) buf, "Unreliable unsequenced %d", rx_seq_no[SPRT_TCID_UNRELIABLE_UNSEQUENCED]) + 1;
        if (len2 != len)
        {
            fprintf(stderr, "ERROR: length mismatch - %d %d - '%s '%s'\n", len, len2, msg, buf);
        }
        else if (memcmp(msg, buf, len) != 0)
        {
            fprintf(stderr, "ERROR: message mismatch - '%s '%s'\n", msg, buf);
        }
        /*endif*/
        break;
    case SPRT_TCID_RELIABLE_SEQUENCED:
        if (seq_no != ((rx_seq_no[SPRT_TCID_RELIABLE_SEQUENCED] + 1) & 0x3FFF))
        {
            fprintf(stderr,
                    "ERROR: Reliable sequenced channel packet received with a non-consecutive sequence number - %d, expected %d\n",
                    seq_no,
                    rx_seq_no[SPRT_TCID_RELIABLE_SEQUENCED]);
        }
        /*endif*/
        rx_seq_no[SPRT_TCID_RELIABLE_SEQUENCED] = seq_no;
        len2 = sprintf((char *) buf, "Reliable sequenced %d", rx_seq_no[SPRT_TCID_RELIABLE_SEQUENCED]) + 1;
        if (len2 != len)
        {
            fprintf(stderr, "ERROR: length mismatch - %d %d - '%s '%s'\n", len, len2, msg, buf);
        }
        else if (memcmp(msg, buf, len) != 0)
        {
            fprintf(stderr, "ERROR: message mismatch - '%s '%s'\n", msg, buf);
        }
        /*endif*/
        //sprt_set_local_busy(???, channel, true);
        break;
    case SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED:
        if (seq_no != ((rx_seq_no[SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED] + 1) & 0x3FFF))
        {
            fprintf(stderr,
                    "ERROR: Expedited reliable sequenced channel packet received with a non-consecutive sequence number - %d, expected %d\n",
                    seq_no,
                    rx_seq_no[SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED]);
        }
        /*endif*/
        rx_seq_no[SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED] = seq_no;
        len2 = sprintf((char *) buf, "Expedited reliable sequenced %d", rx_seq_no[SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED]) + 1;
        if (len2 != len)
        {
            fprintf(stderr, "ERROR: length mismatch - %d %d - '%s '%s'\n", len, len2, msg, buf);
        }
        else if (memcmp(msg, buf, len) != 0)
        {
            fprintf(stderr, "ERROR: message mismatch - '%s '%s'\n", msg, buf);
        }
        /*endif*/
        //sprt_set_local_busy(???, channel, true);
        break;
    case SPRT_TCID_UNRELIABLE_SEQUENCED:
        if (seq_no != ((rx_seq_no[SPRT_TCID_UNRELIABLE_SEQUENCED] + 1) & 0x3FFF))
        {
            fprintf(stderr,
                    "ERROR: Unreliable sequenced channel packet received with a non-consecutive sequence number - %d, expected %d\n",
                    seq_no,
                    rx_seq_no[SPRT_TCID_UNRELIABLE_SEQUENCED]);
        }
        /*endif*/
        rx_seq_no[SPRT_TCID_UNRELIABLE_SEQUENCED] = seq_no;
        len2 = sprintf((char *) buf, "Unreliable sequenced %d", rx_seq_no[SPRT_TCID_UNRELIABLE_SEQUENCED]) + 1;
        if (len2 != len)
        {
            fprintf(stderr, "ERROR: length mismatch - %d %d - '%s '%s'\n", len, len2, msg, buf);
        }
        else if (memcmp(msg, buf, len) != 0)
        {
            fprintf(stderr, "ERROR: message mismatch - '%s '%s'\n", msg, buf);
        }
        /*endif*/
        break;
    }
    /*endswitch*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void paced_operations(void)
{
    uint8_t msg[2000];
    int len;

    fprintf(stderr, "Pace at %lu\n", now_us());

    if (send_messages  &&  (rand() % 100) == 0)
    {
        /* There isn't really a sequence number for unreliable unsequenced packets, but we want the messages to vary a little */
        len = sprintf((char *) msg, "Unreliable unsequenced %d", tx_seq_no[SPRT_TCID_UNRELIABLE_UNSEQUENCED]) + 1;
        fprintf(stderr, "Sending %d, %d, %d - '%s'\n", SPRT_TCID_UNRELIABLE_UNSEQUENCED, 0, len, msg);
        printf("Sending %d, %d, %d - '%s'\n", SPRT_TCID_UNRELIABLE_UNSEQUENCED, 0, len, msg);
        if (sprt_tx(sprt_state, SPRT_TCID_UNRELIABLE_UNSEQUENCED, msg, len) == 0)
            bump_sprt_seq_no(tx_seq_no[SPRT_TCID_UNRELIABLE_UNSEQUENCED]);
        else
            fprintf(stderr, "ERROR: Unreliable unsequenced overflow\n");
        /*endif*/
    }
    /*endif*/

    if (send_messages  &&  (rand() % 100) == 0)
    {
        len = sprintf((char *) msg, "Expedited reliable sequenced %d", tx_seq_no[SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED]) + 1;
        fprintf(stderr, "Sending %d, %d, %d - '%s'\n", SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, 0, len, msg);
        printf("Sending %d, %d, %d - '%s'\n", SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, 0, len, msg);
        if (sprt_tx(sprt_state, SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, msg, len) == 0)
            bump_sprt_seq_no(tx_seq_no[SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED]);
        else
            fprintf(stderr, "ERROR: Expedited overflow\n");
        /*endif*/
    }
    /*endif*/

    if (send_messages)
    {
        len = sprintf((char *) msg, "Reliable sequenced %d", tx_seq_no[SPRT_TCID_RELIABLE_SEQUENCED]) + 1;
        fprintf(stderr, "Sending %d, %d, %d - '%s'\n", SPRT_TCID_RELIABLE_SEQUENCED, 0, len, msg);
        printf("Sending %d, %d, %d - '%s'\n", SPRT_TCID_RELIABLE_SEQUENCED, 0, len, msg);
        if (sprt_tx(sprt_state, SPRT_TCID_RELIABLE_SEQUENCED, msg, len) == 0)
            bump_sprt_seq_no(tx_seq_no[SPRT_TCID_RELIABLE_SEQUENCED]);
        else
            fprintf(stderr, "ERROR: Non-expedited overflow\n");
        /*endif*/
    }
    /*endif*/

    if (send_messages  &&  (rand() % 100) == 0)
    {
        len = sprintf((char *) msg, "Unreliable sequenced %d", tx_seq_no[SPRT_TCID_UNRELIABLE_SEQUENCED]) + 1;
        fprintf(stderr, "Sending %d, %d, %d - '%s'\n", SPRT_TCID_UNRELIABLE_SEQUENCED, 0, len, msg);
        printf("Sending %d, %d, %d - '%s'\n", SPRT_TCID_UNRELIABLE_SEQUENCED, 0, len, msg);
        if (sprt_tx(sprt_state, SPRT_TCID_UNRELIABLE_SEQUENCED, msg, len) == 0)
            bump_sprt_seq_no(tx_seq_no[SPRT_TCID_UNRELIABLE_SEQUENCED]);
        else
            fprintf(stderr, "ERROR: Unreliable sequenced overflow\n");
        /*endif*/
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
        fprintf(stderr, "Pace timer expired at %lu\n", now);
        paced_operations();
        pace_timer += 20000;
    }
    /*endif*/
    if (sprt_timer  &&  now >= sprt_timer)
    {
        fprintf(stderr, "SPRT timer expired at %lu\n", now);
        sprt_timer = 0;
        sprt_timer_expired((sprt_state_t *) user_data, now);
    }
    /*endif*/
    if (sprt_timer  &&  sprt_timer < pace_timer)
        socket_dgram_harness_timer = sprt_timer;
    else
        socket_dgram_harness_timer = pace_timer;
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static span_timestamp_t timer_handler(void *user_data, span_timestamp_t timeout)
{
    span_timestamp_t now;

    now = now_us();
    if (timeout == 0)
    {
        fprintf(stderr, "SPRT timer stopped at %lu\n", now);
        sprt_timer = 0;
        socket_dgram_harness_timer = pace_timer;
    }
    else if (timeout == ~0)
    {
        fprintf(stderr, "SPRT get the time %lu\n", now);
        /* Just return the current time */
    }
    else
    {
        fprintf(stderr, "SPRT timer set to %lu at %lu\n", timeout, now);
        if (timeout < now)
            timeout = now;
        /*endif*/
        sprt_timer = timeout;
        if (sprt_timer < pace_timer)
            socket_dgram_harness_timer = timeout;
        /*endif*/
    }
    /*endif*/
    return now;
}
/*- End of function --------------------------------------------------------*/

static void status_handler(void *user_data, int status)
{
    printf("SPRT status event %d\n", status);
}
/*- End of function --------------------------------------------------------*/

static int sprt_tests(bool calling_party)
{
    int i;
    logging_state_t *logging;

    send_messages = true; //calling_party;

    for (i = 0;  i < SPRT_CHANNELS;  i++)
    {
        tx_seq_no[i] = 0;
        rx_seq_no[i] = 0x3FFF;
    }
    /*endfor*/
    if ((dgram_state = socket_dgram_harness_init(NULL,
                                                 (calling_party)  ?  "/tmp/sprt_socket_a"  :  "/tmp/sprt_socket_b",
                                                 (calling_party)  ?  "/tmp/sprt_socket_b"  :  "/tmp/sprt_socket_a",
                                                 (calling_party)  ?  "C"  :  "A",
                                                 calling_party,
                                                 terminal_callback,
                                                 termios_callback,
                                                 hangup_callback,
                                                 terminal_free_space_callback,
                                                 rx_callback,
                                                 tx_callback,
                                                 timer_callback,
                                                 NULL)) == NULL)
    {
        fprintf(stderr, "    Cannot start the socket harness\n");
        exit(2);
    }
    /*endif*/

    if ((sprt_state = sprt_init(NULL,
                                0,
                                120,
                                120,
                                NULL /* Use default params */,
                                tx_packet_handler,
                                (void *) dgram_state,
                                rx_delivery_handler,
                                (void *) dgram_state,
                                timer_handler,
                                (void *) dgram_state,
                                status_handler,
                                (void *) dgram_state)) == NULL)
    {
        fprintf(stderr, "    Cannot start SPRT\n");
        exit(2);
    }
    /*endif*/

    for (i = SPRT_TCID_MIN;  i <= SPRT_TCID_MAX;  i++) 
        max_payloads[i] = sprt_get_far_tc_payload_bytes(sprt_state, i);
    /*endfor*/

    socket_dgram_harness_set_user_data(dgram_state, sprt_state);

    logging = sprt_get_logging_state(sprt_state);
    span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_DATE);
    span_log_set_tag(logging, (calling_party)  ?  "C"  :  "A");

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

    if (sprt_tests(calling_party))
        exit(2);
    /*endif*/
    printf("Tests passed\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
