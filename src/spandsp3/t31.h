/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t31.h - A T.31 compatible class 1 FAX modem interface.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004 Steve Underwood
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

/*! \file */

#if !defined(_SPANDSP_T31_H_)
#define _SPANDSP_T31_H_

/*! \page t31_page T.31 Class 1 FAX modem protocol handling
\section t31_page_sec_1 What does it do?
The T.31 class 1 FAX modem modules implements a class 1 interface to the FAX
modems in spandsp.

\section t31_page_sec_2 How does it work?
*/

/*!
    T.31 descriptor. This defines the working state for a single instance of
    a T.31 FAX modem.
*/
typedef struct t31_state_s t31_state_t;

typedef int (*t31_modem_control_handler_t)(t31_state_t *s, void *user_data, int op, const char *num);

#if defined(__cplusplus)
extern "C"
{
#endif

SPAN_DECLARE(void) t31_call_event(t31_state_t *s, int event);

/*! Return the amount of free space in the AT COMMAND BUFFER.
    \brief Return the amount of free space in the AT COMMAND BUFFER.
    \param s The T.31 modem context.
    \return The number of bytes of free space. */
SPAN_DECLARE(int) t31_at_rx_free_space(t31_state_t *s);

SPAN_DECLARE(int) t31_at_rx(t31_state_t *s, const char *t, int len);

/*! Process a block of received T.31 modem audio samples.
    \brief Process a block of received T.31 modem audio samples.
    \param s The T.31 modem context.
    \param amp The audio sample buffer.
    \param len The number of samples in the buffer.
    \return The number of samples unprocessed. */
SPAN_DECLARE(int) t31_rx(t31_state_t *s, int16_t amp[], int len);

/*! Fake processing of a missing block of received T.31 modem audio samples
    (e.g due to packet loss).
    \brief Fake processing of a missing block of received T.31 modem audio samples.
    \param s The T.31 modem context.
    \param len The number of samples to fake.
    \return The number of samples unprocessed. */
SPAN_DECLARE(int) t31_rx_fillin(t31_state_t *s, int len);

/*! Generate a block of T.31 modem audio samples.
    \brief Generate a block of T.31 modem audio samples.
    \param s The T.31 modem context.
    \param amp The audio sample buffer.
    \param max_len The number of samples to be generated.
    \return The number of samples actually generated.
*/
SPAN_DECLARE(int) t31_tx(t31_state_t *s, int16_t amp[], int max_len);

SPAN_DECLARE(int) t31_t38_send_timeout(t31_state_t *s, int samples);

/*! Select whether silent audio will be sent when transmit is idle.
    \brief Select whether silent audio will be sent when transmit is idle.
    \param s The T.31 modem context.
    \param transmit_on_idle True if silent audio should be output when the transmitter is
           idle. False to transmit zero length audio when the transmitter is idle. The default
           behaviour is false.
*/
SPAN_DECLARE(void) t31_set_transmit_on_idle(t31_state_t *s, bool transmit_on_idle);

/*! Select whether TEP mode will be used (or time allowed for it (when transmitting).
    \brief Select whether TEP mode will be used.
    \param s The T.31 modem context.
    \param use_tep True if TEP is to be used.
*/
SPAN_DECLARE(void) t31_set_tep_mode(t31_state_t *s, bool use_tep);

/*! Select whether T.38 data will be paced as it is transmitted.
    \brief Select whether T.38 data will be paced.
    \param s The T.31 modem context.
    \param without_pacing True if data is to be sent as fast as possible. False if it is
           to be paced.
*/
SPAN_DECLARE(void) t31_set_t38_config(t31_state_t *s, bool without_pacing);

/*! Set audio or T.38 mode.
    \brief Set audio or T.38 mode.
    \param s The T.31 modem context.
    \param t38_mode True for T.38 mode operation. False for audio mode operation.
*/
SPAN_DECLARE(void) t31_set_mode(t31_state_t *s, bool t38_mode);

/*! Get a pointer to the logging context associated with a T.31 context.
    \brief Get a pointer to the logging context associated with a T.31 context.
    \param s The T.31 context.
    \return A pointer to the logging context, or NULL.
*/
SPAN_DECLARE(logging_state_t *) t31_get_logging_state(t31_state_t *s);

/*! Get a pointer to the AT interpreter context associated with a T.31 context.
    \brief Get a pointer to the AT interpreter context associated with a T.31 context.
    \param s The T.31 context.
    \return A pointer to the AT interpreter context, or NULL.
*/
SPAN_DECLARE(at_state_t *) t31_get_at_state(t31_state_t *s);

/*! Get a pointer to the T.38 core context associated with a T.31 context.
    \brief Get a pointer to the T.38 core context associated with a T.31 context.
    \param s The T.31 context.
    \return A pointer to the T.38 core context, or NULL.
*/
SPAN_DECLARE(t38_core_state_t *) t31_get_t38_core_state(t31_state_t *s);

/*! Initialise a T.31 context. This must be called before the first
    use of the context, to initialise its contents.
    \brief Initialise a T.31 context.
    \param s The T.31 context.
    \param at_tx_handler A callback routine to handle AT interpreter channel output.
    \param at_tx_user_data An opaque pointer passed in called to at_tx_handler.
    \param modem_control_handler A callback routine to handle control of the modem (off-hook, etc).
    \param modem_control_user_data An opaque pointer passed in called to modem_control_handler.
    \param tx_t38_packet_handler ???
    \param tx_t38_packet_user_data ???
    \return A pointer to the T.31 context. */
SPAN_DECLARE(t31_state_t *) t31_init(t31_state_t *s,
                                     at_tx_handler_t at_tx_handler,
                                     void *at_tx_user_data,
                                     t31_modem_control_handler_t modem_control_handler,
                                     void *modem_control_user_data,
                                     t38_tx_packet_handler_t tx_t38_packet_handler,
                                     void *tx_t38_packet_user_data);

/*! Release a T.31 context.
    \brief Release a T.31 context.
    \param s The T.31 context.
    \return 0 for OK */
SPAN_DECLARE(int) t31_release(t31_state_t *s);

/*! Free a T.31 context.
    \brief Release a T.31 context.
    \param s The T.31 context.
    \return 0 for OK */
SPAN_DECLARE(int) t31_free(t31_state_t *s);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
