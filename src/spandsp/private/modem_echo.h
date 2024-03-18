/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/modem_echo.h - An echo cancellor, suitable for electrical echos in GSTN modems
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001, 2004 Steve Underwood
 *
 * Based on a bit from here, a bit from there, eye of toad,
 * ear of bat, etc - plus, of course, my own 2 cents.
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

#if !defined(_SPANDSP_PRIVATE_MODEM_ECHO_H_)
#define _SPANDSP_PRIVATE_MODEM_ECHO_H_

/*!
    Modem line echo canceller descriptor. This defines the working state for an
    echo canceller for a PSTN dial up modem. i.e a sparse canceller, canceller,
    which deals with two small periods of echo, over two analogue line segments,
    some substantial echoless delay between them, and some buffering delays for
    each end's modem processing.
*/
struct modem_echo_can_segment_state_s
{
    int adapt;
    int taps;

    int ec_len;

    fir16_state_t fir_state;
    /*! Echo FIR taps (16 bit filtering version) */
    int16_t *fir_taps16;
    /*! Echo FIR taps (32 bit adapting version) */
    int32_t *fir_taps32;

    int32_t adaption_rate;

    int32_t tx_power;
    int32_t rx_power;

    int curr_pos;
};

struct modem_echo_can_state_s
{
    int16_t *local_delay;
    int local_delay_len;
    struct modem_echo_can_segment_state_s near_ec;
    int16_t *bulk_delay;
    int bulk_delay_len;
    struct modem_echo_can_segment_state_s far_ec;
    int16_t *far_delay;
    int far_delay_len;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

#endif
/*- End of file ------------------------------------------------------------*/
