/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v150_1_tests.c - Test V.150_1 processing.
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

/*! \page v150_1_tests_page V.150_1 tests
\section v150_1_tests_page_sec_1 What does it do?
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

#include "spandsp.h"
#include "spandsp-sim.h"

#include "pseudo_terminals.h"
#include "socket_dgram_harness.h"

#define SSE_PACKET_TYPE     118
#define SPRT_PACKET_TYPE    120

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

int rtp_time_stamp = 0;

rtp_t rtp;
v150_1_state_t *v150_1;
socket_dgram_harness_state_t *dgram_state;

int max_payloads[SPRT_CHANNELS];

int pace_no = 0;

bool send_messages = false;
bool calling_party = false;

span_timestamp_t pace_timer = 0;
span_timestamp_t v150_1_timer = 0;

/* Crude RTP routines */

static int rtp_init(rtp_t *rtp, uint32_t time_stamp, uint32_t ssrc)
{
    rtp->v = 2;
    rtp->p = 0;
    rtp->x = 0;
    rtp->cc = 0;
    rtp->m = 0;
    rtp->seq_no = rand();
    rtp->time_stamp = time_stamp;
    rtp->ssrc = ssrc;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int rtp_fill(rtp_t *rtp, uint8_t *buf, int max_len, int pt, const uint8_t *signal, int signal_len, uint32_t advance)
{
    rtp->time_stamp += advance;
    buf[0] = ((rtp->v & 0x03) << 6) | ((rtp->p & 0x01) << 5) | ((rtp->x & 0x01) << 4) | (rtp->cc & 0x0F);
    buf[1] = ((rtp->m & 0x01) << 7) | pt;
    put_net_unaligned_uint16(&buf[2], rtp->seq_no);
    put_net_unaligned_uint32(&buf[4], rtp->time_stamp);
    put_net_unaligned_uint32(&buf[8], rtp->ssrc);
    memcpy(&buf[12], signal, signal_len);
    rtp->seq_no++;
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

    fprintf(stderr, "terminal callback %d\n", len);
    for (i = 0;  i < len;  i++)
    {
        fprintf(stderr, "0x%x ", msg[i]);
    }
    fprintf(stderr, "\n");
    /* TODO: connect AT input to SPRT */
}
/*- End of function --------------------------------------------------------*/

static int termios_callback(void *user_data, struct termios *termios)
{
    //data_modems_state_t *s;

    //s = (data_modems_state_t *) user_data;
    fprintf(stderr, "termios callback\n");
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

static void dgram_rx_callback(void *user_data, const uint8_t buf[], int len)
{
    int pt;
    rtp_t rtp;
    int signal_len;
    uint8_t signal[160];
    v150_1_state_t *s;

    s = (v150_1_state_t *) user_data;
    if (len > 1)
    {
        pt = buf[1] & 0x7F;
        fprintf(stderr, "Packet type %d\n", pt);
        if ((buf[0] & 0xC0) == 0x80)
        {
            /* This looks like RTP */
            fprintf(stderr, "Looks RTPish\n");
            signal_len = rtp_extract(&rtp, signal, 160, buf, len);
            if (rtp.pt == SSE_PACKET_TYPE)
            {
                v150_1_rx_sse_packet(s, rtp.seq_no, rtp.time_stamp, signal, signal_len);
            }
            /*endif*/
            rtp_time_stamp = rtp.time_stamp + 160;
        }
        else
        {
            /* Could be SPRT */
            fprintf(stderr, "Looks SPRTish\n");
            if (pt == SPRT_PACKET_TYPE)
            {
                sprt_rx_packet(&s->sprt, buf, len);
            }
            /*endif*/
        }
        /*endif*/
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static int tx_callback(void *user_data, uint8_t buff[], int len)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_status_report_handler(void *user_data, v150_1_status_t *report)
{
    fprintf(stderr, "V.150.1 status report received\n");
    switch (report->reason)
    {
    case V150_1_STATUS_REASON_MEDIA_STATE_CHANGED:
        fprintf(stderr,
                "Media state changed to %s %s\n",
                v150_1_media_state_to_str(report->types.media_state_change.local_state),
                v150_1_media_state_to_str(report->types.media_state_change.remote_state));
        break;
    case V150_1_STATUS_REASON_CONNECTION_STATE_CHANGED:
        fprintf(stderr, "Connection state changed to %s\n", v150_1_state_to_str(report->types.connection_state_change.state));
        switch (report->types.connection_state_change.state)
        {
        case V150_1_STATE_IDLE:
            fprintf(stderr, "    Cleardown reason %s\n", v150_1_cleardown_reason_to_str(report->types.connection_state_change.cleardown_reason));
            break;
        case V150_1_STATE_INITED:
            if (calling_party)
                v150_1_tx_init(v150_1);
            else
                v150_1_tx_jm_info(v150_1);
            /*endif*/
            break;
        }
        /*endswitch*/
        break;
    case V150_1_STATUS_REASON_CONNECTION_STATE_PHYSUP:
        fprintf(stderr, "Physup received\n");
        v150_1_tx_mr_event(v150_1, V150_1_MR_EVENT_ID_PHYSUP);
        break;
    case V150_1_STATUS_REASON_CONNECTION_STATE_CONNECTED:
        fprintf(stderr, "Connected received\n");
        v150_1_tx_connect(v150_1);
        break;
    case V150_1_STATUS_REASON_DATA_FORMAT_CHANGED:
        fprintf(stderr, "Data format changed\n");
        fprintf(stderr, "    Format is %d data bits, %d stop bits, %s parity\n",
               report->types.data_format_change.bits,
               report->types.data_format_change.stop_bits,
               v150_1_parity_to_str(report->types.data_format_change.parity_code));
        break;
    case V150_1_STATUS_REASON_BREAK_RECEIVED:
        fprintf(stderr, "Break received\n");
        fprintf(stderr, "    Break source %s\n", v150_1_break_source_to_str(report->types.break_received.source));
        fprintf(stderr, "    Break type %s\n", v150_1_break_type_to_str(report->types.break_received.type));
        fprintf(stderr, "    Break duration %d ms\n", report->types.break_received.duration);
        break;
    case V150_1_STATUS_REASON_RATE_RETRAIN_RECEIVED:
        fprintf(stderr, "Retrain received\n");
        break;
    case V150_1_STATUS_REASON_RATE_RENEGOTIATION_RECEIVED:
        fprintf(stderr, "Rate renegotiation received\n");
        break;
    case V150_1_STATUS_REASON_BUSY_CHANGED:
        fprintf(stderr, "Busy status change received\n");
        fprintf(stderr, "Near side now %sbusy\n", (report->types.busy_change.local_busy)  ?  ""  :  "not ");
        fprintf(stderr, "Far side now %sbusy\n", (report->types.busy_change.far_busy)  ?  ""  :  "not ");
        break;
    default:
        fprintf(stderr, "Unknown status report reason %d received\n", report->reason);
        break;
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_data_stream_handler(void *user_data, const uint8_t msg[], int len, int fill)
{
    int i;

    if (fill > 0)
        fprintf(stderr, "%d missing characters\n", fill);
    /*endif*/
    fprintf(stderr, ">>>");
    for (i = 0;  i < len;  i++)
    {
        fprintf(stderr, " %02x", msg[i]);
    }
    /*endfor*/
    fprintf(stderr, "<<<\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/

/* Get a packet from one side, and pass it to the other */
static int sprt_tx_packet_handler(void *user_data, const uint8_t pkt[], int len)
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

static int sse_tx_packet_handler(void *user_data, bool repeat, const uint8_t *pkt, int len)
{
    int i;
    socket_dgram_harness_state_t *s;
    int sent_len;
    uint8_t buf[256];
    int len2;
    
    fprintf(stderr, "Tx message");
    for (i = 0;  i < len;  i++)
        fprintf(stderr, " %02x", pkt[i]);
    /*endfor*/
    fprintf(stderr, "\n");

    len2 = rtp_fill(&rtp, buf, 256, SSE_PACKET_TYPE, pkt, len, (repeat)  ?  0  :  160);

    s = (socket_dgram_harness_state_t *) user_data;
    /* We need a packet loss mechanism here */
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

    return 0;
}
/*- End of function --------------------------------------------------------*/

static void paced_operations(void)
{
    int i;
    uint8_t buf[256];

    fprintf(stderr, "Pace at %lu\n", now_us());

    pace_no++;

    if (pace_no == 50)
    {
        if (!calling_party)
        {
            if (v150_1_tx_sse_packet(v150_1, V150_1_MEDIA_STATE_MODEM_RELAY, V150_1_SSE_MOIP_RIC_V32BIS_AA, 0) != 0)
                fprintf(stderr, "ERROR: Failed to send message\n");
            /*endif*/
        }
        /*endif*/
    }
    /*endif*/
#if 0
    if (pace_no == 50)
    {
        if (!calling_party)
            v150_1_tx_init(v150_1);
        /*endif*/
    }
    /*endif*/
    if (pace_no == 500)
    {
        if (!calling_party)
        {
            v150_1_tx_mr_event(v150_1, V150_1_MR_EVENT_ID_PHYSUP);
            v150_1_tx_connect(v150_1);
        }
        /*endif*/
    }
    /*endif*/
#if 0
    else
    {
        for (i = 0;  i < 256;  i++)
            buf[i] = i;
        /*endfor*/
        if (v150_1_tx_info_stream(v150_1, buf, 42) < 0)
            fprintf(stderr, "Failed to send good message\n");
        /*endif*/
    }
#endif
#endif
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static void timer_callback(void *user_data)
{
    span_timestamp_t now;
    span_timestamp_t cand;

    now = now_us();
    if (pace_timer  &&  now >= pace_timer)
    {
        fprintf(stderr, "Pace timer expired at %lu\n", now);
        paced_operations();
        pace_timer += 20000;
    }
    /*endif*/
    if (v150_1_timer  &&  now >= v150_1_timer)
    {
        fprintf(stderr, "V150.1 timer expired at %lu\n", now);
        v150_1_timer = 0;
        v150_1_timer_expired(v150_1, now);
    }
    /*endif*/
    cand = ~0;
    if (v150_1_timer  &&  v150_1_timer < cand)
        cand = v150_1_timer;
    /*endif*/
    if (pace_timer  &&  pace_timer < cand)
        cand = pace_timer;
    /*endif*/
    socket_dgram_harness_timer = cand;
}
/*- End of function --------------------------------------------------------*/

static span_timestamp_t v150_1_timer_handler(void *user_data, span_timestamp_t timeout)
{
    span_timestamp_t now;

    now = now_us();
    if (timeout == 0)
    {
        fprintf(stderr, "V.150.1 timer stopped at %lu\n", now);
        v150_1_timer = 0;
        socket_dgram_harness_timer = pace_timer;
    }
    else if (timeout == ~0)
    {
        fprintf(stderr, "V.150.1 get the time %lu\n", now);
        /* Just return the current time */
    }
    else
    {
        fprintf(stderr, "V.150.1 timer set to %lu at %lu\n", timeout, now);
        if (timeout < now)
            timeout = now;
        /*endif*/
        v150_1_timer = timeout;
        if (pace_timer == 0  ||  pace_timer > v150_1_timer)
            socket_dgram_harness_timer = v150_1_timer;
        else
            socket_dgram_harness_timer = pace_timer;
        /*endif*/
    }
    /*endif*/
    return now;
}
/*- End of function --------------------------------------------------------*/

static int message_decode_tests(void)
{
    uint8_t buf[256];
    int len;

    /* The following INIT message should say:
        Preferred non-error controlled Rx channel: USC
        Preferred error controlled Rx channel: USC
        XID profile exchange  not supported
        Asymmetric data types not supported
        I_RAW-CHAR            supported
        I_RAW-BIT             not supported
        I_FRAME               not supported
        I_OCTET (no DLCI)     supported
        I_CHAR-STAT           not supported
        I_CHAR-DYN            not supported
        I_OCTET-CS            supported
        I_CHAR-STAT-CS        not supported
        I_CHAR-DYN-CS         not supported
     */
    printf("INIT test\n");
    buf[0] = 0x01;
    buf[1] = 0x40;
    buf[2] = 0x80;
    v150_1_test_rx_sprt_msg(v150_1, SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, 0, buf, 3);

    /* The following MR-EVENT message 
        Selected modulation V.32
        Tx data signalling rate 9600
        Rx data signalling rate 9600
     */
    printf("MR-EVENT test\n");
    buf[0] = 0x08;
    buf[1] = 0x03;
    buf[2] = 0x00;
    buf[3] = 0x18;
    buf[4] = 0x25;
    buf[5] = 0x80;
    buf[6] = 0x25;
    buf[7] = 0x80;
    buf[8] = 0x00;
    buf[9] = 0x00;
    v150_1_test_rx_sprt_msg(v150_1, SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, 1, buf, 10);

    /* The following CONNECT message should say:
        Modulation V.32
        Compression direction Neither way
        Compression None
        Error correction None
        Tx data rate 9600
        Rx data rate 9600
        I_RAW-CHAR            available
        I_RAW-BIT             not available
        I_FRAME               not available
        I_OCTET               not available
        I_CHAR-STAT           not available
        I_CHAR-DYN            not available
        I_OCTET-CS            available
        I_CHAR-STAT-CS        not available
        I_CHAR-DYN-CS         not available
     */
    printf("CONNECT test\n");
    buf[0] = 0x05;
    buf[1] = 0x18;
    buf[2] = 0x00;
    buf[3] = 0x25;
    buf[4] = 0x80;
    buf[5] = 0x25;
    buf[6] = 0x80;
    buf[7] = 0x02;
    buf[8] = 0x00;
    v150_1_test_rx_sprt_msg(v150_1, SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, 2, buf, 9);

    printf("I_OCTET-CS sending \"TEST\" test\n");
    buf[0] = 0x16;
    buf[1] = 0x00;
    buf[2] = 0x00;
    buf[3] = 0x54;
    buf[4] = 0x45;
    buf[5] = 0x53;
    buf[6] = 0x54;
    v150_1_test_rx_sprt_msg(v150_1, SPRT_TCID_RELIABLE_SEQUENCED, 0, buf, 7);

    printf("I_OCTET-CS sending \"TEST\" test\n");
    buf[0] = 0x16;
    buf[1] = 0x00;
    buf[2] = 0x00;
    buf[3] = 0x54;
    buf[4] = 0x45;
    buf[5] = 0x53;
    buf[6] = 0x54;
    v150_1_test_rx_sprt_msg(v150_1, SPRT_TCID_UNRELIABLE_SEQUENCED, 0, buf, 7);

    printf("CLEARDOWN test\n");
    buf[0] = 0x09;
    buf[1] = 0x05;
    buf[2] = 0x01;
    buf[3] = 0x02;
    v150_1_test_rx_sprt_msg(v150_1, SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, 3, buf, 4);

    return 0;
}
/*- End of function --------------------------------------------------------*/

static int message_encode_tests(void)
{
    uint8_t buf[256];
    int i;

    v150_1_tx_null(v150_1);

    /* The following INIT message should say:
        Preferred non-error controlled Rx channel: USC
        Preferred error controlled Rx channel: USC
        XID profile exchange  not supported
        Asymmetric data types not supported
        I_RAW-CHAR            supported
        I_RAW-BIT             not supported
        I_FRAME               not supported
        I_OCTET (no DLCI)     supported
        I_CHAR-STAT           not supported
        I_CHAR-DYN            not supported
        I_OCTET-CS            supported
        I_CHAR-STAT-CS        not supported
        I_CHAR-DYN-CS         not supported
     */
    printf("INIT test\n");
    buf[0] = 0x01;
    buf[1] = 0x40;
    buf[2] = 0x80;
    v150_1_test_rx_sprt_msg(v150_1, SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, 0, buf, 3);

    v150_1_tx_init(v150_1);

    v150_1_tx_jm_info(v150_1);

    /* The following MR-EVENT message 
        Selected modulation V.32
        Tx data signalling rate 9600
        Rx data signalling rate 9600
     */
    printf("MR-EVENT test\n");
    buf[0] = 0x08;
    buf[1] = 0x03;
    buf[2] = 0x00;
    buf[3] = 0x18;
    buf[4] = 0x25;
    buf[5] = 0x80;
    buf[6] = 0x25;
    buf[7] = 0x80;
    buf[8] = 0x00;
    buf[9] = 0x00;
    v150_1_test_rx_sprt_msg(v150_1, SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, 1, buf, 10);

    v150_1_tx_mr_event(v150_1, V150_1_MR_EVENT_ID_PHYSUP);

    /* The following CONNECT message should say:
        Modulation V.32
        Compression direction Neither way
        Compression None
        Error correction None
        Tx data rate 9600
        Rx data rate 9600
        I_RAW-CHAR            available
        I_RAW-BIT             not available
        I_FRAME               not available
        I_OCTET               not available
        I_CHAR-STAT           not available
        I_CHAR-DYN            not available
        I_OCTET-CS            available
        I_CHAR-STAT-CS        not available
        I_CHAR-DYN-CS         not available
     */
    printf("CONNECT test\n");
    buf[0] = 0x05;
    buf[1] = 0x18;
    buf[2] = 0x00;
    buf[3] = 0x25;
    buf[4] = 0x80;
    buf[5] = 0x25;
    buf[6] = 0x80;
    buf[7] = 0x02;
    buf[8] = 0x00;
    v150_1_test_rx_sprt_msg(v150_1, SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, 2, buf, 9);

    v150_1_tx_connect(v150_1);

    //v150_1_tx_xid_xchg(v150_1);
    //v150_1_tx_start_jm(v150_1);
    //v150_1_tx_prof_xchg(v150_1);

    if (v150_1_tx_info_stream(v150_1, (const uint8_t *) "Test side 0", 11) < 0)
        fprintf(stderr, "Failed to send good message\n");
    /*endif*/

    for (i = 0;  i < 256;  i++)
        buf[i] = i;
    /*endfor*/
    if (v150_1_tx_info_stream(v150_1, buf, 129) < 0)
        fprintf(stderr, "Failed to send good message\n");
    /*endif*/

    for (i = 0;  i < 256;  i++)
        buf[i] = 255 - i;
    /*endfor*/
    if (v150_1_tx_info_stream(v150_1, buf, 129) < 0)
        fprintf(stderr, "Failed to send good message\n");
    /*endif*/

    for (i = 0;  i < 256;  i++)
        buf[i] = 255 - i;
    /*endfor*/
    if (v150_1_tx_info_stream(v150_1, buf, 198) >= 0)
        fprintf(stderr, "Able to send bad length message\n");
    /*endif*/

    for (i = 0;  i < 256;  i++)
        buf[i] = i;
    /*endfor*/
    if (v150_1_tx_info_stream(v150_1, buf, 129) < 0)
        fprintf(stderr, "Failed to send byte stream\n");
    /*endif*/

    v150_1_tx_break(v150_1, V150_1_BREAK_SOURCE_V42_LAPM, V150_1_BREAK_TYPE_DESTRUCTIVE_EXPEDITED, 1230);
    v150_1_tx_break_ack(v150_1);
    v150_1_tx_cleardown(v150_1, V150_1_CLEARDOWN_REASON_LINK_LAYER_DISCONNECT);

    v150_1_tx_cleardown(v150_1, V150_1_CLEARDOWN_REASON_LINK_LAYER_DISCONNECT);

    return 0;
}
/*- End of function --------------------------------------------------------*/

static int state_machine_tests(void)
{
    //v150_1_state_machine(v150_1, V150_1_SIGNAL_ANS, NULL, 0);
    //v150_1_state_machine(v150_1, V150_1_SIGNAL_ANS, NULL, 0);
    v150_1_state_machine(v150_1, V150_1_SIGNAL_CM, NULL, 0);
    v150_1_state_machine(v150_1, V150_1_SIGNAL_MODEM_RELAY, NULL, 0);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int dynamic_tests(void)
{
    socket_dgram_harness_timer = 
    pace_timer = now_us() + 20000;
    socket_dgram_harness_run(dgram_state);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_tests(void)
{
    logging_state_t *logging;
    int msg_id_priorities[10];
    int i;
    int max;

    if ((dgram_state = socket_dgram_harness_init(NULL,
                                                 (calling_party)  ?  "/tmp/v150_1_socket_a"  :  "/tmp/v150_1_socket_b",
                                                 (calling_party)  ?  "/tmp/v150_1_socket_b"  :  "/tmp/v150_1_socket_a",
                                                 (calling_party)  ?  "C"  :  "A",
                                                 calling_party,
                                                 terminal_callback,
                                                 termios_callback,
                                                 hangup_callback,
                                                 terminal_free_space_callback,
                                                 dgram_rx_callback,
                                                 tx_callback,
                                                 timer_callback,
                                                 NULL)) == NULL)
    {
        fprintf(stderr, "    Cannot start the socket harness\n");
        exit(2);
    }
    /*endif*/

    if ((v150_1 = v150_1_init(NULL,
                              sprt_tx_packet_handler,
                              (void *) dgram_state,
                              120,
                              120,
                              sse_tx_packet_handler,
                              (void *) dgram_state,
                              v150_1_timer_handler,
                              NULL,
                              v150_1_data_stream_handler,
                              NULL,
                              v150_1_status_report_handler,
                              NULL,
                              NULL,
                              NULL)) == NULL)
    {
        fprintf(stderr, "    Cannot start V.150.1\n");
        exit(2);
    }
    /*endif*/

    logging = sprt_get_logging_state(&v150_1->sprt);
    span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_DATE);
    span_log_set_tag(logging, (calling_party)  ?  "C"  :  "A");

    logging = v150_1_get_logging_state(v150_1);
    span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_DATE);
    span_log_set_tag(logging, (calling_party)  ?  "C"  :  "A");

    v150_1_set_info_stream_tx_mode(v150_1, SPRT_TCID_RELIABLE_SEQUENCED, V150_1_MSGID_I_OCTET_CS);

    msg_id_priorities[0] = V150_1_MSGID_I_OCTET_CS;
    msg_id_priorities[1] = V150_1_MSGID_I_OCTET;
    msg_id_priorities[2] = -1; 
    v150_1_set_info_stream_msg_priorities(v150_1, msg_id_priorities);

    v150_1_set_modulation(v150_1, V150_1_SELMOD_V34);
    v150_1_set_compression_direction(v150_1, V150_1_COMPRESS_NEITHER_WAY);
    v150_1_set_compression(v150_1, V150_1_COMPRESSION_NONE);
    v150_1_set_compression_parameters(v150_1,
                                      512,
                                      512,
                                      6,
                                      6,
                                      0,
                                      0);
    v150_1_set_error_correction(v150_1, V150_1_ERROR_CORRECTION_NONE);
    v150_1_set_tx_symbol_rate(v150_1, true, V150_1_SYMBOL_RATE_3429);
    v150_1_set_rx_symbol_rate(v150_1, true, V150_1_SYMBOL_RATE_3429);
    v150_1_set_tx_data_signalling_rate(v150_1, 33600);
    v150_1_set_rx_data_signalling_rate(v150_1, 33600);

    v150_1_set_sse_reliability_method(v150_1,
                                      V150_1_SSE_RELIABILITY_BY_REPETITION,
                                      3,
                                      100000,
                                      0);

    v150_1_set_near_cdscselect(v150_1, V150_1_CDSCSELECT_AUDIO_RFC4733);
    v150_1_set_far_cdscselect(v150_1, V150_1_CDSCSELECT_MIXED);

    for (i = SPRT_TCID_MIN;  i <= SPRT_TCID_MAX;  i++)
    {
        max_payloads[i] = sprt_get_far_tc_payload_bytes(&v150_1->sprt, i);
        fprintf(stderr, "Max payload %d is %d\n", i, max_payloads[i]);
    }
    /*endfor*/

    socket_dgram_harness_set_user_data(dgram_state, v150_1);

    if (v150_1_set_local_tc_payload_bytes(v150_1, SPRT_TCID_RELIABLE_SEQUENCED, 256) < 0)
        fprintf(stderr, "Failed to set new max payload bytes\n");
    /*endif*/
    if (v150_1_set_local_tc_payload_bytes(v150_1, SPRT_TCID_RELIABLE_SEQUENCED, 257) >= 0)
        fprintf(stderr, "Able to set bad new max payload bytes\n");
    /*endif*/
    if (v150_1_set_local_tc_payload_bytes(v150_1, SPRT_TCID_RELIABLE_SEQUENCED, 132) < 0)
        fprintf(stderr, "Failed to set new max payload bytes\n");
    /*endif*/
    if (v150_1_set_local_tc_payload_bytes(v150_1, SPRT_TCID_RELIABLE_SEQUENCED, 131) >= 0)
        fprintf(stderr, "Able to set bad new max payload bytes\n");
    /*endif*/

    max = v150_1_get_local_tc_payload_bytes(v150_1, SPRT_TCID_RELIABLE_SEQUENCED);
    fprintf(stderr, "Max payload bytes is %d\n", max);

    rtp_init(&rtp, 0, 0x12345678);

    //message_decode_tests();
    message_encode_tests();
    //state_machine_tests();
    //dynamic_tests();

    return 0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int opt;

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

    if (v150_1_tests())
        exit(2);
    /*endif*/
    fprintf(stderr, "Tests passed\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
