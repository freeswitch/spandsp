/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/v150_1_sse.h - An implementation of the state signaling events
 *                        (SSE), protocol defined in V.150.1 Annex C, less
 *                        the packet exchange part
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

#if !defined(_SPANDSP_PRIVATE_V150_1_SSE_H_)
#define _SPANDSP_PRIVATE_V150_1_SSE_H_

struct v150_1_sse_state_s
{
    int reliability_method;

    int repetition_count;                                                   /* Default 3        See V.150.1 C.4.1 */
    int repetition_interval;                                                /* Defautl 20ms     See V.150.1 C.4.1 */

    int ack_n0count;                                                        /* Default 3        See V.150.1 C.4.3.1 */
    int ack_t0interval;                                                     /* Default 10ms     See V.150.1 C.4.3.1 */
    int ack_t1interval;                                                     /* Default 300ms    See V.150.1 C.4.3.1 */

    int recovery_n;                                                         /* Default 5        See V.150.1 C.5.4.1 */
    int recovery_t1;                                                        /* Default 1s       See V.150.1 C.5.4.1 */
    int recovery_t2;                                                        /* Default 1s       See V.150.1 C.5.4.1 */

    span_timestamp_t latest_timer;

    /* Explicit acknowledgement variables. */
    bool explicit_ack_enabled;

    span_timestamp_t recovery_timer_t1;                                     /* See V.150.1 C.5.4.1 */
    span_timestamp_t recovery_timer_t2;                                     /* See V.150.1 C.5.4.1 */
    int recovery_counter_n;                                                 /* See V.150.1 C.5.4.1 */

    /* Timer to control repetition transmission */
    span_timestamp_t repetition_timer;                                      /* See V.150.1 C.4.1 */
    /* Counter used to control repetition transmission */
    int repetition_counter;                                                 /* See V.150.1 C.4.1 */

    /* Timer to control sending mode change messages to the remote node */
    span_timestamp_t ack_timer_t0;                                          /* See V.150.1 C.4.3.1 */
    /* Timer to recover from lost acknowledgements sent by the remote node */
    span_timestamp_t ack_timer_t1;                                          /* See V.150.1 C.4.3.1 */
    /* Counter used to control sending mode change messages to the remote node */
    int ack_counter_n0;                                                     /* See V.150.1 C.4.3.1 */
    bool force_response;

    bool immediate_timer;

    uint8_t last_tx_pkt[256];
    int last_tx_len;

    /* The last timestamp received from the remote gateway or endpoint */
    uint32_t previous_rx_timestamp;

    v150_1_sse_tx_packet_handler_t tx_packet_handler;
    void *tx_packet_user_data;
};

#endif
/*- End of file ------------------------------------------------------------*/
