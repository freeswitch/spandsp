/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v80.c - In band DCE control and synchronous data modes for asynchronous DTE
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2023 Steve Underwood
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
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
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
#include "spandsp/logging.h"
#include "spandsp/v80.h"

#include "spandsp/private/logging.h"
//#include "spandsp/private/v80.h"

SPAN_DECLARE(const char *) v80_escape_to_str(int esc)
{
    static const char *escapes[] =
    {
        "MFGExtend",
        "Mfg1",
        "Mfg2",
        "Mfg3",
        "Mfg4",
        "Mfg5",
        "Mfg6",
        "Mfg7",
        "Mfg8",
        "Mfg9",
        "Mfg10",
        "Mfg11",
        "Mfg12",
        "Mfg13",
        "Mfg14",
        "Mfg15",
        "ExtendMfg",
        "Mfg1",
        "Mfg2",
        "Mfg3",
        "Mfg4",
        "Mfg5",
        "Mfg6",
        "Mfg7",
        "Mfg8",
        "Mfg9",
        "Mfg10",
        "Mfg11",
        "Mfg12",
        "Mfg13",
        "Mfg14",
        "Mfg15",
        "Extend0",
        "Extend1",
        "Circuit 105 OFF",
        "Circuit 105 ON",
        "Circuit 108 OFF",
        "Circuit 108 ON",
        "Circuit 133 OFF",
        "Circuit 133 ON",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "Single EM P",
        "Dingle EM P",
        "Flow OFF",
        "Flow ON",
        "Single EM",
        "Double EM",
        "Poll",
        "???",
        "Extend0",
        "Extend1",
        "Circuit 106 OFF",
        "Circuit 106 ON",
        "Circuit 107 OFF",
        "Circuit 107 ON",
        "Circuit 109 OFF",
        "Circuit 109 ON",
        "Circuit 110 OFF",
        "Circuit 110 ON",
        "Circuit 125 OFF",
        "Circuit 125 ON",
        "Circuit 132 OFF",
        "Circuit 132 ON",
        "Circuit 142 OFF",
        "Circuit 142 ON",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "Single EM P",
        "Double EM P",
        "OFF line",
        "ON line",
        "Flow OFF",
        "Flow ON",
        "Single EM",
        "Double EM",
        "Poll",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "???",
        "T3",
        "T4",
        "T7",
        "T8",
        "T9",
        "T10",
        "T11",
        "T12",
        "T13",
        "T14",
        "T15",
        "T16",
        "T17",
        "T18",
        "T19",
        "T20",
        "Mark",
        "Flag",
        "Err",
        "Hunt",
        "Under",
        "TOver",
        "ROver",
        "Resume",
        "BNum",
        "UNum",
        "EOTH",
        "ECS",
        "RRNH",
        "RTNH",
        "RateH",
        "CTL",
        "RTNC"
    };

    if (esc >= 0x20  &&  esc <= 0xC0)
        return escapes[esc - 0x20];
    /*endif*/
    return "???";
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v80_bit_rate_code_to_bit_rate(int rate_code)
{
    static const int rates[] =
    {
         1200,
         2400,
         4800,
         7200,
         9600,
        12000,
        16800,
        19200,
        21600,
        24000,
        26400,
        28800,
        31200,
        33600,
        32000,
        56000,
        64000
    };

    if (rate_code >= V80_BIT_RATE_1200  &&  rate_code <= V80_BIT_RATE_64000)
        return rates[rate_code - V80_BIT_RATE_1200];
    /*endif*/
    return -1;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
