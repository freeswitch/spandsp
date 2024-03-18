/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v34.h - ITU V.34 modem
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2009 Steve Underwood
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

/*! \page v34_page The V.34 modem
\section v34_page_sec_1 What does it do?

\section v34__page_sec_2 How does it work?
*/

#if !defined(_SPANDSP_V34_H_)
#define _SPANDSP_V34_H_

#if defined(SPANDSP_USE_FIXED_POINT)
#define V34_CONSTELLATION_SCALING_FACTOR        512.0
#else
#define V34_CONSTELLATION_SCALING_FACTOR        1.0
#endif

enum v34_supported_bit_rates_e
{
    V34_SUPPORT_2400  = 0x0001,
    V34_SUPPORT_4800  = 0x0002,
    V34_SUPPORT_7200  = 0x0004,
    V34_SUPPORT_9600  = 0x0008,
    V34_SUPPORT_12000 = 0x0010,
    V34_SUPPORT_14400 = 0x0020,
    V34_SUPPORT_16800 = 0x0040,
    V34_SUPPORT_19200 = 0x0080,
    V34_SUPPORT_21600 = 0x0100,
    V34_SUPPORT_24000 = 0x0200,
    V34_SUPPORT_26400 = 0x0400,
    V34_SUPPORT_28800 = 0x0800,
    V34_SUPPORT_31200 = 0x1000,
    V34_SUPPORT_33600 = 0x2000
};

enum v34_half_duplex_modes_e
{
    /* Make this the source side modem in the half-duplex exchange */
    V34_HALF_DUPLEX_SOURCE,
    /* Make this the recipient side modem in the half-duplex exchange */
    V34_HALF_DUPLEX_RECIPIENT,
    /* Start control channel operation */
    V34_HALF_DUPLEX_CONTROL_CHANNEL,
    /* Start primary channel operation in the current source/recipient mode */
    V34_HALF_DUPLEX_PRIMARY_CHANNEL,
    /* Stop transmission */
    V34_HALF_DUPLEX_SILENCE
};

/*!
    V.34 modem descriptor. This defines the working state for a single instance
    of a V.34 modem.
*/
typedef struct v34_state_s v34_state_t;

