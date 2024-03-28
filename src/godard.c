/*
 * SpanDSP - a series of DSP components for telephony
 *
 * godard.c - Godard symbol timing error detector.
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

/* This symbol sync scheme is based on the technique first described by Dominique Godard in
   Passband Timing Recovery in an All-Digital Modem Receiver
   IEEE TRANSACTIONS ON COMMUNICATIONS, VOL. COM-26, NO. 5, MAY 1978 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#else
#include "spandsp/stdbool.h"
#endif
#include "floating_fudge.h"

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/logging.h"
#include "spandsp/godard.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/godard.h"

#if defined(SPANDSP_USE_FIXED_POINTx)
#define FP_SCALE(x)                     FP_Q6_10(x)
#define FP_SCALE_32(x)                  FP_Q22_10(x)
#define FP_SHIFT_FACTOR                 10
#else
#define FP_SCALE(x)                     (x)
#define FP_SCALE_32(x)                  (x)
#endif

SPAN_DECLARE(int) godard_ted_correction(godard_ted_state_t *s)
{
    return s->total_baud_timing_correction;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(godard_ted_descriptor_t *) godard_ted_make_descriptor(godard_ted_descriptor_t *s,
                                                                   float sample_rate,
                                                                   float baud_rate,
                                                                   float carrier_freq,
                                                                   float alpha,
                                                                   float coarse_trigger,
                                                                   float fine_trigger,
                                                                   int coarse_step,
                                                                   int fine_step)
{
    float low_edge;
    float high_edge;

    if (s == NULL)
    {
        if ((s = (godard_ted_descriptor_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
        /*endif*/
    }
    /*endif*/
    memset(s, 0, sizeof(*s));
    low_edge = 2.0f*M_PI*(carrier_freq - baud_rate/2.0f)/sample_rate;
    high_edge = 2.0f*M_PI*(carrier_freq + baud_rate/2.0f)/sample_rate;

    s->low_band_edge_coeff[0] = FP_SCALE(2.0f*alpha*cosf(low_edge));
    s->low_band_edge_coeff[1] = FP_SCALE(-alpha*alpha);
    s->low_band_edge_coeff[2] = FP_SCALE(-alpha*sinf(low_edge));
    s->high_band_edge_coeff[0] = FP_SCALE(2.0f*alpha*cosf(high_edge));
    s->high_band_edge_coeff[1] = FP_SCALE(-alpha*alpha);
    s->high_band_edge_coeff[2] = FP_SCALE(-alpha*sinf(high_edge));
    s->mixed_band_edges_coeff_3 = FP_SCALE(-alpha*alpha*(sinf(high_edge)*cosf(low_edge) - sinf(low_edge)*cosf(high_edge)));

    s->coarse_trigger = coarse_trigger;
    s->fine_trigger = fine_trigger;
    s->coarse_step = coarse_step;
    s->fine_step = fine_step;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) godard_ted_free_descriptor(godard_ted_descriptor_t *s)
{
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

#if defined(SPANDSP_USE_FIXED_POINTx)
SPAN_DECLARE(void) godard_ted_rx(godard_ted_state_t *s, int16_t sample)
{
    int32_t v;

    /* Symbol timing synchronisation band edge filters */
    /* Low Nyquist band edge filter */
    v = ((s->low_band_edge[0]*s->desc.low_band_edge_coeff[0]) >> FP_SHIFT_FACTOR)
      + ((s->low_band_edge[1]*s->desc.low_band_edge_coeff[1]) >> FP_SHIFT_FACTOR)
      + sample;
    s->symbol_sync_low[1] = s->symbol_sync_low[0];
    s->symbol_sync_low[0] = v;
    /* High Nyquist band edge filter */
    v = ((s->high_band_edge[0]*s->desc.high_band_edge_coeff[0]) >> FP_SHIFT_FACTOR)
      + ((s->high_band_edge[1]*s->desc.high_band_edge_coeff[1] >> FP_SHIFT_FACTOR)
      + sample;
    s->high_band_edge[1] = s->high_band_edge[0];
    s->high_band_edge[0] = v;
}
/*- End of function --------------------------------------------------------*/
#else
SPAN_DECLARE(void) godard_ted_rx(godard_ted_state_t *s, float sample)
{
    float v;

    /* Symbol timing synchronisation band edge filters */
    /* Low Nyquist band edge filter */
    v = s->low_band_edge[0]*s->desc.low_band_edge_coeff[0]
      + s->low_band_edge[1]*s->desc.low_band_edge_coeff[1]
      + sample;
    s->low_band_edge[1] = s->low_band_edge[0];
    s->low_band_edge[0] = v;
    /* High Nyquist band edge filter */
    v = s->high_band_edge[0]*s->desc.high_band_edge_coeff[0]
      + s->high_band_edge[1]*s->desc.high_band_edge_coeff[1]
      + sample;
    s->high_band_edge[1] = s->high_band_edge[0];
    s->high_band_edge[0] = v;
}
/*- End of function --------------------------------------------------------*/
#endif

SPAN_DECLARE(int) godard_ted_per_baud(godard_ted_state_t *s)
{
    int i;
    int eq_put_step_correction;
#if defined(SPANDSP_USE_FIXED_POINTx)
    int32_t v;
    int32_t p;
#else
    float v;
    float p;
#endif

    /* This routine adapts the position of the half baud samples entering the equalizer. */

    /* This is slightly rearranged from figure 3b of the Godard paper, as this saves a couple of
       maths operations */
#if defined(SPANDSP_USE_FIXED_POINTx)
    /* TODO: The scalings used here need more thorough evaluation, to see if overflows are possible. */
    /* Cross correlate */
    v = (((s->low_band_edge[1] >> (FP_SHIFT_FACTOR/2))*(s->high_band_edge[0] >> (FP_SHIFT_FACTOR/2))) >> 14)*s->desc.low_band_edge_coeff[2]
      - (((s->low_band_edge[0] >> (FP_SHIFT_FACTOR/2))*(s->high_band_edge[1] >> (FP_SHIFT_FACTOR/2))) >> 14)*s->desc.high_band_edge_coeff[2]
      + (((s->low_band_edge[1] >> (FP_SHIFT_FACTOR/2))*(s->high_band_edge[1] >> (FP_SHIFT_FACTOR/2))) >> 14)*s->desc.mixed_band_edges_coeff_3;
    /* Filter away any DC component */
    p = v - s->symbol_sync_dc_filter[1];
    s->symbol_sync_dc_filter[1] = s->symbol_sync_dc_filter[0];
    s->symbol_sync_dc_filter[0] = v;
    /* A little integration will now filter away much of the HF noise */
    s->baud_phase -= p;
    v = abs(s->baud_phase);
#else
    /* Cross correlate */
    v = s->low_band_edge[1]*s->high_band_edge[0]*s->desc.low_band_edge_coeff[2]
      - s->low_band_edge[0]*s->high_band_edge[1]*s->desc.high_band_edge_coeff[2]
      + s->low_band_edge[1]*s->high_band_edge[1]*s->desc.mixed_band_edges_coeff_3;
    /* Filter away any DC component */
    p = v - s->dc_filter[1];
    s->dc_filter[1] = s->dc_filter[0];
    s->dc_filter[0] = v;
    /* A little integration will now filter away much of the HF noise */
    s->baud_phase -= p;
    v = fabsf(s->baud_phase);
#endif
    eq_put_step_correction = 0;
    if (v > s->desc.fine_trigger)
    {
        i = (v > s->desc.coarse_trigger)  ?  s->desc.coarse_step  :  s->desc.fine_step;
        if (s->baud_phase < FP_SCALE_32(0.0f))
            i = -i;
        /*endif*/
        //printf("v = %10.5f %5d - %f %f %d\n", v, i, p, s->baud_phase, s->total_baud_timing_correction);
        eq_put_step_correction = i;
        s->total_baud_timing_correction += i;
    }
    /*endif*/
    return eq_put_step_correction;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(godard_ted_state_t *) godard_ted_init(godard_ted_state_t *s, const godard_ted_descriptor_t *desc)
{
    if (s == NULL)
    {
        if ((s = (godard_ted_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
        /*endif*/
    }
    /*endif*/
    memset(s, 0, sizeof(*s));
    s->desc = *desc;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) godard_ted_release(godard_ted_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) godard_ted_free(godard_ted_state_t *s)
{
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
