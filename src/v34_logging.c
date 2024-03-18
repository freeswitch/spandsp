/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v34_logging.c - ITU V.34 modem logging
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

/* THIS IS A WORK IN PROGRESS - NOT YET FUNCTIONAL! */

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
#include "spandsp/fast_convert.h"
#include "spandsp/logging.h"
#include "spandsp/bit_operations.h"
#include "spandsp/bitstream.h"
#include "spandsp/complex.h"
#include "spandsp/vector_float.h"
#include "spandsp/complex_vector_float.h"
#include "spandsp/vector_int.h"
#include "spandsp/complex_vector_int.h"
#include "spandsp/modem_echo.h"
#include "spandsp/async.h"
#include "spandsp/power_meter.h"
#include "spandsp/arctan2.h"
#include "spandsp/dds.h"
#include "spandsp/crc.h"
#include "spandsp/complex_filters.h"

#include "spandsp/v29rx.h"
#include "spandsp/v34.h"

#include "spandsp/private/bitstream.h"
#include "spandsp/private/logging.h"
#include "spandsp/private/power_meter.h"
#include "spandsp/private/modem_echo.h"
#include "spandsp/private/v34.h"

#include "v34_local.h"
#include "v34_superconstellation_map.h"
#include "v34_tables.h"

static const char *trellis_size_code_to_str(int code)
{
    switch (code)
    {
    case V34_TRELLIS_16:
        return "16 state";
    case V34_TRELLIS_32:
        return "32 state";
    case V34_TRELLIS_64:
        return "64 state";
    case V34_TRELLIS_RESERVED:
        return "Reserved for ITU-T";
    }
    /*endswitch*/
    return "???";
}
/*- End of function --------------------------------------------------------*/

void log_info0(logging_state_t *log, bool tx, const v34_capabilities_t *cap, int info0_acknowledgement)
{
    static const char *tx_sources[4] =
    {
        "internal",
        "sync'd to rx",
        "external",
        "reserved for ITU-T"
    };
    int i;

    span_log(log, SPAN_LOG_FLOW, "%s INFO0:\n", (tx)  ?  "Tx"  :  "Rx");
    for (i = 0;  i < 6;  i++)
    {
        span_log(log,
                 SPAN_LOG_FLOW,
                 "  Baud rate %d %s %s\n",
                 baud_rate_parameters[i].baud_rate,
                 (cap->support_baud_rate_low_carrier[i])  ?  "low"  :  "---",
                 (cap->support_baud_rate_low_carrier[i])  ?  "high"  :  "----");
    }
    /*endfor*/
    span_log(log, SPAN_LOG_FLOW, "  3429 baud %sallowed\n", (cap->rate_3429_allowed)  ?  ""  :  "dis");
    span_log(log, SPAN_LOG_FLOW, "  Tx power reduction %ssupported\n", (cap->support_power_reduction)  ?  ""  :  "not ");
    span_log(log, SPAN_LOG_FLOW, "  Max different between Tx and Rx baud rates is %d\n", cap->max_baud_rate_difference);
    span_log(log, SPAN_LOG_FLOW, "  Constellations up to %d supported\n", (cap->support_1664_point_constellation)  ?  1664  :  960);
    span_log(log, SPAN_LOG_FLOW, "  Tx clock source - %s\n", tx_sources[cap->tx_clock_source]);
    span_log(log, SPAN_LOG_FLOW, "  Message %sfrom a CME modem\n", (cap->from_cme_modem)  ?  ""  :  "not ");
    span_log(log, SPAN_LOG_FLOW, "  INFO0 frame %sacknowledged\n", (info0_acknowledgement)  ?  ""  :  "not ");
}
/*- End of function --------------------------------------------------------*/

