/*
 * SpanDSP - a series of DSP components for telephony
 *
 * agcf.h - Floating point automatic gain contro for modems.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2024 Steve Underwood
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

#if !defined(_SPANDSP_AGCF_H_)
#define _SPANDSP_AGCF_H_

typedef struct agcf_state_s agcf_state_t;

typedef struct agcf_descriptor_s
{
    float signal_on_power_threshold;
    float signal_off_power_threshold;
    float signal_target_power;
    /* A persistence check on a signal appearing. */
    int16_t signal_on_persistence_check;
    /* A persistence check on a signal disappearing. */
    int16_t signal_off_persistence_check;
    /* A long persistence check on a signal disappearing. That is
       something that will ride over blips in the signal. */
    int16_t signal_down_persistence_check;
} agcf_descriptor_t;

#define AGC_SAMPLES_PER_CHUNK   40

#if defined(__cplusplus)
extern "C"
{
#endif

/*! \brief Create an AGC descriptor
    \param s The AGC context.
    \param signal_target_power  The power to normalize to, in dBm0.
    \param signal_on_power_threshold  The minimum power to declare signal on, in dBm0.
    \param signal_off_power_threshold  The maximum power to declare signal off, in dBm0.
    \param signal_on_persistence_check  Persistence check count for signal on.
    \param signal_off_persistence_check  Persistence check count for signal off.
    \return A pointer to the initialised context, or NULL if there was a problem. */
SPAN_DECLARE(agcf_descriptor_t *) agcf_make_descriptor(agcf_descriptor_t *s,
                                                       float signal_target_power,
                                                       float signal_on_power_threshold,
                                                       float signal_off_power_threshold,
                                                       int signal_on_persistence_check,
                                                       int signal_off_persistence_check);

SPAN_DECLARE(int) agcf_free_descriptor(agcf_descriptor_t *s);

/*! Process a block of received samples.
    \brief Process a block of received samples.
    \param out The output buffer for the scaled samples.
    \param in The input buffer for the samples.
    \param len The length of the in and out buffers.
    \return True if a signal is present. */
SPAN_DECLARE(bool) agcf_rx(agcf_state_t *s, float out[], const float in[], int len);

/*! Process a block of received samples.
    \brief Process a block of received samples.
    \param out The output buffer for the scaled samples.
    \param in The input buffer for the samples.
    \param len The length of the in and out buffers.
    \return True if a signal is present. */
SPAN_DECLARE(bool) agcf_from_int16_rx(agcf_state_t *s, float out[], const int16_t in[], int len);

SPAN_DECLARE(float) agcf_current_power_dbm0(agcf_state_t *s);

/*! Get the current scaling. */
SPAN_DECLARE(float) agcf_get_scaling(agcf_state_t *s);

/*! Set the scaling, instead of adapting it. This allows a known good scaling factor
    to be resused within a session. */
SPAN_DECLARE(void) agcf_set_scaling(agcf_state_t *s, float scaling);

/*! Enable or disable AGC adpation. */
SPAN_DECLARE(void) agcf_set_adaption(agcf_state_t *s, bool adapt);

/*! Get the logging context associated with an AGC context.
    \brief Get the logging context associated with an AGC context.
    \param s The AGC context.
    \return A pointer to the logging context */
SPAN_DECLARE(logging_state_t *) agcf_get_logging_state(agcf_state_t *s);

/*! \brief Initialise an AGC context.
    \param s The AGC context.
    \param desc
    \return A pointer to the initialised context, or NULL if there was a problem.
*/
SPAN_DECLARE(agcf_state_t *) agcf_init(agcf_state_t *s, const agcf_descriptor_t *desc);

/*! \brief Release an AGC receive context.
    \param s The ADSI receive context.
    \return 0 for OK.
*/
SPAN_DECLARE(int) agcf_release(agcf_state_t *s);

/*! \brief Free the resources of an ADSI receive context.
    \param s The ADSI receive context.
    \return 0 for OK.
*/
SPAN_DECLARE(int) agcf_free(agcf_state_t *s);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
