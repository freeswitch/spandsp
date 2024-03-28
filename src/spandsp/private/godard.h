/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/godard.h - Godard symbol timing error detector.
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

#if !defined(_SPANDSP_PRIVATE_GODARD_H_)
#define _SPANDSP_PRIVATE_GODARD_H_

struct godard_ted_state_s
{
    godard_ted_descriptor_t desc;
#if defined(SPANDSP_USE_FIXED_POINTx)
    /*! Low band edge filter for symbol sync. */
    int32_t low_band_edge[2];
    /*! High band edge filter for symbol sync. */
    int32_t high_band_edge[2];
    /*! DC filter for symbol sync. */
    int32_t dc_filter[2];
    /*! Baud phase for symbol sync. */
    int32_t baud_phase;
#else
    /*! Low band edge filter for symbol sync. */
    float low_band_edge[2];
    /*! High band edge filter for symbol sync. */
    float high_band_edge[2];
    /*! DC filter for symbol sync. */
    float dc_filter[2];
    /*! Baud phase for symbol sync. */
    float baud_phase;
#endif
    /*! \brief The total symbol timing correction since the carrier came up.
               This is only for performance analysis purposes. */
    int total_baud_timing_correction;
};

#endif
/*- End of file ------------------------------------------------------------*/
