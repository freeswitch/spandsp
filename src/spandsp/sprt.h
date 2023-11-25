/*
 * SpanDSP - a series of DSP components for telephony
 *
 * sprt.h - An implementation of the SPRT protocol defined in V.150.1
 *          Annex B, less the packet exchange part
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

#if !defined(_SPANDSP_SPRT_H_)
#define _SPANDSP_SPRT_H_

#define SPRT_MIN_TC0_PAYLOAD_BYTES                  140
#define SPRT_MAX_TC0_PAYLOAD_BYTES                  256
#define SPRT_DEFAULT_TC0_PAYLOAD_BYTES              140

#define SPRT_MIN_TC1_PAYLOAD_BYTES                  132
#define SPRT_MAX_TC1_PAYLOAD_BYTES                  256
#define SPRT_DEFAULT_TC1_PAYLOAD_BYTES              132

#define SPRT_MIN_TC1_WINDOWS_SIZE                   32
#define SPRT_MAX_TC1_WINDOWS_SIZE                   96
#define SPRT_DEFAULT_TC1_WINDOWS_SIZE               32

#define SPRT_MIN_TC2_PAYLOAD_BYTES                  132
#define SPRT_MAX_TC2_PAYLOAD_BYTES                  256
#define SPRT_DEFAULT_TC2_PAYLOAD_BYTES              132

#define SPRT_MIN_TC2_WINDOWS_SIZE                   8
#define SPRT_MAX_TC2_WINDOWS_SIZE                   32
#define SPRT_DEFAULT_TC2_WINDOWS_SIZE               8

#define SPRT_MIN_TC3_PAYLOAD_BYTES                  140
#define SPRT_MAX_TC3_PAYLOAD_BYTES                  256
#define SPRT_DEFAULT_TC3_PAYLOAD_BYTES              140

/* Max window size for any channel. */
#define SPRT_MAX_WINDOWS_SIZE                       96

/* Only typical values are specified for the timers */

#define SPRT_DEFAULT_TIMER_TC1_TA01                 90000       /* us */
#define SPRT_DEFAULT_TIMER_TC1_TA02                 130000      /* us */
#define SPRT_DEFAULT_TIMER_TC1_TR03                 500000      /* us */

#define SPRT_DEFAULT_TIMER_TC2_TA01                 90000       /* us */
#define SPRT_DEFAULT_TIMER_TC2_TA02                 500000      /* us */
#define SPRT_DEFAULT_TIMER_TC2_TR03                 500000      /* us */

#define SPRT_MIN_MAX_TRIES                          1
#define SPRT_MAX_MAX_TRIES                          20
#define SPRT_DEFAULT_MAX_TRIES                      10

enum sprt_status_e
{
    SPRT_STATUS_OK                                  = 0,
    SPRT_STATUS_EXCESS_RETRIES                      = 1,
    SPRT_STATUS_SUBSESSION_CHANGED                  = 2,
    SPRT_STATUS_OUT_OF_SEQUENCE                     = 3
};

/* This view of the transmission channels divides them into an overall range, and a reliable subset
   range within the overall range. */
enum sprt_tcid_range_view_e
{
    SPRT_TCID_MIN                                   = 0,
    SPRT_TCID_MIN_RELIABLE                          = 1,
    SPRT_TCID_MAX_RELIABLE                          = 2,
    SPRT_TCID_MAX                                   = 3
};

/* The total number of channels */
#define SPRT_CHANNELS                               4

/* This view of the transmission channels specifically names them, for direct access. */
enum sprt_tcid_e
{
    SPRT_TCID_UNRELIABLE_UNSEQUENCED                = 0,  /* Used for ack only */
    SPRT_TCID_RELIABLE_SEQUENCED                    = 1,  /* Used for data */
    SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED          = 2,  /* Used for control/signalling data */
    SPRT_TCID_UNRELIABLE_SEQUENCED                  = 3   /* Used for sequenced data that does not require reliable delivery */
};

enum sprt_timer_e
{
    SPRT_TIMER_TA01                                 = 0,
    SPRT_TIMER_TA02                                 = 1,
    SPRT_TIMER_TR03                                 = 2
};

enum sprt_timer_action_e
{
    SPRT_TIMER_SET                                  = 0,
    SPRT_TIMER_CLEAR                                = 1,
    SPRT_TIMER_ADJUST                               = 2
};

typedef struct
{
    uint16_t payload_bytes;
    uint16_t window_size;
    int timer_ta01;
    int timer_ta02;
    int timer_tr03;
} channel_parms_t;

typedef int (*sprt_tx_packet_handler_t) (void *user_data, const uint8_t pkt[], int len);
typedef int (*sprt_rx_delivery_handler_t) (void *user_data, int channel, int seq_no, const uint8_t msg[], int len);
typedef span_timestamp_t (*sprt_timer_handler_t) (void *user_data, span_timestamp_t timeout);

typedef struct sprt_state_s sprt_state_t;

