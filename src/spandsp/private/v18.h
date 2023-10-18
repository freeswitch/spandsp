/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/v18.h - V.18 text telephony for the deaf.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004-2009 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#if !defined(_SPANDSP_PRIVATE_V18_H_)
#define _SPANDSP_PRIVATE_V18_H_

enum
{
    GOERTZEL_TONE_SET_390HZ     = 0,
    GOERTZEL_TONE_SET_980HZ     = 1,
    GOERTZEL_TONE_SET_1180HZ    = 2,
    GOERTZEL_TONE_SET_1270HZ    = 3,
    GOERTZEL_TONE_SET_1300HZ    = 4,
    GOERTZEL_TONE_SET_1400HZ    = 5,
    GOERTZEL_TONE_SET_1650HZ    = 6,
    GOERTZEL_TONE_SET_1800HZ    = 7,
    GOERTZEL_TONE_SET_2225HZ    = 8,
    GOERTZEL_TONE_SET_ENTRIES   = 9
};

enum
{
    V18_TX_STATE_ORIGINATING_1  = 1,
    V18_TX_STATE_ORIGINATING_2  = 2,
    V18_TX_STATE_ORIGINATING_3  = 3,
    V18_TX_STATE_ORIGINATING_42 = 42,

    V18_TX_STATE_ANSWERING_1    = 101,
    V18_TX_STATE_ANSWERING_2    = 102,
    V18_TX_STATE_ANSWERING_3    = 103,
    V18_TX_STATE_ANSWERING_42   = 142
};

enum
{
    V18_RX_STATE_ORIGINATING_1  = 1,
    V18_RX_STATE_ORIGINATING_2  = 2,
    V18_RX_STATE_ORIGINATING_3  = 3,
    V18_RX_STATE_ORIGINATING_42 = 42,

    V18_RX_STATE_ANSWERING_1    = 101,
    V18_RX_STATE_ANSWERING_2    = 102,
    V18_RX_STATE_ANSWERING_3    = 103,
    V18_RX_STATE_ANSWERING_42   = 142
};

struct v18_state_s
{
    /*! \brief True if we are the calling modem */
    bool calling_party;
    int initial_mode;
    int nation;
    span_put_msg_func_t put_msg;
    void *put_msg_user_data;
    span_modem_status_func_t status_handler;
    void *status_handler_user_data;
    bool repeat_shifts;
    bool autobauding;
    /* The stored message is used during probing. See V.18/5.2.12.1 */
    char stored_message[81];
    int current_mode;
    int tx_state;
    int rx_state;
    union
    {
        queue_state_t queue;
        uint8_t buf[QUEUE_STATE_T_SIZE(128)];
    } queue;
    tone_gen_descriptor_t alert_tone_desc;
    tone_gen_state_t alert_tone_gen;
    fsk_tx_state_t fsk_tx;
    dtmf_tx_state_t dtmf_tx;
    async_tx_state_t async_tx;
    int baudot_tx_shift;
    int tx_signal_on;
    bool tx_draining;
    uint8_t next_byte;

    fsk_rx_state_t fsk_rx;
    dtmf_rx_state_t dtmf_rx;
    modem_connect_tones_rx_state_t answer_tone_rx;

#if defined(SPANDSP_USE_FIXED_POINTx)
    /*! Minimum acceptable tone level for detection. */
    int32_t threshold;
    /*! The accumlating total energy on the same period over which the Goertzels work. */
    int32_t energy;
#else
    /*! Minimum acceptable tone level for detection. */
    float threshold;
    /*! The accumlating total energy on the same period over which the Goertzels work. */
    float energy;
#endif
    goertzel_state_t tone_set[GOERTZEL_TONE_SET_ENTRIES];
    /*! The current sample number within a tone processing block. */
    int current_goertzel_sample;
    /*! Tone state duration */
    span_sample_timer_t tone_duration;
    span_sample_timer_t target_tone_duration;
    int in_tone;

    int baudot_rx_shift;
    uint8_t rx_msg[256 + 1];
    int rx_msg_len;
    span_sample_timer_t msg_in_progress_timer;

    span_sample_timer_t rx_suppression_timer;
    span_sample_timer_t tx_suppression_timer;

    span_sample_timer_t ta_interval;
    span_sample_timer_t tc_interval;
    span_sample_timer_t te_interval;
    span_sample_timer_t tm_interval;
    span_sample_timer_t tr_interval;
    span_sample_timer_t tt_interval;

    span_sample_timer_t ta_timer;       // 3s automoding timer
    span_sample_timer_t tc_timer;       // 6s probing timer
    span_sample_timer_t te_timer;       // 2.7s probing timer
    span_sample_timer_t tm_timer;       // 3s probing timer
    span_sample_timer_t tr_timer;       // 2s 
    span_sample_timer_t tt_timer;       // 3s return to probing timer

    int txp_cnt;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

#endif
/*- End of file ------------------------------------------------------------*/
