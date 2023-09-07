/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/v32bis.h - ITU V.32bis modem
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2008 Steve Underwood
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

/* V.32bis SUPPORT IS A WORK IN PROGRESS - NOT YET FUNCTIONAL! */

#if !defined(_SPANDSP_PRIVATE_V32BIS_H_)
#define _SPANDSP_PRIVATE_V32BIS_H_

extern const complexf_t v32bis_constellation[16];

/*!
    V.32bis modem descriptor. This defines the working state for a single instance
    of a V.32bis modem.
*/
struct v32bis_state_s
{
    /*! \brief The bit rate of the modem. Valid values are 1200 and 2400. */
    int bit_rate;
    /*! \brief True is this is the calling side modem. */
    bool calling_party;

    v17_rx_state_t rx;
    v17_tx_state_t tx;
    modem_echo_can_state_t *ec;

    uint16_t permitted_rates_signal;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

#endif
/*- End of file ------------------------------------------------------------*/
