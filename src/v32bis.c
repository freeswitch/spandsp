/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v32bis.c - ITU V.32bis modem
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

/*! \file */

/* V.32bis SUPPORT IS A WORK IN PROGRESS - NOT YET FUNCTIONAL! */

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
#include "spandsp/complex.h"
#include "spandsp/vector_float.h"
#include "spandsp/complex_vector_float.h"
#include "spandsp/async.h"
#include "spandsp/power_meter.h"
#include "spandsp/arctan2.h"
#include "spandsp/dds.h"
#include "spandsp/complex_filters.h"

#include "spandsp/modem_echo.h"
#include "spandsp/v29rx.h"
#include "spandsp/v17tx.h"
#include "spandsp/v17rx.h"
#include "spandsp/v32bis.h"

#include "spandsp/v17tx.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/power_meter.h"
#include "spandsp/private/v17tx.h"
#include "spandsp/private/v17rx.h"
#include "spandsp/private/v32bis.h"

#if defined(SPANDSP_USE_FIXED_POINTx)
#define FP_SCALE(x)     ((int16_t) x)
#else
#define FP_SCALE(x)     (x)
#endif

#define FP_CONSTELLATION_SCALE(x)       FP_SCALE(x)

#include "v17_v32bis_tx_constellation_maps.h"
#include "v17_v32bis_rx_constellation_maps.h"
#include "v17_v32bis_tx_rrc.h"
#include "v17_v32bis_rx_rrc.h"

#if defined(SPANDSP_USE_FIXED_POINT)
SPAN_DECLARE(int) v32bis_equalizer_state(v32bis_state_t *s, complexi16_t **coeffs)
#else
SPAN_DECLARE(int) v32bis_equalizer_state(v32bis_state_t *s, complexf_t **coeffs)
#endif
{
    return v17_rx_equalizer_state(&s->rx, coeffs);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(float) v32bis_rx_carrier_frequency(v32bis_state_t *s)
{
    return v17_rx_carrier_frequency(&s->rx);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(float) v32bis_rx_symbol_timing_correction(v32bis_state_t *s)
{
    return v17_rx_symbol_timing_correction(&s->rx);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(float) v32bis_rx_signal_power(v32bis_state_t *s)
{
    return v17_rx_signal_power(&s->rx);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v32bis_tx(v32bis_state_t *s, int16_t amp[], int len)
{
    return v17_tx(&s->tx, amp, len);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v32bis_rx(v32bis_state_t *s, const int16_t amp[], int len)
{
    return v17_rx(&s->rx, amp, len);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v32bis_rx_fillin(v32bis_state_t *s, int len)
{
    return v17_rx_fillin(&s->rx, len);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v32bis_tx_power(v32bis_state_t *s, float power)
{
    v17_tx_power(&s->tx, power);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v32bis_set_get_bit(v32bis_state_t *s, span_get_bit_func_t get_bit, void *user_data)
{
    v17_tx_set_get_bit(&s->tx, get_bit, user_data);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v32bis_set_put_bit(v32bis_state_t *s, span_put_bit_func_t put_bit, void *user_data)
{
    v17_rx_set_put_bit(&s->rx, put_bit, user_data);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v32bis_set_supported_bit_rates(v32bis_state_t *s, int rates)
{
    s->permitted_rates_signal = (rates & 0x1660) | 0x8990;
    //Rate signal sync test is (value & 0x888F) == 0x8880
    //E signal sync test is (value & 0x888F) == 0x888F
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v32bis_current_bit_rate(v32bis_state_t *s)
{
    return 14400;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) v32bis_get_logging_state(v32bis_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v32bis_restart(v32bis_state_t *s, int bit_rate)
{
    span_log(&s->rx.logging, SPAN_LOG_FLOW, "Restarting V.32bis, %dbps\n", bit_rate);
    v17_tx_restart(&s->tx, bit_rate, false, false);
    v17_rx_restart(&s->rx, bit_rate, false);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(v32bis_state_t *) v32bis_init(v32bis_state_t *s,
                                           int bit_rate,
                                           bool calling_party,
                                           span_get_bit_func_t get_bit,
                                           void *get_bit_user_data,
                                           span_put_bit_func_t put_bit,
                                           void *put_bit_user_data)
{
    if (s == NULL)
    {
        if ((s = (v32bis_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "V.32bis");
    s->bit_rate = bit_rate;
    s->calling_party = calling_party;

    /* V.32bis never uses TEP */
    v17_tx_init(&s->tx, bit_rate, false, get_bit, get_bit_user_data);
    v17_rx_init(&s->rx, bit_rate, put_bit, put_bit_user_data);
    s->ec = modem_echo_can_init(256);

    /* Initialise things which are not quite like V.17 */
    if (s->calling_party)
    {
        s->tx.scrambler_tap = 17;
        s->rx.scrambler_tap = 4;
    }
    else
    {
        s->tx.scrambler_tap = 4;
        s->rx.scrambler_tap = 17;
    }
    v32bis_set_supported_bit_rates(s,
                                   V32BIS_RATE_14400
                                 | V32BIS_RATE_12000
                                 | V32BIS_RATE_9600
                                 | V32BIS_RATE_7200
                                 | V32BIS_RATE_4800);
    v32bis_restart(s, bit_rate);
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v32bis_release(v32bis_state_t *s)
{
    modem_echo_can_free(s->ec);
    s->ec = NULL;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v32bis_free(v32bis_state_t *s)
{
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v32bis_set_qam_report_handler(v32bis_state_t *s, qam_report_handler_t handler, void *user_data)
{
    s->rx.qam_report = handler;
    s->rx.qam_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
