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
#include <memory.h>
#include <ctype.h>

#include "spandsp.h"

v150_1_state_t *v150_1[2];

static int v150_1_status_report_handler(void *user_data, v150_1_status_t *report)
{
    printf("V.150.1 status report received\n");
    switch (report->reason)
    {
    case V150_1_STATUS_REASON_STATE_CHANGED:
        printf("Connection state changed to %s\n", v150_1_state_to_str(report->state_change.state));
        if (report->state_change.state == V150_1_STATE_IDLE)
            printf("    Cleardown reason %s\n", v150_1_cleardown_reason_to_str(report->state_change.cleardown_reason));
        /*endif*/
        break;
    case V150_1_STATUS_REASON_DATA_FORMAT_CHANGED:
        printf("Data format changed\n");
        printf("    Format is %d data bits, %d stop bits, %s parity\n",
               report->data_format_change.bits,
               report->data_format_change.stop_bits,
               v150_1_parity_to_str(report->data_format_change.parity_code));
        break;
    case V150_1_STATUS_REASON_BREAK_RECEIVED:
        printf("Break received\n");
        printf("    Break source %s\n", v150_1_break_source_to_str(report->break_received.source));
        printf("    Break type %s\n", v150_1_break_type_to_str(report->break_received.type));
        printf("    Break duration %d ms\n", report->break_received.duration);
        break;
    case V150_1_STATUS_REASON_RATE_RETRAIN_RECEIVED:
        printf("Retrain received\n");
        break;
    case V150_1_STATUS_REASON_RATE_RENEGOTIATION_RECEIVED:
        printf("Rate renegotiation received\n");
        break;
    case V150_1_STATUS_REASON_BUSY_CHANGED:
        printf("Busy status change received\n");
        printf("Near side now %sbusy\n", (report->busy_change.local_busy)  ?  ""  :  "not ");
        printf("Far side now %sbusy\n", (report->busy_change.far_busy)  ?  ""  :  "not ");
        break;
    default:
        printf("Unknown status report reason %d received\n", report->reason);
        break;
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_octet_stream_handler(void *user_data, const uint8_t msg[], int len, int fill)
{
    int i;

    if (fill > 0)
        printf("%d missing characters\n", fill);
    /*endif*/
    printf(">>>");
    for (i = 0;  i < len;  i++)
    {
        printf(" %02x", msg[i]);
    }
    /*endfor*/
    printf("<<<\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/

/* Get a packet from one side, and pass it to the other */
static int v150_1_tx_packet_handler(void *user_data, int chan, const uint8_t msg[], int len)
{
    int i;
    int which;

    which = (intptr_t) user_data;
    printf("Tx (%d) ", which);
    for (i = 0;  i < len;  i++)
        printf("%02x ", msg[i]);
    /*endfor*/
    printf("\n");

    v150_1_process_rx_msg(v150_1[which ^ 1], chan, 0, msg, len);

    return 0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    logging_state_t *logging;
    uint8_t buf[256];
    int msg_id_priorities[10];
    int i;
    int max;

    v150_1[0] = v150_1_init(NULL,
                            v150_1_tx_packet_handler,
                            (void *) (intptr_t) 0,
                            v150_1_octet_stream_handler,
                            (void *) (intptr_t) 0,
                            v150_1_status_report_handler,
                            (void *) (intptr_t) 0);

    v150_1[1] = v150_1_init(NULL,
                            v150_1_tx_packet_handler,
                            (void *) (intptr_t) 1,
                            v150_1_octet_stream_handler,
                            (void *) (intptr_t) 1,
                            v150_1_status_report_handler,
                            (void *) (intptr_t) 1);

    logging = v150_1_get_logging_state(v150_1[0]);
    span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_DATE);
    logging = v150_1_get_logging_state(v150_1[1]);
    span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_DATE);

    v150_1_set_info_stream_tx_mode(v150_1[0], SPRT_TCID_RELIABLE_SEQUENCED, V150_1_MSGID_I_OCTET_CS);
    v150_1_set_info_stream_tx_mode(v150_1[1], SPRT_TCID_RELIABLE_SEQUENCED, V150_1_MSGID_I_OCTET_CS);

    msg_id_priorities[0] = V150_1_MSGID_I_OCTET;
    msg_id_priorities[1] = V150_1_MSGID_I_OCTET_CS;
    msg_id_priorities[2] = -1; 
    v150_1_set_info_stream_msg_priorities(v150_1[0], msg_id_priorities);
    v150_1_set_info_stream_msg_priorities(v150_1[1], msg_id_priorities);

    v150_1_tx_null(v150_1[0]);
    v150_1_tx_null(v150_1[1]);

    v150_1_tx_init(v150_1[0]);
    v150_1_tx_init(v150_1[1]);

    v150_1_set_modulation(v150_1[0], V150_1_SELMOD_V34);
    v150_1_set_compression_direction(v150_1[0], V150_1_COMPRESS_NEITHER_WAY);
    v150_1_set_compression(v150_1[0], V150_1_COMPRESSION_NONE);
    v150_1_set_compression_parameters(v150_1[0],
                                      512,
                                      512,
                                      6,
                                      6,
                                      0,
                                      0);
    v150_1_set_error_correction(v150_1[0], V150_1_ERROR_CORRECTION_NONE);
    v150_1_set_tx_symbol_rate(v150_1[0], true, V150_1_SYMBOL_RATE_3429);
    v150_1_set_rx_symbol_rate(v150_1[0], true, V150_1_SYMBOL_RATE_3429);
    v150_1_set_tx_data_signalling_rate(v150_1[0], 33600);
    v150_1_set_rx_data_signalling_rate(v150_1[0], 33600);

    v150_1_set_modulation(v150_1[1], V150_1_SELMOD_V34);
    v150_1_set_compression_direction(v150_1[1], V150_1_COMPRESS_NEITHER_WAY);
    v150_1_set_compression(v150_1[1], V150_1_COMPRESSION_NONE);
    v150_1_set_compression_parameters(v150_1[1],
                                      512,
                                      512,
                                      6,
                                      6,
                                      0,
                                      0);
    v150_1_set_error_correction(v150_1[1], V150_1_ERROR_CORRECTION_NONE);
    v150_1_set_tx_symbol_rate(v150_1[1], true, V150_1_SYMBOL_RATE_3429);
    v150_1_set_rx_symbol_rate(v150_1[1], true, V150_1_SYMBOL_RATE_3429);
    v150_1_set_tx_data_signalling_rate(v150_1[1], 33600);
    v150_1_set_rx_data_signalling_rate(v150_1[1], 33600);

    v150_1_tx_jm_info(v150_1[0]);
    v150_1_tx_jm_info(v150_1[1]);
    v150_1_tx_mr_event(v150_1[0], V150_1_MR_EVENT_ID_PHYSUP);
    v150_1_tx_mr_event(v150_1[1], V150_1_MR_EVENT_ID_PHYSUP);
    v150_1_tx_connect(v150_1[0]);
    v150_1_tx_connect(v150_1[1]);

    v150_1_tx_xid_xchg(v150_1[0]);
    v150_1_tx_start_jm(v150_1[0]);
    v150_1_tx_prof_xchg(v150_1[0]);

    if (v150_1_tx_info_stream(v150_1[0], (const uint8_t *) "Test side 0", 11) < 0)
        printf("Failed to send good message\n");
    /*endif*/
    if (v150_1_tx_info_stream(v150_1[1], (const uint8_t *) "Test side 1", 11) < 0)
        printf("Failed to send good message\n");
    /*endif*/

    max = v150_1_get_local_tc_payload_bytes(v150_1[0], SPRT_TCID_RELIABLE_SEQUENCED);
    printf("Max payload bytes is %d\n", max);

    for (i = 0;  i < 256;  i++)
        buf[i] = i;
    /*endfor*/
    if (v150_1_tx_info_stream(v150_1[0], buf, 129) < 0)
        printf("Failed to send good message\n");
    /*endif*/
    for (i = 0;  i < 256;  i++)
        buf[i] = 255 - i;
    /*endfor*/
    if (v150_1_tx_info_stream(v150_1[1], buf, 129) < 0)
        printf("Failed to send good message\n");
    /*endif*/

    if (v150_1_set_local_tc_payload_bytes(v150_1[0], SPRT_TCID_RELIABLE_SEQUENCED, 200) < 0)
        printf("Failed to set new max payload bytes\n");
    /*endif*/
    if (v150_1_set_local_tc_payload_bytes(v150_1[1], SPRT_TCID_RELIABLE_SEQUENCED, 200) < 0)
        printf("Failed to set new max payload bytes\n");
    /*endif*/

    for (i = 0;  i < 256;  i++)
        buf[i] = i;
    /*endfor*/
    if (v150_1_tx_info_stream(v150_1[0], buf, 197) < 0)
        printf("Failed to send good message\n");
    /*endif*/
    for (i = 0;  i < 256;  i++)
        buf[i] = 255 - i;
    /*endfor*/
    if (v150_1_tx_info_stream(v150_1[1], buf, 198) >= 0)
        printf("Able to send bad length message\n");
    /*endif*/

    if (v150_1_set_local_tc_payload_bytes(v150_1[0], SPRT_TCID_RELIABLE_SEQUENCED, 256) < 0)
        printf("Failed to set new max payload bytes\n");
    /*endif*/
    if (v150_1_set_local_tc_payload_bytes(v150_1[1], SPRT_TCID_RELIABLE_SEQUENCED, 257) >= 0)
        printf("Able to set bad new max payload bytes\n");
    /*endif*/

    if (v150_1_set_local_tc_payload_bytes(v150_1[0], SPRT_TCID_RELIABLE_SEQUENCED, 132) < 0)
        printf("Failed to set new max payload bytes\n");
    /*endif*/
    if (v150_1_set_local_tc_payload_bytes(v150_1[1], SPRT_TCID_RELIABLE_SEQUENCED, 132) >= 0)
        printf("Able to set bad new max payload bytes\n");
    /*endif*/

    for (i = 0;  i < 256;  i++)
        buf[i] = i;
    /*endfor*/
    if (v150_1_tx_info_stream(v150_1[0], buf, 129) < 0)
        printf("Failed to send byte stream\n");
    /*endif*/
    for (i = 0;  i < 256;  i++)
        buf[i] = 255 - i;
    /*endfor*/
    if (v150_1_tx_info_stream(v150_1[1], buf, 129) < 0)
        printf("Failed to send byte stream\n");
    /*endif*/

    v150_1_tx_break(v150_1[0], V150_1_BREAK_SOURCE_V42_LAPM, V150_1_BREAK_TYPE_DESTRUCTIVE_EXPEDITED, 1230);
    v150_1_tx_break_ack(v150_1[1]);

    v150_1_tx_cleardown(v150_1[0], V150_1_CLEARDOWN_REASON_LINK_LAYER_DISCONNECT);

    v150_1_tx_cleardown(v150_1[1], V150_1_CLEARDOWN_REASON_LINK_LAYER_DISCONNECT);

    buf[0] = 0x01;
    buf[1] = 0x40;
    buf[2] = 0x80;
    v150_1_process_rx_msg(v150_1[0], SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, 0, buf, 3);

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
    v150_1_process_rx_msg(v150_1[0], SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, 1, buf, 10);

    buf[0] = 0x05;
    buf[1] = 0x18;
    buf[2] = 0x00;
    buf[3] = 0x25;
    buf[4] = 0x80;
    buf[5] = 0x25;
    buf[6] = 0x80;
    buf[7] = 0x02;
    buf[8] = 0x00;
    v150_1_process_rx_msg(v150_1[0], SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, 2, buf, 9);

    buf[0] = 0x09;
    buf[1] = 0x05;
    buf[2] = 0x01;
    buf[3] = 0x02;
    v150_1_process_rx_msg(v150_1[0], SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, 2, buf, 4);

    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
