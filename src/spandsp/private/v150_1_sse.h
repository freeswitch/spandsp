/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/v150_1_sse.h - An implementation of the SSE protocol defined in V.150.1
 *                        Annex C, less the packet exchange part
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
    /* Explicit acknowledgement parameters. */
    bool explicit_acknowledgements;
    int repetitions;
    int repetition_interval;
    int t0interval;                                                         /* Default 10ms     See V.150.1 C.4.3.1 */
    int t1interval;                                                         /* Default 300ms    See V.150.1 C.4.3.1 */
    int n0count;                                                            /* Default 3        See V.150.1 C.4.3.1 */

    /* Explicit acknowledgement variables. */
    /* The current media state of the local node (i.e., the value that will be sent to the remote
       node in the event field of an SSE message) */
    uint8_t lcl_mode;                                                       /* See V.150.1 C.4.3.1 */
    /* The last known media state of the remote node, as known by the local node (i.e. the value
       that will be sent to the remote node in the remote media state field of an SSE extension
       field with explicit acknowledgement) */
    uint8_t rmt_mode;                                                       /* See V.150.1 C.4.3.1 */
    /* The last known mode of the local node known by the remote node, as known by the local node
       (i.e., the value that was received from the remote node in the remote media state field of
       an SSE extension field with explicit acknowledgement) */
    uint8_t rmt_ack;                                                        /* See V.150.1 C.4.3.1 */
    /* Timer to control sending mode change messages to the remote node */
    span_timestamp_t timer_t0;                                              /* See V.150.1 C.4.3.1 */
    /* Timer to recover from lost acknowledgements sent by the remote node */
    span_timestamp_t timer_t1;                                              /* See V.150.1 C.4.3.1 */
    /* Counter used to control sending mode change messages to the remote node */
    int counter_n0;                                                         /* See V.150.1 C.4.3.1 */

    bool immediate_timer;
    bool force_response;

    /* The last timestamp received from the remote gateway or endpoint */
    uint32_t previous_rx_timestamp;

    v150_1_sse_packet_handler_t packet_handler;
    void *packet_user_data;
    v150_1_sse_status_handler_t status_handler;
    void *status_user_data;
    v150_1_sse_timer_handler_t timer_handler;
    void *timer_user_data;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

#endif
/*- End of file ------------------------------------------------------------*/
