/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/agcf.h - Floating point automatic gain control for modems.
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

#if !defined(_SPANDSP_PRIVATE_AGCF_H_)
#define _SPANDSP_PRIVATE_AGCF_H_

struct agcf_state_s
{
    agcf_descriptor_t desc;

    /* Used for DC blocking */
    float dc_block_x;
    float dc_block_y;

    float gain;

    float current_energy;
    int current_samples;
    float last_power;

    int8_t signal_on_persistence;
    int8_t signal_off_persistence;

    /* True if the AGC should be adapting */
    bool adapt;
    /* True if the AGC should be detecting a signal */
    bool detect;
    bool scale_signal;
    bool signal_present;

    /*! */
    logging_state_t logging;
};

#endif
/*- End of file ------------------------------------------------------------*/