void log_info1c(logging_state_t *log, bool tx, const info1c_t *info1c)
{
    int i;

    span_log(log, SPAN_LOG_FLOW, "%s INFO1c:\n", (tx)  ?  "Tx"  :  "Rx");
    span_log(log, SPAN_LOG_FLOW, "  Minimum power reduction = %ddB\n", info1c->power_reduction);
    span_log(log, SPAN_LOG_FLOW, "  Additional power reduction = %ddB\n", info1c->additional_power_reduction);
    span_log(log, SPAN_LOG_FLOW, "  Length of MD = %dms\n", info1c->md*35);
    for (i = 0;  i <= 5;  i++)
    {
        span_log(log, SPAN_LOG_FLOW, "  Baud rate %d use %s carrier\n", baud_rate_parameters[i].baud_rate, (info1c->rate_data[i].use_high_carrier)  ?  "high"  :  "low");
        span_log(log, SPAN_LOG_FLOW, "  Baud rate %d pre-emphasis index = %d\n", baud_rate_parameters[i].baud_rate, info1c->rate_data[i].pre_emphasis);
        span_log(log, SPAN_LOG_FLOW, "  Baud rate %d max data rate = %dbps\n", baud_rate_parameters[i].baud_rate, info1c->rate_data[i].max_bit_rate*2400);
    }
    /*endfor*/
    if (info1c->freq_offset == -512)
        span_log(log, SPAN_LOG_FLOW, "  Frequency offset not available\n");
    else
        span_log(log, SPAN_LOG_FLOW, "  Frequency offset = %fHz\n", info1c->freq_offset*0.02f);
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

void log_info1a(logging_state_t *log, bool tx, const info1a_t *info1a)
{
    span_log(log, SPAN_LOG_FLOW, "%s INFO1a:\n", (tx)  ?  "Tx"  :  "Rx");
    span_log(log, SPAN_LOG_FLOW, "  Minimum power reduction = %ddB\n", info1a->power_reduction);
    span_log(log, SPAN_LOG_FLOW, "  Addition power reduction = %ddB\n", info1a->additional_power_reduction);
    span_log(log, SPAN_LOG_FLOW, "  Length of MD = %dms\n", info1a->md*35);
    span_log(log, SPAN_LOG_FLOW, "  %s carrier\n", (info1a->use_high_carrier)  ?  "High"  :  "Low");
    span_log(log, SPAN_LOG_FLOW, "  Pre-emphasis filter = %d\n", info1a->preemphasis_filter);
    span_log(log, SPAN_LOG_FLOW, "  Maximum data rate = %dbps\n", info1a->max_data_rate*2400);
    span_log(log, SPAN_LOG_FLOW, "  Baud rate A->C = %d\n", baud_rate_parameters[info1a->baud_rate_a_to_c].baud_rate);
    span_log(log, SPAN_LOG_FLOW, "  Baud rate C->A = %d\n", baud_rate_parameters[info1a->baud_rate_c_to_a].baud_rate);
    if (info1a->freq_offset == -512)
        span_log(log, SPAN_LOG_FLOW, "  Frequency offset not available\n");
    else
        span_log(log, SPAN_LOG_FLOW, "  Frequency offset = %fHz\n", info1a->freq_offset*0.02f);
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

void log_infoh(logging_state_t *log, bool tx, const infoh_t *infoh)
{
    span_log(log, SPAN_LOG_FLOW, "%s INFO0h:\n", (tx)  ?  "Tx"  :  "Rx");
    span_log(log, SPAN_LOG_FLOW, "  Minimum power reduction = %ddB\n", infoh->power_reduction);
    span_log(log, SPAN_LOG_FLOW, "  Length of TRN = %dms\n", infoh->length_of_trn*35);
    span_log(log, SPAN_LOG_FLOW, "  %s carrier\n", (infoh->use_high_carrier)  ?  "High"  :  "Low");
    span_log(log, SPAN_LOG_FLOW, "  Pre-emphasis filter = %d\n", infoh->preemphasis_filter);
    span_log(log, SPAN_LOG_FLOW, "  Baud rate = %d\n", baud_rate_parameters[infoh->baud_rate].baud_rate);
    span_log(log, SPAN_LOG_FLOW, "  Training constellation = %d state\n", (infoh->trn16)  ?  16  :  4);
}
/*- End of function --------------------------------------------------------*/

void log_mp(logging_state_t *log, bool tx, const mp_t *mp)
{
    int i;

    span_log(log, SPAN_LOG_FLOW, "%s MP:\n", (tx)  ?  "Tx"  :  "Rx");
    span_log(log, SPAN_LOG_FLOW, "  Type = %d\n", mp->type);
    span_log(log, SPAN_LOG_FLOW, "  Max data rate A to C = %dbps\n", mp->bit_rate_a_to_c*2400);
    span_log(log, SPAN_LOG_FLOW, "  Max data rate C to A = %dbps\n", mp->bit_rate_c_to_a*2400);
    span_log(log, SPAN_LOG_FLOW, "  Aux channel supported = %d\n", mp->aux_channel_supported);
    span_log(log, SPAN_LOG_FLOW, "  Trellis size = %s\n", trellis_size_code_to_str(mp->trellis_size));
    span_log(log, SPAN_LOG_FLOW, "  Use non-linear encoder = %d\n", mp->use_non_linear_encoder);
    span_log(log, SPAN_LOG_FLOW, "  Expanded shaping = %d\n", mp->expanded_shaping);
    span_log(log, SPAN_LOG_FLOW, "  MP acknowledged = %d\n", mp->mp_acknowledged);
    span_log(log, SPAN_LOG_FLOW, "  Signalling rate mask = 0x%04X\n", mp->signalling_rate_mask);
    span_log(log, SPAN_LOG_FLOW, "  Asymmetric rates allowed = %d\n",  mp->asymmetric_rates_allowed);
    if (mp->type == 1)
    {
        for (i = 0;  i < 3;  i++)
            span_log(log, SPAN_LOG_FLOW, "  Precoder coeff[%d] = (%d, %d)\n", i, mp->precoder_coeffs[i].re, mp->precoder_coeffs[i].im);
        /*endfor*/
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

void log_mph(logging_state_t *log, bool tx, const mph_t *mph)
{
    int i;

    span_log(log, SPAN_LOG_FLOW, "%s MPh:\n", (tx)  ?  "Tx"  :  "Rx");
    span_log(log, SPAN_LOG_FLOW, "  Type = %d\n", mph->type);
    span_log(log, SPAN_LOG_FLOW, "  Max data rate = %dbps\n", mph->max_data_rate*2400);
    span_log(log, SPAN_LOG_FLOW, "  Control channel data rate = %dbps\n", (mph->control_channel_2400)  ?  2400  :  1200);
    span_log(log, SPAN_LOG_FLOW, "  Trellis size = %s\n", trellis_size_code_to_str(mph->trellis_size));
    span_log(log, SPAN_LOG_FLOW, "  Use non-linear encoder = %d\n", mph->use_non_linear_encoder);
    span_log(log, SPAN_LOG_FLOW, "  Expanded shaping = %d\n", mph->expanded_shaping);
    span_log(log, SPAN_LOG_FLOW, "  Signalling rate mask = 0x%04X\n", mph->signalling_rate_mask);
    span_log(log, SPAN_LOG_FLOW, "  Asymmetric rates allowed = %d\n", mph->asymmetric_rates_allowed);
    if (mph->type == 1)
    {
        for (i = 0;  i < 3;  i++)
            span_log(log, SPAN_LOG_FLOW, "  Precoder coeff[%d] = (%d, %d)\n", i, mph->precoder_coeffs[i].re, mph->precoder_coeffs[i].im);
        /*endfor*/
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) log_parameters(logging_state_t *log, bool tx, v34_parameters_t *parms)
{
    span_log(log, SPAN_LOG_FLOW, "%s V.34 parameters:\n", (tx)  ?  "Tx"  :  "Rx");
    span_log(log, SPAN_LOG_FLOW,
             "  Max bit rate:       %dbps%s\n",
             ((parms->max_bit_rate_code >> 1) + 1)*2400,
             (parms->max_bit_rate_code & 1)  ?   "+ 200bps"  :  "");
    /*! \brief Parameters for the current bit rate and baud rate */
    span_log(log, SPAN_LOG_FLOW, "  Bit rate:           %dbps\n", parms->bit_rate);
    /*! \brief Bits per high mapping frame. A low mapping frame is one bit less. */
    span_log(log, SPAN_LOG_FLOW, "  b:                  %d\n", parms->b);
    span_log(log, SPAN_LOG_FLOW, "  j:                  %d\n", parms->j);
    /*! \brief The number of shell mapped bits */
    span_log(log, SPAN_LOG_FLOW, "  k:                  %d\n", parms->k);
    span_log(log, SPAN_LOG_FLOW, "  l:                  %d points\n", parms->l);
    span_log(log, SPAN_LOG_FLOW, "  m:                  %d\n", parms->m);
    span_log(log, SPAN_LOG_FLOW, "  p:                  %d\n", parms->p);
    /*! \brief The number of uncoded Q bits per 2D symbol */
    span_log(log, SPAN_LOG_FLOW, "  q:                  %d (mask %d)\n", parms->q, parms->q_mask);
    /*! \brief Mapping frame switching parameter */
    span_log(log, SPAN_LOG_FLOW, "  r:                  %d\n", parms->r);
    span_log(log, SPAN_LOG_FLOW, "  w:                  %d\n", parms->w);
    /*! The numerator and dnominator of the number of samples per symbol ratio. */
    span_log(log,
             SPAN_LOG_FLOW,
             "  Samples per symbol: %d/%d\n",
             parms->samples_per_symbol_numerator,
             parms->samples_per_symbol_denominator);
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
