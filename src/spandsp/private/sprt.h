/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/sprt.h - An implementation of the SPRT protocol defined in V.150.1
 *                  Annex B, less the packet exchange part
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

#if !defined(_SPANDSP_PRIVATE_SPRT_H_)
#define _SPANDSP_PRIVATE_SPRT_H_

/* Timer TA01 is a buffering timer for ACKs. Start the timer when you buffer the first ACK.
   If TA01 expires before you have three ACKs, or some data to send, send a packet with a
   partically filled ACK section. Table B.3/V.150.1 in the spec implies there are separate
   TA01 timers for the two types of reliable channel, although the suggested values are the
   same. With ACKs for the two reliable channels being mixed in one packet, what would
   different timers really mean?

   Timer TA02 is a kind of keepalive timer for reliable packets. If there are no ACKs and
   no data packets to send for TA02, an ACK only packet for the channel is sent, to keep
   the BASE_SEQUENCE_NO updated. Each type of reliable channel can have a different value for
   TA02, and the suggested values in the spec are different.
   
   Timer TR03 is the retransmit timer for the reliable channels. Packets not acknowledged after
   TR03 times out are retransmitted. */

typedef struct
{
    bool active;

    /* The maximum payload bytes is a per packet limit, which can be different for each
       channel. For channel 0 it is unclear if this should be anything other than zero. */
    int max_payload_bytes;
    /* The window size is only relevant for the reliable channels - channels 1 and 2. */
    int window_size;

    /* TA02 is only relevant for the 2 reliable channels, but make it a per channel timeout */
    int ta02_timeout;
    /* TR03 is only relevant for the 2 reliable channels, but make it a per channel timeout.
       There is a TR03 timeout for every slot in the window, but for each channel they all
       use the same timeout value. */
    int tr03_timeout;

    /* There is a single TA02 timer for each reliable channel. */
    span_timestamp_t ta02_timer;

    /* The base sequence number should always be zero for the unreliable channels.
       For the reliable channels it is the next sequence number to be delivered to
       the application. */
    uint16_t base_sequence_no;
    /* This is the current sequence number for adding the next entry to the queue. */
    uint16_t queuing_sequence_no;

    uint8_t max_tries;

    /* Only used for the reliable channels */
    volatile int buff_in_ptr;
    volatile int buff_acked_out_ptr;
    uint8_t *buff;
    uint16_t *buff_len;
    span_timestamp_t *tr03_timer;
    /* These are small buffers, so just make them statically the size of the largest possible
       window */
    uint8_t prev_in_time[SPRT_MAX_WINDOWS_SIZE];
    uint8_t next_in_time[SPRT_MAX_WINDOWS_SIZE];
    uint8_t remaining_tries[SPRT_MAX_WINDOWS_SIZE];

    uint8_t first_in_time;
    uint8_t last_in_time;

    /* Busy indicates the application is congested, */
    bool busy;
} sprt_chan_t;

struct sprt_state_s
{
    sprt_tx_packet_handler_t tx_packet_handler;
    void *tx_user_data;
    sprt_rx_delivery_handler_t rx_delivery_handler;
    void *rx_user_data;
    sprt_timer_handler_t timer_handler;
    void *timer_user_data;
    span_modem_status_func_t status_handler;
    void *status_user_data;

    span_timestamp_t latest_timer;

    struct
    {
        uint8_t subsession_id;
        uint8_t payload_type;

        sprt_chan_t chan[SPRT_CHANNELS];
    } rx;
    struct
    {
        uint8_t subsession_id;
        uint8_t payload_type;

        sprt_chan_t chan[SPRT_CHANNELS];

        /* The ACK queue is shared across the reliable channels. */
        volatile int ack_queue_ptr;
        uint16_t ack_queue[3];

        /* TA01 is not channel specific. */
        int ta01_timeout;
        span_timestamp_t ta01_timer;
        /* The "immediate" timer is a special to get an immediate callback, without getting
           deeper into nesting, with the protocol calling the app, calling protocol, ad
           infinitum */
        bool immediate_timer;
    } tx;
    /*! \brief Error and flow logging control */
    logging_state_t logging;
#if defined(SPANDSP_FULLY_DEFINE_SPRT_STATE_T)
    /* TODO: This stuff is currently defined as the maximum possible sizes. It could be
             allocated in a more adaptive manner. */
    /*! \brief The data buffer, sized at the time the structure is created. */
    /* TODO: make this buffer area adapt, rather than always being the maximum possible
       size. */

    /* Make these buffers statically as big as they could ever need to be. We may be using
       smaller windows, or a limited maximum payload, and not need all this buffer space.
       However, if we change these values after initialisation we could be in trouble if
       we dynamically allocate just the amount of buffer space we need. */
    uint8_t tc1_rx_buff[(SPRT_MAX_TC1_WINDOWS_SIZE + 1)*SPRT_MAX_TC1_PAYLOAD_BYTES];
    uint16_t tc1_rx_buff_len[SPRT_MAX_TC1_WINDOWS_SIZE + 1];

    uint8_t tc2_rx_buff[(SPRT_MAX_TC2_WINDOWS_SIZE + 1)*SPRT_MAX_TC2_PAYLOAD_BYTES];
    uint16_t tc2_rx_buff_len[SPRT_MAX_TC2_WINDOWS_SIZE + 1];

    /* Make these buffers statically as big as they could ever need to be. We may be using
       smaller windows, or a limited maximum payload, and not need all this buffer space.
       However, if we change these values after initialisation we could be in trouble if
       we dynamically allocate just the amount of buffer space we need. */
    uint8_t tc1_tx_buff[(SPRT_MAX_TC1_WINDOWS_SIZE + 1)*SPRT_MAX_TC1_PAYLOAD_BYTES];
    uint16_t tc1_tx_buff_len[SPRT_MAX_TC1_WINDOWS_SIZE + 1];
    span_timestamp_t tc1_tx_tr03_timer[SPRT_MAX_TC1_WINDOWS_SIZE + 1];

    uint8_t tc2_tx_buff[(SPRT_MAX_TC2_WINDOWS_SIZE + 1)*SPRT_MAX_TC2_PAYLOAD_BYTES];
    uint16_t tc2_tx_buff_len[SPRT_MAX_TC2_WINDOWS_SIZE + 1];
    span_timestamp_t tc2_tx_tr03_timer[SPRT_MAX_TC2_WINDOWS_SIZE + 1];
#endif
};

/* For packet buffer sizing purposes we need the maximum length of a constructed SPRT packet. */
#define SPRT_MAX_PACKET_BYTES                       (12 + 256)

#define SPRT_SEQ_NO_MASK                            0x3FFF

/* Used as the length of the data in a buffer slot when that slot is free */
#define SPRT_LEN_SLOT_FREE                          0xFFFF

#define TR03_QUEUE_FREE_SLOT_TAG                    0xFFU

#endif
/*- End of file ------------------------------------------------------------*/