#if defined(__cplusplus)
extern "C" {
#endif

/*! \brief Find the name of an SPRT channel.
    \param channel The number of the SPRT channel (0 to 3).
    \return A pointer to a short string name for the channel, or NULL for an invalid channel. */
SPAN_DECLARE(const char *) sprt_transmission_channel_to_str(int channel);

SPAN_DECLARE(int) sprt_timer_expired(sprt_state_t *s, span_timestamp_t now);

/*! \brief Process a packet arriving from the far end. If the packet validates as an SPRT
           packet 0 is returned. If the packet does not follow the structure of an SPRT
           packet, or its packet type field does not contain the expected value, -1 is
           returned. In a mixed packet environment, where things like RTP, T.38 and SPRT
           packets are mixed in the same stream, -1 should indicate than one of the other
           packet sinks should be tried.
    \param s The SPRT context.
    \param pkt The SPRT packet buffer.
    \param len The length of the packet.
    \return 0 for accepted as a valid SPRT packet. -1 for rejected as an SPRT packet.*/
SPAN_DECLARE(int) sprt_rx_packet(sprt_state_t *s, const uint8_t pkt[], int len);

/*! \brief Send a message to a SPRT channel.
    \param s The SPRT context.
    \param channel The SPRT channel.
    \param buf The message.
    \param len The length of the message.
    \return 0 for OK. */
SPAN_DECLARE(int) sprt_tx(sprt_state_t *s, int channel, const uint8_t buf[], int len);

SPAN_DECLARE(int) sprt_set_local_tc_windows_size(sprt_state_t *s, int channel, int size);

SPAN_DECLARE(int) sprt_get_local_tc_windows_size(sprt_state_t *s, int channel);

SPAN_DECLARE(int) sprt_set_local_tc_payload_bytes(sprt_state_t *s, int channel, int max_len);

SPAN_DECLARE(int) sprt_get_local_tc_payload_bytes(sprt_state_t *s, int channel);

SPAN_DECLARE(int) sprt_set_local_tc_max_tries(sprt_state_t *s, int channel, int max_tries);

SPAN_DECLARE(int) sprt_get_local_tc_max_tries(sprt_state_t *s, int channel);

SPAN_DECLARE(int) sprt_set_far_tc_payload_bytes(sprt_state_t *s, int channel, int max_len);

SPAN_DECLARE(int) sprt_get_far_tc_payload_bytes(sprt_state_t *s, int channel);

SPAN_DECLARE(int) sprt_set_far_tc_windows_size(sprt_state_t *s, int channel, int size);

SPAN_DECLARE(int) sprt_get_far_tc_windows_size(sprt_state_t *s, int channel);

SPAN_DECLARE(int) sprt_set_tc_timeout(sprt_state_t *s, int channel, int timer, int timeout);

SPAN_DECLARE(int) sprt_get_tc_timeout(sprt_state_t *s, int channel, int timer);

/*! Set whether the local end of the specified channel of the SPRT context is currently busy.
    \brief Test if local end of SPRT context is busy.
    \param s The SPRT context.
    \param channel The SPRT channel number.
    \param busy true for busy.
    \return true for previously busy */
SPAN_DECLARE(int) sprt_set_local_busy(sprt_state_t *s, int channel, bool busy);

/*! Test whether the far end of the specified channel of the SPRT context is currently busy.
    \brief Test if far end of SPRT context is busy.
    \param s The SPRT context.
    \param channel The SPRT channel number.
    \return true for busy */
SPAN_DECLARE(bool) sprt_get_far_busy_status(sprt_state_t *s, int channel);

/*! Get the logging context associated with an SPRT context.
    \brief Get the logging context associated with an SPRT context.
    \param s The SPRT context.
    \return A pointer to the logging context */
SPAN_DECLARE(logging_state_t *) sprt_get_logging_state(sprt_state_t *s);

/*! \brief Initialise an SPRT context.
    \param s The SPRT context.
    \param subsession_id The subsession ID for transmitted SPRT headers
    \param rx_payload_type The payload type expected in received SPRT headers
    \param tx_payload_type The payload type sent in transmitted SPRT headers
    \param parms The parameter set for sizing the SPRT instance. NULL means use the defaults.
    \param tx_packet_handler The callback function, used to send assembled packets.
    \param tx_user_data An opaque pointer supplied to tx_packet_handler.
    \param rx_delivery_handler The callback function, used to report arriving packets.
    \param rx_user_data An opaque pointer supplied to rx_delivery_handler.
    \param timer_handler The callback function, used to control the timers used by SPRT.
    \param timer_user_data An opaque pointer supplied to timer_handler.
    \param status_handler The callback function, used to report status events.
    \param status_user_data An opaque pointer supplied to status_handler.
    \return A pointer to the SPRT context, or NULL if there was a problem. */
SPAN_DECLARE(sprt_state_t *) sprt_init(sprt_state_t *s,
                                       uint8_t subsession_id,
                                       uint8_t rx_payload_type,
                                       uint8_t tx_payload_type,
                                       channel_parms_t parms[SPRT_CHANNELS],
                                       sprt_tx_packet_handler_t tx_packet_handler,
                                       void *tx_user_data,
                                       sprt_rx_delivery_handler_t rx_delivery_handler,
                                       void *rx_user_data,
                                       sprt_timer_handler_t timer_handler,
                                       void *timer_user_data,
                                       span_modem_status_func_t status_handler,
                                       void *status_user_data);

/*! \brief Release an SPRT context.
    \param s The SPRT context.
    \return 0 for OK. */
SPAN_DECLARE(int) sprt_release(sprt_state_t *s);

/*! \brief Free an SPRT context.
    \param s The SPRT context.
    \return 0 for OK. */
SPAN_DECLARE(int) sprt_free(sprt_state_t *s);

#if defined(__cplusplus)
}
#endif
#endif
/*- End of file ------------------------------------------------------------*/
