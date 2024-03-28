/*
 * SpanDSP - a series of DSP components for telephony
 *
 * agc_float.c - Floating point automatic gain control for modems.
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/logging.h"
#include "spandsp/math_fixed.h"
#include "spandsp/agc_float.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/agc_float.h"

#define DC_BLOCK_COEFF  0.9921875 // (1-2^(-7))

SPAN_DECLARE(agcf_descriptor_t *) agcf_make_descriptor(agcf_descriptor_t *s,
                                                       float signal_target_power,
                                                       float signal_on_power_threshold,
                                                       float signal_off_power_threshold,
                                                       int signal_on_persistence_check,
                                                       int signal_off_persistence_check)
{
    if (signal_on_power_threshold < signal_off_power_threshold)
        return NULL;
    /*endif*/

    if (s == NULL)
    {
        if ((s = (agcf_descriptor_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
        /*endif*/
    }
    /*endif*/
    memset(s, 0, sizeof(*s));
    s->signal_target_power = energy_threshold_dbm0(AGC_SAMPLES_PER_CHUNK, signal_target_power);
    s->signal_on_power_threshold = energy_threshold_dbm0(AGC_SAMPLES_PER_CHUNK, signal_on_power_threshold);
    s->signal_off_power_threshold = energy_threshold_dbm0(AGC_SAMPLES_PER_CHUNK, signal_off_power_threshold);
    s->signal_on_persistence_check = signal_on_persistence_check + 1;
    s->signal_off_persistence_check = signal_off_persistence_check + 1;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) agcf_free_descriptor(agcf_descriptor_t *s)
{
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(bool) agcf_from_int16_rx(agcf_state_t *s, float out[], const int16_t in[], int len)
{
    int i;
    float sample;
    float sample_no_dc;

    if (s->adapt  ||  s->detect)
    {
        for (i = 0;  i < len;  i++)
        {
            sample = in[i];
            /* Block DC from being counted as part of the signal energy */
            sample_no_dc = sample - s->dc_block_x + DC_BLOCK_COEFF*s->dc_block_y;
            s->dc_block_x = sample;
            s->dc_block_y = sample_no_dc;
            s->current_energy += sample_no_dc*sample_no_dc;
            if (++s->current_samples >= AGC_SAMPLES_PER_CHUNK)
            {
                s->last_power = s->current_energy;
                if (s->last_power >= s->desc.signal_on_power_threshold)
                {
                    if (s->signal_on_persistence < s->desc.signal_on_persistence_check)
                    {
                        if (++s->signal_on_persistence == s->desc.signal_on_persistence_check)
                        {
                            s->signal_present = true;
                        }
                        /*endif*/
                    }
                    /*endif*/
                }
                else
                {
                    s->signal_on_persistence = 0;
                    if (s->last_power <= s->desc.signal_off_power_threshold)
                    {
                        if (s->signal_off_persistence < s->desc.signal_off_persistence_check)
                        {
                            if (++s->signal_off_persistence == s->desc.signal_off_persistence_check)
                            {
                                s->signal_present = false;
                            }
                            /*endif*/
                        }
                        /*endif*/
                    }
                    else
                    {
                        s->signal_off_persistence = 0;
                    }
                    /*endif*/
                }
                /*endif*/
                if (s->signal_present  &&  s->adapt)
                {
                    if (s->last_power)
                        s->gain = sqrt(s->desc.signal_target_power/s->last_power);
                    else
                        s->gain = 1.0f;
                    /*endif*/
                }
                /*endif*/
                s->current_energy = 0;
                s->current_samples = 0;
            }
            /*endif*/
        }
        /*endfor*/
    }
    /*endif*/
    if (s->scale_signal)
    {
        for (i = 0;  i < len;  i++)
            out[i] = in[i]*s->gain;
        /*endfor*/
    }
    /*endif*/
    return s->signal_present;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(bool) agcf_rx(agcf_state_t *s, float out[], const float in[], int len)
{
    int i;
    float sample;
    float sample_no_dc;

    if (s->adapt  ||  s->detect)
    {
        for (i = 0;  i < len;  i++)
        {
            sample = in[i];
            /* Block DC from being counted as part of the signal energy */
            sample_no_dc = sample - s->dc_block_x + DC_BLOCK_COEFF*s->dc_block_y;
            s->dc_block_x = sample;
            s->dc_block_y = sample_no_dc;
            s->current_energy += sample_no_dc*sample_no_dc;
            if (++s->current_samples >= AGC_SAMPLES_PER_CHUNK)
            {
                s->last_power = s->current_energy;
                if (s->last_power >= s->desc.signal_on_power_threshold)
                {
                    if (s->signal_on_persistence < s->desc.signal_on_persistence_check)
                    {
                        if (++s->signal_on_persistence == s->desc.signal_on_persistence_check)
                        {
                            s->signal_present = true;
                        }
                        /*endif*/
                    }
                    /*endif*/
                }
                else
                {
                    s->signal_on_persistence = 0;
                    if (s->last_power <= s->desc.signal_off_power_threshold)
                    {
                        if (s->signal_off_persistence < s->desc.signal_off_persistence_check)
                        {
                            if (++s->signal_off_persistence == s->desc.signal_off_persistence_check)
                            {
                                s->signal_present = false;
                            }
                            /*endif*/
                        }
                        /*endif*/
                    }
                    else
                    {
                        s->signal_off_persistence = 0;
                    }
                    /*endif*/
                }
                /*endif*/
                if (s->signal_present  &&  s->adapt)
                {
                    if (s->last_power)
                        s->gain = sqrt(s->desc.signal_target_power/s->last_power);
                    else
                        s->gain = 1.0f;
                    /*endif*/
                }
                /*endif*/
                s->current_energy = 0;
                s->current_samples = 0;
            }
            /*endif*/
        }
        /*endfor*/
    }
    /*endif*/
    if (s->scale_signal)
    {
        for (i = 0;  i < len;  i++)
            out[i] = in[i]*s->gain;
        /*endfor*/
    }
    /*endif*/
    return s->signal_present;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(float) agcf_get_scaling(agcf_state_t *s)
{
    return s->gain;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) agcf_set_scaling(agcf_state_t *s, float scaling)
{
    s->gain = scaling;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(float) agcf_current_power_dbm0(agcf_state_t *s)
{
    return power_ratio_to_db(s->last_power/(32768.0f*32768.0f)) + DBM0_MAX_POWER;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) agcf_set_adaption(agcf_state_t *s, bool adapt)
{
    s->adapt = adapt;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) agcf_get_logging_state(agcf_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

#define power_threshold_dbm0(len,thresh)       (float) (((len)*32768.0f*32768.0f/2.0f)*powf(10.0f, ((thresh) - DBM0_MAX_SINE_POWER)/10.0f))

SPAN_DECLARE(agcf_state_t *) agcf_init(agcf_state_t *s, const agcf_descriptor_t *desc)
{
    if (s == NULL)
    {
        if ((s = (agcf_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
        /*endif*/
    }
    /*endif*/
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "AGC");
    s->desc = *desc;
    s->gain = 1.0f;
    s->adapt = true;
    s->detect = true;
    s->scale_signal = true;

    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) agcf_release(agcf_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) agcf_free(agcf_state_t *s)
{
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