#if defined(__cplusplus)
extern "C"
{
#endif

/*! Process a block of received V.34 modem audio samples.
    \brief Process a block of received V.34 modem audio samples.
    \param s The modem context.
    \param amp The audio sample buffer.
    \param len The number of samples in the buffer.
    \return The number of samples unprocessed. */
SPAN_DECLARE(int) v34_rx(v34_state_t *s, const int16_t amp[], int len);

/*! Fake processing of a missing block of received V.34 modem audio samples.
    (e.g due to packet loss).
    \brief Fake processing of a missing block of received V.34 modem audio samples.
    \param s The modem context.
    \param len The number of samples to fake.
    \return The number of samples unprocessed. */
SPAN_DECLARE(int) v34_rx_fillin(v34_state_t *s, int len);

/*! Get a snapshot of the current equalizer coefficients.
    \brief Get a snapshot of the current equalizer coefficients.
    \param coeffs The vector of complex coefficients.
    \return The number of coefficients in the vector. */
#if defined(SPANDSP_USE_FIXED_POINT)
SPAN_DECLARE(int) v34_equalizer_state(v34_state_t *s, complexi16_t **coeffs);
#else
SPAN_DECLARE(int) v34_equalizer_state(v34_state_t *s, complexf_t **coeffs);
#endif

/*! Get the current received carrier frequency.
    \param s The modem context.
    \return The frequency, in Hertz. */
SPAN_DECLARE(float) v34_rx_carrier_frequency(v34_state_t *s);

/*! Get the current symbol timing correction since startup.
    \param s The modem context.
    \return The correction. */
SPAN_DECLARE(float) v34_rx_symbol_timing_correction(v34_state_t *s);

/*! Get a current received signal power.
    \param s The modem context.
    \return The signal power, in dBm0. */
SPAN_DECLARE(float) v34_rx_signal_power(v34_state_t *s);

/*! Set the power level at which the carrier detection will cut in
    \param s The modem context.
    \param cutoff The signal cutoff power, in dBm0. */
SPAN_DECLARE(void) v34_rx_set_signal_cutoff(v34_state_t *s, float cutoff);

/*! Set a handler routine to process QAM status reports
    \param s The modem context.
    \param handler The handler routine.
    \param user_data An opaque pointer passed to the handler routine. */
SPAN_DECLARE(void) v34_set_qam_report_handler(v34_state_t *s, qam_report_handler_t handler, void *user_data);

/*! Generate a block of V.34 modem audio samples.
    \brief Generate a block of V.34 modem audio samples.
    \param s The modem context.
    \param amp The audio sample buffer.
    \param len The number of samples to be generated.
    \return The number of samples actually generated. */
SPAN_DECLARE(int) v34_tx(v34_state_t *s, int16_t amp[], int len);

/*! Adjust a V.34 modem transmit context's power output.
    \brief Adjust a V.34 modem transmit context's output power.
    \param s The modem context.
    \param power The power level, in dBm0 */
SPAN_DECLARE(void) v34_tx_power(v34_state_t *s, float power);

/*! Report the current operating bit rate of a V.34 modem context.
    \brief Report the current operating bit rate of a V.34 modem context
    \param s The modem context.
    \return ??? */
SPAN_DECLARE(int) v34_get_current_bit_rate(v34_state_t *s);

/*! Change the operating mode of a V.34 half-duplex modem.
    \brief Change the operating mode of a V.34 half-duplex modem.
    \param s The modem context.
    \param mode The new mode to be selected.
    \return ??? */
SPAN_DECLARE(int) v34_half_duplex_change_mode(v34_state_t *s, int mode);

/*! Reinitialise an existing V.34 modem context, so it may be reused.
    \brief Reinitialise an existing V.34 modem context.
    \param s The modem context.
    \param baud_rate The baud rate of the modem. Valid values are 2400, 2743, 2800, 3000, 3200 and 3429
    \param bit_rate The bit rate of the modem. Valid values are 4800, 7200, 9600, 12000 and 14400.
    \param duplex True if this is a full duplex mode modem. Otherwise this is a half-duplex modem.
    \return 0 for OK, -1 for bad parameter */
SPAN_DECLARE(int) v34_restart(v34_state_t *s, int baud_rate, int bit_rate, bool duplex);

/*! Initialise a V.34 modem context. This must be called before the first
    use of the context, to initialise its contents.
    \brief Initialise a V.34 modem context.
    \param s The modem context.
    \param baud_rate The baud rate of the modem. Valid values are 2400, 2743, 2800, 3000, 3200 and 3429
    \param bit_rate The bit rate of the modem. Valid values are 4800, 7200, 9600, 12000 and 14400.
    \param calling_party True if this is the calling modem.
    \param duplex True if this is a full duplex mode modem. Otherwise this is a half-duplex modem.
    \param get_bit The callback routine used to get the data to be transmitted.
    \param get_bit_user_data An opaque pointer, passed in calls to the gett routine.
    \param put_bit The callback routine used to get the data to be transmitted.
    \param put_bit_user_data An opaque pointer, passed in calls to the put routine.
    \return A pointer to the modem context, or NULL if there was a problem. */
SPAN_DECLARE(v34_state_t *) v34_init(v34_state_t *s,
                                     int baud_rate,
                                     int bit_rate,
                                     bool calling_party,
                                     bool duplex,
                                     span_get_bit_func_t get_bit,
                                     void *get_bit_user_data,
                                     span_put_bit_func_t put_bit,
                                     void *put_bit_user_data);

/*! Release a V.34 modem receive context.
    \brief Release a V.34 modem receive context.
    \param s The modem context.
    \return 0 for OK */
SPAN_DECLARE(int) v34_release(v34_state_t *s);

/*! Free a V.34 modem receive context.
    \brief Free a V.34 modem receive context.
    \param s The modem context.
    \return 0 for OK */
SPAN_DECLARE(int) v34_free(v34_state_t *s);

/*! Get the logging context associated with a V.34 modem context.
    \brief Get the logging context associated with a V.34 modem context.
    \param s The modem context.
    \return A pointer to the logging context */
SPAN_DECLARE(logging_state_t *) v34_get_logging_state(v34_state_t *s);

/*! Change the get_bit function associated with a V.34 modem context.
    \brief Change the get_bit function associated with a V.34 modem context.
    \param s The modem context.
    \param get_bit The callback routine used to get the data to be transmitted.
    \param user_data An opaque pointer. */
SPAN_DECLARE(void) v34_set_get_bit(v34_state_t *s, span_get_bit_func_t get_bit, void *user_data);

/*! Change the get_aux_bit function associated with a V.34 modem context.
    \brief Change the get_aux_bit function associated with a V.34 modem context.
    \param s The modem context.
    \param get_bit The callback routine used to get the aux. data to be transmitted.
    \param user_data An opaque pointer. */
SPAN_DECLARE(void) v34_set_get_aux_bit(v34_state_t *s, span_get_bit_func_t get_bit, void *user_data);

/*! Change the put_bit function associated with a V.34 modem context.
    \brief Change the put_bit function associated with a V.34 modem context.
    \param s The modem context.
    \param put_bit The callback routine used to process the data received.
    \param user_data An opaque pointer. */
SPAN_DECLARE(void) v34_set_put_bit(v34_state_t *s, span_put_bit_func_t put_bit, void *user_data);

/*! Change the put_aux_bit function associated with a V.34 modem context.
    \brief Change the put_aux_bit function associated with a V.34 modem context.
    \param s The modem context.
    \param put_bit The callback routine used to process the aux data received.
    \param user_data An opaque pointer. */
SPAN_DECLARE(void) v34_set_put_aux_bit(v34_state_t *s, span_put_bit_func_t put_bit, void *user_data);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
