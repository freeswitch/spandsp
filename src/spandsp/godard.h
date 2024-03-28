/*
 * SpanDSP - a series of DSP components for telephony
 *
 * godard.h - Godard symbol timing error detector.
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

#if !defined(_SPANDSP_GODARD_H_)
#define _SPANDSP_GODARD_H_

typedef struct godard_ted_state_s godard_ted_state_t;

typedef struct godard_ted_descriptor_s
{
#if defined(SPANDSP_USE_FIXED_POINTx)
    /*! Low band edge filter coefficients */
    int32_t low_band_edge_coeff[3];
    /*! High band edge filter coefficients */
    int32_t high_band_edge_coeff[3];
    /*! The blended filter coefficient */
    int32_t mixed_edges_coeff_3;
    
    /*! Error needed to cause a coarse step in the baud alignment */
    int32_t coarse_trigger;
    /*! Error needed to cause a fine step in the baud alignment */
    int32_t fine_trigger;
#else
    /*! Low band edge filter coefficients */
    float low_band_edge_coeff[3];
    /*! High band edge filter coefficients */
    float high_band_edge_coeff[3];
    /*! The blended filter coefficient */
    float mixed_band_edges_coeff_3;

    /*! Error needed to cause a coarse step in the baud alignment */
    float coarse_trigger;
    /*! Error needed to cause a fine step in the baud alignment */
    float fine_trigger;
#endif
    /*! The size of a coarse step in the baud alignment. This is used to rapidly
        pull in the alignment during symbol acquisition. We need to switch to
        finer steps as we pull in the alignment, or the equalizer will not
        adapt well. */
    int coarse_step;
    /*! The size of a fine step in the baud alignment. This is used to track
        smaller amounts of misalignment once we are roughly on the symbols. */
    int fine_step;
} godard_ted_descriptor_t;

#if defined(__cplusplus)
extern "C"
{
#endif

SPAN_DECLARE(godard_ted_descriptor_t *) godard_ted_make_descriptor(godard_ted_descriptor_t *desc,
                                                                   float sample_rate,
                                                                   float baud_rate,
                                                                   float carrier_freq,
                                                                   float alpha,
                                                                   float coarse_trigger,
                                                                   float fine_trigger,
                                                                   int coarse_step,
                                                                   int fine_step);

SPAN_DECLARE(int) godard_ted_free_descriptor(godard_ted_descriptor_t *s);

SPAN_DECLARE(int) godard_ted_correction(godard_ted_state_t *s);

#if defined(SPANDSP_USE_FIXED_POINTx)
SPAN_DECLARE(void) godard_ted_rx(godard_ted_state_t *s, int16_t sample);
#else
SPAN_DECLARE(void) godard_ted_rx(godard_ted_state_t *s, float sample);
#endif

SPAN_DECLARE(int) godard_ted_per_baud(godard_ted_state_t *s);

SPAN_DECLARE(godard_ted_state_t *) godard_ted_init(godard_ted_state_t *s, const godard_ted_descriptor_t *desc);

SPAN_DECLARE(int) godard_ted_release(godard_ted_state_t *s);

SPAN_DECLARE(int) godard_ted_free(godard_ted_state_t *s);

#if defined(__cplusplus)
}
#endif
#endif
/*- End of file ------------------------------------------------------------*/
