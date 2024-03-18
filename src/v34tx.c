/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v34tx.c - ITU V.34 modem, transmit part
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

#include "v22bis_tx_rrc.h"

#include "v34_tx_2400_rrc.h"
#include "v34_tx_2743_rrc.h"
#include "v34_tx_2800_rrc.h"
#include "v34_tx_3000_rrc.h"
#include "v34_tx_3200_rrc.h"
#include "v34_tx_3429_rrc.h"

#include "v34_local.h"
#include "v34_tables.h"
#include "v34_superconstellation_map.h"
#include "v34_convolutional_coders.h"
#include "v34_probe_signals.h"
#include "v34_shell_map.h"
#include "v34_tx_pre_emphasis_filters.h"

#if defined(SPANDSP_USE_FIXED_POINT)
#define complex_sig_set(re,im) complex_seti16(re,im)
#define complex_sig_t complexi16_t
#else
#define complex_sig_set(re,im) complex_setf(re,im)
#define complex_sig_t complexf_t
#endif

#define FP_Q9_7_TO_F(x)                 ((float) x/128.0f)

#define EQUALIZER_DELTA                 0.21f
#define EQUALIZER_SLOW_ADAPT_RATIO      0.1f

#define V34_TRAINING_SEG_1              0
#define V34_TRAINING_SEG_4              0
#define V34_TRAINING_END                0
#define V34_TRAINING_SHUTDOWN_END       0

#define INFO_FILL_AND_SYNC_BITS         0x4EF

#if defined(SPANDSP_USE_FIXED_POINT)
#define TRAINING_SCALE(x)               ((int16_t) (32767.0f*x + ((x >= 0.0)  ?  0.5  :  -0.5)))
#else
#define TRAINING_SCALE(x)               (x)
#endif

#define TRAINING_AMP                    10.0f

enum
{
    TRAINING_TX_STAGE_NORMAL_OPERATION_V34 = 0,
    TRAINING_TX_STAGE_NORMAL_OPERATION_CC = 1,
    TRAINING_TX_STAGE_PARKED
};

#if defined(SPANDSP_USE_FIXED_POINT)
typedef int16_t tx_shaper_t[V34_TX_FILTER_STEPS];
#else
typedef float tx_shaper_t[V34_TX_FILTER_STEPS];
#endif

static const tx_shaper_t *v34_tx_shapers[] =
{
    tx_pulseshaper_2400,
    tx_pulseshaper_2743,
    tx_pulseshaper_2800,
    tx_pulseshaper_3000,
    tx_pulseshaper_3200,
    tx_pulseshaper_3429
};

static const complex_sig_t zero = {TRAINING_SCALE(0.0f), TRAINING_SCALE(0.0f)};

static const complex_sig_t training_constellation_4[4] =
{
    {TRAINING_SCALE(-0.7071068f*TRAINING_AMP), TRAINING_SCALE(-0.7071068f*TRAINING_AMP)},   /* 225 degrees */
    {TRAINING_SCALE(-0.7071068f*TRAINING_AMP), TRAINING_SCALE( 0.7071068f*TRAINING_AMP)},   /* 135 degrees */
    {TRAINING_SCALE( 0.7071068f*TRAINING_AMP), TRAINING_SCALE( 0.7071068f*TRAINING_AMP)},   /*  45 degrees */
    {TRAINING_SCALE( 0.7071068f*TRAINING_AMP), TRAINING_SCALE(-0.7071068f*TRAINING_AMP)}    /* 315 degrees */
};

static const complex_sig_t training_constellation_16[16] =
{
    {TRAINING_SCALE(-1.0f*TRAINING_AMP), TRAINING_SCALE(-1.0f*TRAINING_AMP)},
    {TRAINING_SCALE(-1.0f*TRAINING_AMP), TRAINING_SCALE( 1.0f*TRAINING_AMP)},
    {TRAINING_SCALE( 1.0f*TRAINING_AMP), TRAINING_SCALE( 1.0f*TRAINING_AMP)},
    {TRAINING_SCALE( 1.0f*TRAINING_AMP), TRAINING_SCALE(-1.0f*TRAINING_AMP)},

    {TRAINING_SCALE( 3.0f*TRAINING_AMP), TRAINING_SCALE(-1.0f*TRAINING_AMP)},
    {TRAINING_SCALE(-1.0f*TRAINING_AMP), TRAINING_SCALE(-3.0f*TRAINING_AMP)},
    {TRAINING_SCALE(-3.0f*TRAINING_AMP), TRAINING_SCALE( 1.0f*TRAINING_AMP)},
    {TRAINING_SCALE( 1.0f*TRAINING_AMP), TRAINING_SCALE( 3.0f*TRAINING_AMP)},

    {TRAINING_SCALE(-1.0f*TRAINING_AMP), TRAINING_SCALE( 3.0f*TRAINING_AMP)},
    {TRAINING_SCALE( 3.0f*TRAINING_AMP), TRAINING_SCALE( 1.0f*TRAINING_AMP)},
    {TRAINING_SCALE( 1.0f*TRAINING_AMP), TRAINING_SCALE(-3.0f*TRAINING_AMP)},
    {TRAINING_SCALE(-3.0f*TRAINING_AMP), TRAINING_SCALE(-1.0f*TRAINING_AMP)},

    {TRAINING_SCALE( 3.0f*TRAINING_AMP), TRAINING_SCALE( 3.0f*TRAINING_AMP)},
    {TRAINING_SCALE( 3.0f*TRAINING_AMP), TRAINING_SCALE(-3.0f*TRAINING_AMP)},
    {TRAINING_SCALE(-3.0f*TRAINING_AMP), TRAINING_SCALE(-3.0f*TRAINING_AMP)},
    {TRAINING_SCALE(-3.0f*TRAINING_AMP), TRAINING_SCALE( 3.0f*TRAINING_AMP)}
};

/*
DUPLEX OPERATION
----------------

Duplex caller
-------------
V.8 sequence   | INFO0c |   B   |!B|                   |   B   |!B|L1|   L2   | INFO1c |                                        |S|!S|  MD  |S|!S| PP | TRN |  J  |J'|  TRN |MP |MP |MP'|MP'|E| B1 | Data
---------------|XXXXXXXX|XXXXXXX|XX|-------------------|XXXXXXX|XX|XX|XXXXXXXX|XXXXXXXX|----------------------------------------|X|XX|XXXXXX|X|XX|XXXX|XXXXX|XXXXX|XX|XXXXXX|XXX|XXX|XXX|XXX|X|XXXX|XXXXXXXXXXXXX

Duplex answerer
---------------
V.8 sequence    | INFO0a  | A |  !A  | A |L1|   L2   | A |!A|               |     A    | INFO1a |  | S |!S| MD | S |!S| PP | TRN | J |                       | S |!S|  TRN |MP |MP |MP'|MP'|E| B1 |   Data
----------------|XXXXXXXXX|XXX|XXXXXX|XXX|XX|XXXXXXXX|XXX|XX|---------------|XXXXXXXXXX|XXXXXXXX|--|XXX|XX|XXXX|XXX|XX|XXXX|XXXXX|XXX|-----------------------|XXX|XX|XXXXXX|XXX|XXX|XXX|XXX|X|XXXX|XXXXXXXXXXXXXXX

J       Repetitions of 0x8990 for a 4 point constellation, or 0x89B0 for a 16 point constellation.
J'      A single transmission of 0x899F.
MD      Manufacturer specific training sequence
PP      Preliminary training sequence. 8 symbols, repeated 4 times.
S       90 degree alternations
!S      180 degree shift from S
TRN     Training sequence 4 or 16 symbols scrambled by the scrambler
MP      Modulation parameter sequence
MP'     Modulation parameter sequence with the acknowledgement bit set
A       1200Hz
!A      Phase reversed 1200Hz
B       2400Hz
!B      Phase reversed 2400Hz
ALT
E
*/

/*
HALF-DUPLEX OPERATION
---------------------

Half duplex caller, when caller is source
-----------------------------------------
V.8 sequence  silence  INFO0c  B  !B  L1  L2  B  silence  S  !S  PP  TRN  silence

Half duplex answerer, when caller is source
-------------------------------------------
V.8 sequence silence INFO0a  A  !A  silence  A  INFOh  silence

Half duplex caller, when answerer is source
-------------------------------------------
V.8 sequence  silence  INFO0c  B  !B  silence  B INFOh  silence

Half duplex answerer, when answerer is source
---------------------------------------------
V.8 sequence silence INFO0a  A  !A  L1  L2  A  silence  S  !S  PP  TRN  silence


High speed training sequences:

Caller
------
S  !S  MD  S  !S  PP  TRN  J  J'  TRN  MP  MP'  E  B1  Data

Answer
------
S  !S  MD  S  !S  PP  TRN  J  [wait]  S  !S  TRN  MP  MP'  E  B1  Data

Control channel training sequences:

PPh  ALT  MPh  MPh  E  Data

or

Sh  !Sh  ALT  PPh  ALT  MPh  MPh  E  Data


J       Repetitions of 0x8990 for a 4 point constellation, or 0x89B0 for a 16 point constellation.
J'      A single transmission of 0x899F.
MD      Manufacturer specific training sequence
PP      Preliminary training sequence. 8 symbols, repeated 4 times.
S       90 degree alternations
!S      180 degree shift from S
Sh      90 degree alternations
!Sh     180 degree shift from S
TRN     Training sequence 4 or 16 symbols scrambled by the scrambler
MP      Modulation parameter sequence
MP'     Modulation parameter sequence with the acknowledgement bit set
MPh     Modulation parameter sequence
A       1200Hz
!A      Phase reversed 1200Hz
B       2400Hz
!B      Phase reversed 2400Hz
ALT
E
*/

/*
Power is -16.328760dBm0 V.8

Power is -16.416633dBm0 info0
Power is -16.359079dBm0 tone
Power is -11.280243dBm0 L1
Power is -17.304279dBm0 L2
Power is -16.403152dBm0 tone


Power is -18.137333dBm0 S


Power is -17.194252dBm0 CC

Phase 1 is all nominal

A is 2400Hz 1dB below nominal + guard at nominal

B is 1200Hz at nominal

INFO is 2400Hz at 1dB below nominal + guard 7dB below nominal

INFO is 1200Hz at nominal

L1 is 6dB above nominal

L2 is nominal

CC is 2400Hz at 1dB below nominal + guard 7dB below nominal

CC is 1200Hz at nominal
*/

/*
    Framing terminology:
        2 symbols makes a 4D symbol (k = 0, 1)
        4 4D symbols makes a mapping frame (j = 0, 1, 2, 3)
        P mapping frames makes a data frame (35 or 40ms) (P = 12, 14, 15 or 16)
        J data frames makes a super frame (280ms) (J = 7 or 8)
*/

static void tx_silence_init(v34_state_t *s, int duration);
static void transmission_preamble_init(v34_state_t *s);
static void info0_baud_init(v34_state_t *s);
static void initial_ab_not_ab_baud_init(v34_state_t *s);
static void l1_l2_signal_init(v34_state_t *s);
static void second_a_baud_init(v34_state_t *s);
static void second_b_baud_init(v34_state_t *s);
static void info1_baud_init(v34_state_t *s);
static void infoh_baud_init(v34_state_t *s);
static void s_not_s_baud_init(v34_state_t *s);
static void pp_baud_init(v34_state_t *s);
static void trn_baud_init(v34_state_t *s);
static void mp_or_mph_baud_init(v34_state_t *s);
static void e_baud_init(v34_state_t *s);

/* Control channel startup routines */
static void pph_baud_init(v34_state_t *s);
static void first_alt_baud_init(v34_state_t *s);
static void second_alt_baud_init(v34_state_t *s);
static void sh_baud_init(v34_state_t *s);

static __inline__ int scramble(v34_tx_state_t *s, int in_bit)
{
    int out_bit;

    /* One of the scrambler taps is a variable, so it can be adjusted for caller or answerer operation. */
    out_bit = (in_bit ^ (s->scramble_reg >> s->scrambler_tap) ^ (s->scramble_reg >> (23 - 1))) & 1;
    s->scramble_reg = (s->scramble_reg << 1) | out_bit;
    return out_bit;
}
/*- End of function --------------------------------------------------------*/

static uint16_t crc_bit_block(const uint8_t buf[], int first_bit, int last_bit, uint16_t crc)
{
    int pre;
    int post;

    /* Calculate the CRC between first_bit and last_bit, inclusive, of buf */
    last_bit++;
    pre = first_bit & 0x7;
    first_bit >>= 3;
    if (pre)
    {
        crc = crc_itu16_bits(buf[first_bit] >> pre, (8 - pre), crc);
        first_bit++;
    }
    /*endif*/
    post = last_bit & 0x7;
    last_bit >>= 3;
    if ((last_bit - first_bit) != 0)
        crc = crc_itu16_calc(buf + first_bit, last_bit - first_bit, crc);
    /*endif*/
    if (post)
        crc = crc_itu16_bits(buf[last_bit], post, crc);
    /*endif*/
    return crc;
}
/*- End of function --------------------------------------------------------*/

static int info0_sequence_tx(v34_tx_state_t *s)
{
    uint8_t *t;
    uint16_t crc;
    bitstream_state_t bs;

    log_info0(s->logging, true, &v34_capabilities, s->info0_acknowledgement);
    bitstream_init(&bs, true);
    t = s->txbuf;
    /* 0:3      Fill bits: 1111. */
    /* 4:11     Frame sync: 01110010, where the left-most bit is first in time. */
    bitstream_put(&bs, &t, INFO_FILL_AND_SYNC_BITS, 12);
    /* 12       Set to 1 indicates symbol rate 2743 is supported. */
    bitstream_put(&bs, &t, (v34_capabilities.support_baud_rate_low_carrier[V34_BAUD_RATE_2743])  ?  1  :  0, 1);
    /* 13       Set to 1 indicates symbol rate 2800 is supported. */
    bitstream_put(&bs, &t, (v34_capabilities.support_baud_rate_low_carrier[V34_BAUD_RATE_2800])  ?  1  :  0, 1);
    /* 14       Set to 1 indicates symbol rate 3429 is supported. */
    bitstream_put(&bs, &t, (v34_capabilities.support_baud_rate_low_carrier[V34_BAUD_RATE_3429])  ?  1  :  0, 1);
    /* 15       Set to 1 indicates the ability to transmit at the low carrier frequency with a symbol rate of 3000. */
    bitstream_put(&bs, &t, (v34_capabilities.support_baud_rate_low_carrier[V34_BAUD_RATE_3000])  ?  1  :  0, 1);
    /* 16       Set to 1 indicates the ability to transmit at the high carrier frequency with a symbol rate of 3000. */
    bitstream_put(&bs, &t, (v34_capabilities.support_baud_rate_high_carrier[V34_BAUD_RATE_3000])  ?  1  :  0, 1);
    /* 17       Set to 1 indicates the ability to transmit at the low carrier frequency with a symbol rate of 3200. */
    bitstream_put(&bs, &t, (v34_capabilities.support_baud_rate_low_carrier[V34_BAUD_RATE_3200])  ?  1  :  0, 1);
    /* 18       Set to 1 indicates the ability to transmit at the high carrier frequency with a symbol rate of 3200. */
    bitstream_put(&bs, &t, (v34_capabilities.support_baud_rate_high_carrier[V34_BAUD_RATE_3200])  ?  1  :  0, 1);
    /* 19       Set to 0 indicates that transmission with a symbol rate of 3429 is disallowed. */
    bitstream_put(&bs, &t, (v34_capabilities.rate_3429_allowed)  ?  1  :  0, 1);
    /* 20       Set to 1 indicates the ability to reduce transmit power to a value lower than the nominal setting. */
    bitstream_put(&bs, &t, (v34_capabilities.support_power_reduction)  ?  1  :  0, 1);
    /* 21:23    Maximum allowed difference in symbol rates in the transmit and receive directions. With the symbol rates
                labelled in increasing order, where 0 represents 2400 and 5 represents 3429, an integer between 0 and 5
                indicates the difference allowed in number of symbol rate steps. */
    bitstream_put(&bs, &t, v34_capabilities.max_baud_rate_difference, 3);
    /* 24       Set to 1 in an INFO0 sequence transmitted from a CME modem. */
    bitstream_put(&bs, &t, v34_capabilities.from_cme_modem, 1);
    /* 25       Set to 1 indicates the ability to support up to 1664-point signal constellations. */
    bitstream_put(&bs, &t, (v34_capabilities.support_1664_point_constellation)  ?  1  :  0, 1);
    /* 26:27    Transmit clock source: 0 = internal; 1 = synchronized to receive timing; 2 = external; 3 = reserved for ITU-T. */
    bitstream_put(&bs, &t, v34_capabilities.tx_clock_source, 2);
    /* 28       Set to 1 to acknowledge correct reception of an INFO0 frame during error recovery. */
    bitstream_put(&bs, &t, s->info0_acknowledgement, 1);
    bitstream_emit(&bs, &t);
    crc = crc_bit_block(s->txbuf, 12, 28, 0xFFFF);
    /* 29:44    CRC. */
    bitstream_put(&bs, &t, crc, 16);
    /* 45:48    Fill bits: 1111. */
    bitstream_put(&bs, &t, 0xF, 4);
    /* Add some extra postamble, so we have a whole number of bytes to work with. */
    bitstream_put(&bs, &t, 0, 8);
    bitstream_flush(&bs, &t);
    return 49;
}
/*- End of function --------------------------------------------------------*/

static void prepare_info1c(v34_state_t *s)
{
    int i;

    s->tx.info1c.power_reduction = 0;
    s->tx.info1c.additional_power_reduction = 0;
    s->tx.info1c.md = 0;
    s->tx.info1c.freq_offset = 0;

    for (i = 0;  i <= V34_BAUD_RATE_3429;  i++)
    {
        s->tx.info1c.rate_data[i].use_high_carrier = false;
        s->tx.info1c.rate_data[i].pre_emphasis = 6;
        s->tx.info1c.rate_data[i].max_bit_rate = (s->tx.baud_rate >= i)  ?  ((s->tx.parms.max_bit_rate_code >> 1) + 1)  :  0;
    }
}
/*- End of function --------------------------------------------------------*/

static void prepare_info1a(v34_state_t *s)
{
    s->tx.info1a.power_reduction = 0;
    s->tx.info1a.additional_power_reduction = 0;
    s->tx.info1a.md = 0;
    s->tx.info1a.freq_offset = 0;

    s->tx.info1a.use_high_carrier = false;
    s->tx.info1a.preemphasis_filter = 6;
    s->tx.info1a.max_data_rate = s->tx.parms.max_bit_rate_code;

    s->tx.info1a.baud_rate_a_to_c = s->tx.baud_rate;
    s->tx.info1a.baud_rate_c_to_a = s->tx.baud_rate;
}
/*- End of function --------------------------------------------------------*/

static void prepare_infoh(v34_state_t *s)
{
    s->tx.infoh.power_reduction = 0;
    s->tx.infoh.length_of_trn = 30;
    s->tx.infoh.use_high_carrier = 0;
    s->tx.infoh.preemphasis_filter = 0;
    s->tx.infoh.baud_rate = 14;
    s->tx.infoh.trn16 = 0;
}
/*- End of function --------------------------------------------------------*/

static int info1c_sequence_tx(v34_tx_state_t *s, info1c_t *info1c)
{
    uint8_t *t;
    uint16_t crc;
    bitstream_state_t bs;
    int i;

    log_info1c(s->logging, true, info1c);
    bitstream_init(&bs, true);
    t = s->txbuf;
    /* 0:3      Fill bits: 1111. */
    /* 4:11     Frame sync: 01110010, where the left-most bit is first in time. */
    bitstream_put(&bs, &t, INFO_FILL_AND_SYNC_BITS, 12);
    /* 12:14    Minimum power reduction to be implemented by the answer modem transmitter. An integer between 0 and 7
                gives the recommended power reduction in dB. These bits shall indicate 0 if INFO0a indicated that the answer
                modem transmitter cannot reduce its power. */
    bitstream_put(&bs, &t, info1c->power_reduction, 3);
    /* 15:17    Additional power reduction, below that indicated by bits 12-14, which can be tolerated by the call modem
                receiver. An integer between 0 and 7 gives the additional power reduction in dB. These bits shall indicate 0 if
                INFO0a indicated that the answer modem transmitter cannot reduce its power. */
    bitstream_put(&bs, &t, info1c->additional_power_reduction, 3);
    /* 18:24    Length of MD to be transmitted by the call modem during Phase 3. An integer between 0 and 127 gives the
                length of this sequence in 35 ms increments. */
    bitstream_put(&bs, &t, info1c->md, 7);
    /* 25       Set to 1 indicates that the high carrier frequency is to be used in transmitting from the answer modem to the call
                modem for a symbol rate of 2400. */
    /* 26:29    Pre-emphasis filter to be used in transmitting from the answer modem to the call modem for a symbol
                rate of 2400. These bits form an integer between 0 and 10 which represents the pre-emphasis filter index
                (see Tables 3 and 4). */
    /* 30:33    Projected maximum data rate for a symbol rate of 2400. These bits form an integer between 0 and 14 which
                gives the projected data rate as a multiple of 2400 bits/s. A 0 indicates the symbol rate cannot be used. */

    /* 34:42    Probing results pertaining to a final symbol rate selection of 2743 symbols per second. The coding of these
                9 bits is identical to that for bits 25-33. */

    /* 43:51    Probing results pertaining to a final symbol rate selection of 2800 symbols per second. The coding of these
                9 bits is identical to that for bits 25-33. */

    /* 52:60    Probing results pertaining to a final symbol rate selection of 3000 symbols per second. The coding of these
                9 bits is identical to that for bits 25-33. Information in this field shall be consistent with the answer modem
                capabilities indicated in INFO0a. */

    /* 61:69    Probing results pertaining to a final symbol rate selection of 3200 symbols per second. The coding of these
                9 bits is identical to that for bits 25-33. Information in this field shall be consistent with the answer modem
                capabilities indicated in INFO0a. */

    /* 70:78    Probing results pertaining to a final symbol rate selection of 3429 symbols per second. The coding of these
                9 bits is identical to that for bits 25-33. Information in this field shall be consistent with the answer modem
                capabilities indicated in INFO0a. */
    for (i = 0;  i <= 5;  i++)
    {
        bitstream_put(&bs, &t, info1c->rate_data[i].use_high_carrier, 1);
        bitstream_put(&bs, &t, info1c->rate_data[i].pre_emphasis, 4);
        bitstream_put(&bs, &t, info1c->rate_data[i].max_bit_rate, 4);
    }
    /*endfor*/
    /* 79:88    Frequency offset of the probing tones as measured by the call modem receiver. The frequency offset number
                shall be the difference between the nominal 1050 Hz line probing signal tone received and the 1050 Hz tone
                transmitted, f(received) and f(transmitted). A two's complement signed integer between -511 and 511 gives the
                measured offset in 0.02 Hz increments. Bit 88 is the sign bit of this integer. The frequency offset measurement
                shall be accurate to 0.25 Hz. Under conditions where this accuracy cannot be achieved, the integer shall be set
                to -512 indicating that this field is to be ignored. */
    bitstream_put(&bs, &t, info1c->freq_offset, 10);
    bitstream_emit(&bs, &t);
    crc = crc_bit_block(s->txbuf, 12, 88, 0xFFFF);
    /* 89:104   CRC. */
    bitstream_put(&bs, &t, crc, 16);
    /* 105:108  Fill bits: 1111. */
    bitstream_put(&bs, &t, 0xF, 4);
    /* Add some extra postamble, so we have a whole number of bytes to work with. */
    bitstream_put(&bs, &t, 0, 8);
    bitstream_flush(&bs, &t);
    return 109;
}
/*- End of function --------------------------------------------------------*/

static int info1a_sequence_tx(v34_tx_state_t *s, info1a_t *info1a)
{
    uint8_t *t;
    uint16_t crc;
    bitstream_state_t bs;

    log_info1a(s->logging, true, info1a);
    bitstream_init(&bs, true);
    t = s->txbuf;
    /* 0:3      Fill bits: 1111. */
    /* 4:11     Frame sync: 01110010, where the left-most bit is first in time. */
    bitstream_put(&bs, &t, INFO_FILL_AND_SYNC_BITS, 12);
    /* 12:14    Minimum power reduction to be implemented by the call modem transmitter. An integer between 0 and 7 gives
                the recommended power reduction in dB. These bits shall indicate 0 if INFO0c indicated that the call modem
                transmitter cannot reduce its power. */
    bitstream_put(&bs, &t, info1a->power_reduction, 3);
    /* 15:17    Additional power reduction, below that indicated by bits 12:14, which can be tolerated by the answer modem
                receiver. An integer between 0 and 7 gives the additional power reduction in dB. These bits shall indicate 0 if
                INFO0c indicated that the call modem transmitter cannot reduce its power. */
    bitstream_put(&bs, &t, info1a->additional_power_reduction, 3);
    /* 18:24    Length of MD to be transmitted by the answer modem during Phase 3. An integer between 0 and 127 gives the
                length of this sequence in 35 ms increments. */
    bitstream_put(&bs, &t, info1a->md, 7);
    /* 25       Set to 1 indicates that the high carrier frequency is to be used in transmitting from the call modem to the answer
                modem. This shall be consistent with the capabilities of the call modem indicated in INFO0c. */
    bitstream_put(&bs, &t, info1a->use_high_carrier, 1);
    /* 26:29    Pre-emphasis filter to be used in transmitting from the call modem to the answer modem. These bits form an
                integer between 0 and 10 which represents the pre-emphasis filter index (see Tables 3 and 4). */
    bitstream_put(&bs, &t, info1a->preemphasis_filter, 4);
    /* 30:33    Projected maximum data rate for the selected symbol rate from the call modem to the answer modem. These bits
                form an integer between 0 and 14 which gives the projected data rate as a multiple of 2400 bits/s. */
    bitstream_put(&bs, &t, info1a->max_data_rate, 4);
    /* 34:36    Symbol rate to be used in transmitting from the answer modem to the call modem. An integer between 0 and 5
                gives the symbol rate, where 0 represents 2400 and a 5 represents 3429. The symbol rate selected shall be
                consistent with information in INFO1c and consistent with the symbol rate asymmetry allowed as indicated in
                INFO0a and INFO0c. The carrier frequency and pre-emphasis filter to be used are those already indicated for
                this symbol rate in INFO1c. */
    bitstream_put(&bs, &t, info1a->baud_rate_a_to_c, 3);
    /* 37:39    Symbol rate to be used in transmitting from the call modem to the answer modem. An integer between 0 and 5
                gives the symbol rate, where 0 represents 2400 and a 5 represents 3429. The symbol rate selected shall be
                consistent with the capabilities indicated in INFO0a and consistent with the symbol rate asymmetry allowed as
                indicated in INFO0a and INFO0c. */
    bitstream_put(&bs, &t, info1a->baud_rate_c_to_a, 3);
    /* 40:49    Frequency offset of the probing tones as measured by the answer modem receiver. The frequency offset number
                shall be the difference between the nominal 1050 Hz line probing signal tone received and the 1050 Hz tone
                transmitted, f(received) and f(transmitted). A two's complement signed integer between -511 and 511 gives the
                measured offset in 0.02 Hz increments. Bit 49 is the sign bit of this integer. The frequency offset measurement
                shall be accurate to 0.25 Hz. Under conditions where this accuracy cannot be achieved, the integer shall be set
                to -512 indicating that this field is to be ignored. */
    bitstream_put(&bs, &t, info1a->freq_offset, 10);
    bitstream_emit(&bs, &t);
    crc = crc_bit_block(s->txbuf, 12, 49, 0xFFFF);
    /* 50:65    CRC. */
    bitstream_put(&bs, &t, crc, 16);
    /* 66:69    Fill bits: 1111. */
    bitstream_put(&bs, &t, 0xF, 4);
    /* Add some extra postamble, so we have a whole number of bytes to work with. */
    bitstream_put(&bs, &t, 0, 8);
    bitstream_flush(&bs, &t);
    return 70;
}
/*- End of function --------------------------------------------------------*/

static int infoh_sequence_tx(v34_tx_state_t *s, infoh_t *infoh)
{
    uint8_t *t;
    uint16_t crc;
    bitstream_state_t bs;

    log_infoh(s->logging, true, infoh);
    bitstream_init(&bs, true);
    t = s->txbuf;
    /* 0:3      Fill bits: 1111. */
    /* 4:11     Frame sync: 01110010, where the left-most bit is first in time. */
    bitstream_put(&bs, &t, INFO_FILL_AND_SYNC_BITS, 12);
    /* 12:14    Power reduction requested by the recipient modem receiver. An integer between 0 and 7
                gives the requested power reduction in dB. These bits shall indicate 0 if the source
                modem's INFO0 indicated that the source modem transmitter cannot reduce its power. */
    bitstream_put(&bs, &t, infoh->power_reduction, 3);
    /* 15:21    Length of TRN to be transmitted by the source modem during Phase 3. An integer between
                0 and 127 gives the length of this sequence in 35 ms increments. */
    bitstream_put(&bs, &t, infoh->length_of_trn, 7);
    /* 22       Set to 1 indicates the high carrier frequency is to be used in data mode transmission. This
                must be consistent with the capabilities indicated in the source modem's INFO0. */
    bitstream_put(&bs, &t, infoh->use_high_carrier, 1);
    /* 23:26    Pre-emphasis filter to be used in transmitting from the source modem to the recipient modem.
                These bits form an integer between 0 and 10 which represents the pre-emphasis filter index
                (see Tables 3 and 4). */
    bitstream_put(&bs, &t, infoh->preemphasis_filter, 4);
    /* 27:29    Symbol rate to be used for data transmission. An integer between 0 and 5 gives the symbol rate, where 0
                represents 2400 and a 5 represents 3429. */
    bitstream_put(&bs, &t, infoh->baud_rate, 3);
    /* 30       Set to 1 indicates TRN uses a 16-point constellation, 0 indicates TRN uses a 4-point constellation. */
    bitstream_put(&bs, &t, infoh->trn16, 1);
    bitstream_emit(&bs, &t);
    crc = crc_bit_block(s->txbuf, 12, 30, 0xFFFF);
    /* 31:46    Code CRC. */
    bitstream_put(&bs, &t, crc, 16);
    /* 47:50    Fill bits: 1111. */
    bitstream_put(&bs, &t, 0xF, 4);
    /* Add some extra postamble, so we have a whole number of bytes to work with. */
    bitstream_put(&bs, &t, 0, 8);
    bitstream_flush(&bs, &t);
    return 51;
}
/*- End of function --------------------------------------------------------*/

static int mp_sequence_tx(v34_tx_state_t *s, mp_t *mp)
{
    int i;
    int len;
    uint8_t *t;
    uint16_t crc;
    bitstream_state_t bs;

    log_mp(s->logging, true, mp);
    bitstream_init(&bs, true);
    t = s->txbuf;
    /* 0:16     Frame sync: 11111111111111111. */
    /* 17       Start bit: 0. */
    bitstream_put(&bs, &t, 0x1FFFF, 18);
    /* 18       Type: 0 or 1. */
    bitstream_put(&bs, &t, mp->type, 1);
    /* 19       Reserved for ITU-T: This bit is set to 0 by the transmitting modem and is not
                interpreted by the receiving modem. */
    bitstream_put(&bs, &t, 0, 1);
    /* 20:23    Maximum call modem to answer modem data signalling rate: Data rate = N * 2400
                where N is a four-bit integer between 1 and 14. */
    bitstream_put(&bs, &t, mp->bit_rate_c_to_a, 4);
    /* 24:27    Maximum answer modem to call modem data signalling rate: Data rate = N * 2400
                where N is a four-bit integer between 1 and 14. */
    bitstream_put(&bs, &t, mp->bit_rate_a_to_c, 4);
    /* 28       Auxiliary channel select bit. Set to 1 if modem is capable of supporting and
                enables auxiliary channel. Auxiliary channel is used only if both modems set
                this bit to 1. */
    bitstream_put(&bs, &t, mp->aux_channel_supported, 1);
    /* 29:30    Trellis encoder select bits:
                0 = 16 state; 1 = 32 state; 2 = 64 state; 3 = Reserved for ITU-T.
                Receiver requires remote-end transmitter to use selected trellis encoder. */
    bitstream_put(&bs, &t, mp->trellis_size, 2);
    /* 31       Non-linear encoder parameter select bit for the remote-end transmitter.
                0: Q = 0, 1: Q = 0.3125. */
    bitstream_put(&bs, &t, mp->use_non_linear_encoder, 1);
    /* 32       Constellation shaping select bit for the remote-end transmitter.
                0: minimum, 1: expanded (see Table 10). */
    bitstream_put(&bs, &t, mp->expanded_shaping, 1);
    /* 33       Acknowledge bit. 0 = modem has not received MP from far end. 1 = received MP from far end. */
    bitstream_put(&bs, &t, mp->mp_acknowledged, 1);
    /* 34       Start bit: 0. */
    bitstream_put(&bs, &t, 0, 1);
    /* 35:49    Data signalling rate capability mask.
                Bit 35:2400; bit 36:4800; bit 37:7200;...; bit 46:28 800; bit 47:31 200; bit 48:33 600;
                bit 49: Reserved for ITU-T. (This bit is set to 0 by the transmitting modem and is not
                interpreted by the receiving modem.) Bits set to 1 indicate data signalling rates supported
                and enabled in both transmitter and receiver of modem. */
    bitstream_put(&bs, &t, mp->signalling_rate_mask, 15);
    /* 50       Asymmetric data signalling rate enable. Set to 1 indicates modem capable of asymmetric
                data signalling rates. */
    bitstream_put(&bs, &t, mp->asymmetric_rates_allowed, 1);
    if (mp->type == 1)
    {
        /* 51       Start bit: 0. */
        /* 52:67    Precoding coefficient h(1) real. */
        /* 68       Start bit: 0. */
        /* 69:84    Precoding coefficient h(1) imaginary. */
        /* 85       Start bit: 0. */
        /* 86:101   Precoding coefficient h(2) real. */
        /* 102      Start bit: 0. */
        /* 103:118  Precoding coefficient h(2) imaginary. */
        /* 119      Start bit: 0. */
        /* 120:135  Precoding coefficient h(3) real. */
        /* 136      Start bit: 0. */
        /* 137:152  Precoding coefficient h(3) imaginary. */
        for (i = 0;  i < 3;  i++)
        {
            bitstream_put(&bs, &t, 0, 1);
            bitstream_put(&bs, &t, mp->precoder_coeffs[i].re, 16);
            bitstream_put(&bs, &t, 0, 1);
            bitstream_put(&bs, &t, mp->precoder_coeffs[i].im, 16);
        }
        /*endfor*/
    }
    /*endif*/
    /* 51/153           Start bit: 0. */
    bitstream_put(&bs, &t, 0, 1);
    /* 52:67/154:169    Reserved for ITU-T: These bits are set to 0 by the transmitting modem and are
                        not interpreted by the receiving modem. */
    bitstream_put(&bs, &t, 0, 16);
    /* 68/170           Start bit: 0. */
    bitstream_put(&bs, &t, 0, 1);
    bitstream_emit(&bs, &t);
    crc = 0xFFFF;
    len = (mp->type == 1)  ?  170  :  68;
    for (i = 17;  i < len;  i += 17)
        crc = crc_bit_block(s->txbuf, i, i + 15, crc);
    /*endfor*/
    /* 69:84/171:186    CRC. */
    bitstream_put(&bs, &t, crc, 16);
    /* 85:87 Fill bits: 000.    187 Fill bit: 0. */
    if (mp->type == 1)
        bitstream_put(&bs, &t, 0, 1);
    else
        bitstream_put(&bs, &t, 0, 3);
    /*endif*/
    /* Add some extra postamble, so we have a whole number of bytes to work with. */
    bitstream_put(&bs, &t, 0, 8);
    bitstream_flush(&bs, &t);
    return (mp->type == 1)  ?  188  :  88;
}
/*- End of function --------------------------------------------------------*/

static int mph_sequence_tx(v34_tx_state_t *s, mph_t *mph)
{
    int i;
    int len;
    uint8_t *t;
    uint16_t crc;
    bitstream_state_t bs;

    log_mph(s->logging, true, mph);
    bitstream_init(&bs, true);
    t = s->txbuf;
    /* 0:16     Frame sync: 11111111111111111. */
    /* 17       Start bit: 0. */
    bitstream_put(&bs, &t, 0x1FFFF, 18);
    /* 18       Type: */
    bitstream_put(&bs, &t, mph->type, 1);
    /* 19       Reserved for ITU-T: This bit is set to 0 by the transmitting modem and is not
                interpreted by the receiving modem. */
    bitstream_put(&bs, &t, 0, 1);
    /* 20:23    Maximum data signalling rate:
                Data rate = N * 2400 where N is a four-bit integer between 1 and 14. */
    bitstream_put(&bs, &t, mph->max_data_rate, 4);
    /* 24:26    Reserved for ITU-T: These bits are set to 0 by the transmitting modem and are
                not interpreted by the receiving modem. */
    bitstream_put(&bs, &t, 0, 3);
    /* 27       Control channel data signalling rate selected for remote transmitter.
                0 = 1200 bit/s, 1 = 2400 bit/s (see bit 50 below). */
    bitstream_put(&bs, &t, mph->control_channel_2400, 1);
    /* 28       Reserved for ITU-T: This bit is set to 0 by the transmitting modem and is not
                interpreted by the receiving modem. */
    bitstream_put(&bs, &t, 0, 1);
    /* 29:30    Trellis encoder select bits:
                0 = 16 state; 1 = 32 state; 2 = 64 state; 3 = Reserved for ITU-T.
                Receiver requires remote-end transmitter to use selected trellis encoder. */
    bitstream_put(&bs, &t, mph->trellis_size, 2);
    /* 31       Non-linear encoder parameter select bit for the remote-end transmitter.
                0: Q = 0, 1: Q = 0.3125. */
    bitstream_put(&bs, &t, mph->use_non_linear_encoder, 1);
    /* 32       Constellation shaping select bit for the remote-end transmitter.
                0: minimum, 1: expanded (see Table 10). */
    bitstream_put(&bs, &t, mph->expanded_shaping, 1);
    /* 33       Reserved for ITU-T: This bit is set to 0 by the transmitting modem and is not
                interpreted by the receiving modem. */
    bitstream_put(&bs, &t, 0, 1);
    /* 34       Start bit: 0. */
    bitstream_put(&bs, &t, 0, 1);
    /* 35:49    Data signalling rate capability mask.
                Bit 35:2400; bit 36:4800; bit 37:7200;...; bit 46:28 800; bit 47:31 200; bit 48:33 600;
                bit 49: Reserved for ITU-T. (This bit is set to 0 by the transmitting modem and is not
                interpreted by the receiving modem.) Bits set to 1 indicate data signalling rates supported
                and enabled in both transmitter and receiver of modem. */
    bitstream_put(&bs, &t, mph->signalling_rate_mask, 15);
    /* 50       Enables asymmetric control channel data rates:
                0 = Asymmetric mode not allowed; 1 = Asymmetric mode allowed.
                    Asymmetric mode shall be used only when both modems set bit 50 to 1. If different data
                rates are selected in symmetric mode, both modems shall transmit at the lower rate. */
    bitstream_put(&bs, &t, mph->asymmetric_rates_allowed, 1);
    if (mph->type == 1)
    {
        /* 51       Start bit: 0. */
        /* 52:67    Precoding coefficient h(1) real. */
        /* 68       Start bit: 0. */
        /* 69:84    Precoding coefficient h(1) imaginary. */
        /* 85       Start bit: 0. */
        /* 86:101   Precoding coefficient h(2) real. */
        /* 102      Start bit: 0. */
        /* 103:118  Precoding coefficient h(2) imaginary. */
        /* 119      Start bit: 0. */
        /* 120:135  Precoding coefficient h(3) real. */
        /* 136      Start bit: 0. */
        /* 137:152  Precoding coefficient h(3) imaginary. */
        for (i = 0;  i < 3;  i++)
        {
            bitstream_put(&bs, &t, 0, 1);
            bitstream_put(&bs, &t, mph->precoder_coeffs[i].re, 16);
            bitstream_put(&bs, &t, 0, 1);
            bitstream_put(&bs, &t, mph->precoder_coeffs[i].im, 16);
        }
        /*endfor*/
    }
    /*endif*/
    /* 51/153           Start bit: 0. */
    bitstream_put(&bs, &t, 0, 1);
    /* 52:67/154:169    Reserved for ITU-T: These bits are set to 0 by the transmitting modem and are not
                        interpreted by the receiving modem. */
    bitstream_put(&bs, &t, 0, 16);
    /* 68/170           Start bit: 0. */
    bitstream_put(&bs, &t, 0, 1);
    bitstream_emit(&bs, &t);
    crc = 0xFFFF;
    len = (mph->type == 1)  ?  170  :  68;
    for (i = 17;  i < len;  i += 17)
        crc = crc_bit_block(s->txbuf, i, i + 15, crc);
    /*endfor*/
    /* 69:84/171:186    CRC. */
    bitstream_put(&bs, &t, crc, 16);
    /* 85:87 Fill bits: 000.    187 Fill bit: 0. */
    if (mph->type == 1)
        bitstream_put(&bs, &t, 0, 1);
    else
        bitstream_put(&bs, &t, 0, 3);
    /*endif*/
    /* Add some extra postamble, so we have a whole number of bytes to work with. */
    bitstream_put(&bs, &t, 0, 8);
    bitstream_flush(&bs, &t);
    return (mph->type == 1)  ?  188  :  88;
}
/*- End of function --------------------------------------------------------*/

static int fake_get_bit(void *user_data)
{
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void parse_primary_channel_bitstream(v34_tx_state_t *s)
{
    uint8_t *u;
    const uint8_t *t;
    int i;
    int n;
    int bit;
    int bb;
    int kk;

    /* Parse a series of input data bits into a set of S bits, Q bits, and I bits which we can
       feed into the modulation process. */
    bitstream_init(&s->bs, true);
    u = s->txbuf;
    bb = s->parms.b;
    kk = s->parms.k;
    /* If there are S bits we switch between high mapping frames and low mapping frames based
       on the SWP pattern. We derive SWP algorithmically.  Note that high/low mapping is only
       relevant when b >= 12. */
    s->s_bit_cnt += s->parms.r;
    if (s->s_bit_cnt >= s->parms.p)
    {
        /* This is a high mapping frame */
        s->s_bit_cnt -= s->parms.p;
    }
    else
    {
        if (bb > 12)
        {
            /* We need one less bit in a low mapping frame */
            bb--;
            kk--;
        }
        /*endif*/
    }
    /*endif*/
    i = 0;
    /* The first of the I bits might be auxiliary data */
    s->aux_bit_cnt += s->parms.w;
    if (s->aux_bit_cnt >= s->parms.p)
    {
        s->aux_bit_cnt -= s->parms.p;
        /* Insert an auxiliary data bit after the K bits, where it will appear as
           the first of the I bits. */
        for (  ;  i < kk;  i++)
        {
            if ((bit = s->current_get_bit(s->get_bit_user_data)) == SIG_STATUS_END_OF_DATA)
            {
                /* TODO: Need to handle things properly here. SIG_STATUS_END_OF_DATA may not
                         mean shut down the modem. It may mean shut down the current mode, when
                         we are working half-duplex. */
                s->current_get_bit = fake_get_bit;
            }
            /*endif*/
            bitstream_put(&s->bs, &u, scramble(s, bit), 1);
        }
        /*endfor*/
        /* Auxiliary data bits are not scrambled (V.34/7) */
        bit = (s->get_aux_bit)  ?  s->get_aux_bit(s->get_aux_bit_user_data)  :  0;
        bitstream_put(&s->bs, &u, bit, 1);
        i++;
    }
    for (  ;  i < bb;  i++)
    {
        if ((bit = s->current_get_bit(s->get_bit_user_data)) == SIG_STATUS_END_OF_DATA)
        {
            /* TODO: Need to handle things properly here. SIG_STATUS_END_OF_DATA may not
                     mean shut down the modem. It may mean shut down the current mode, when
                     we are working half-duplex. */
            s->current_get_bit = fake_get_bit;
        }
        /*endif*/
        bitstream_put(&s->bs, &u, scramble(s, bit), 1);
    }
    /*endfor*/
    bitstream_flush(&s->bs, &u);

    bitstream_init(&s->bs, true);
    t = s->txbuf;
    if (s->parms.k)
    {
        /* V.34/9.3.1 */
        /* K is always < 32, so we always get the entire K bits from a single word */
        s->r0 = bitstream_get(&s->bs, &t, kk);
        for (i = 0;  i < 4;  i++)
        {
            /* Some I bits. These are always present, and always 3 bits each. */
            s->ibits[i] = bitstream_get(&s->bs, &t, 3);
            /* Maybe some uncoded Q bits. */
            if (s->parms.q)
            {
                s->qbits[2*i] = bitstream_get(&s->bs, &t, s->parms.q);
                s->qbits[2*i + 1] = bitstream_get(&s->bs, &t, s->parms.q);
            }
            else
            {
                s->qbits[2*i] = 0;
                s->qbits[2*i + 1] = 0;
            }
            /*endif*/
        }
        /*endfor*/
    }
    else
    {
        /* V.34/9.3.2 */
        /* If K is zero (i.e. b = 8, 9, 11, or 12), things need slightly special treatment */
        /* Some I bits. These are always present, and may be 2 or 3 bits each. */
        /* Need to treat 8, 9, 11, and 12 individually */
        s->r0 = 0;
        n = bb - 8;
        for (i = 0;  i < n;  i++)
            s->ibits[i] = bitstream_get(&s->bs, &t, 3);
        /*endfor*/
        for (  ;  i < 4;  i++)
            s->ibits[i] = bitstream_get(&s->bs, &t, 2);
        /*endfor*/
        /* No uncoded Q bits */
        for (i = 0;  i < 8;  i++)
            s->qbits[i] = 0;
        /*endfor*/
    }
    /*endif*/
    span_log(s->logging,
             SPAN_LOG_FLOW,
             "Tx - Parsed %p %8X - %X %X %X %X - %2X %2X %2X %2X %2X %2X %2X %2X\n",
             s,
             s->r0,
             s->ibits[0],
             s->ibits[1],
             s->ibits[2],
             s->ibits[3],
             s->qbits[0],
             s->qbits[1],
             s->qbits[2],
             s->qbits[3],
             s->qbits[4],
             s->qbits[5],
             s->qbits[6],
             s->qbits[7]);
}
/*- End of function --------------------------------------------------------*/

static void shell_map(v34_tx_state_t *s)
{
    int32_t a;
    int32_t b;
    int32_t c;
    int32_t d;
    int32_t e;
    int32_t f;
    int32_t g;
    int32_t h;
    int32_t r[6];
    int32_t t1;
    int32_t t2;
    const uint32_t *g2;
    const uint32_t *g4;
    const uint32_t *z8;

    if (s->parms.m == 0)
    {
        s->mjk[0] = 0;
        s->mjk[1] = 0;
        s->mjk[2] = 0;
        s->mjk[3] = 0;
        s->mjk[4] = 0;
        s->mjk[5] = 0;
        s->mjk[6] = 0;
        s->mjk[7] = 0;
        return;
    }
    /*endif*/
    g2 = g2s[s->parms.m];
    g4 = g4s[s->parms.m];
    z8 = z8s[s->parms.m];

    /* TODO: This code comes directly from the equations in V.34. Can it be made faster? */

    for (a = 1;  z8[a] <= s->r0;  a++)
        /*loop*/;
    /*endfor*/
    /* We are now at a ring which is too big, so step back one */
    a--;

    /* V.34/9-8 */
    t2 = s->r0 - z8[a];
    for (b = -1;  t2 >= 0;  )
    {
        b++;
        t1 = g4[b]*g4[a - b];
        t2 -= t1;
    }
    /*endfor*/
    r[1] = t2 + t1;

    /* V.34/9-9 */
    r[2] = r[1]%g4[b];

    /* V.34/9-10 */
    r[3] = (r[1] - r[2])/g4[b];

    /* V.34/9-11 */
    t2 = r[2];
    for (c = -1;  t2 >= 0;  )
    {
        c++;
        t1 = g2[c]*g2[b - c];
        t2 -= t1;
    }
    /*endfor*/
    r[4] = t2 + t1;

    /* V.34/9-12 */
    t2 = r[3];
    for (d = -1;  t2 >= 0;  )
    {
        d++;
        t1 = g2[d]*g2[a - b - d];
        t2 -= t1;
    }
    /*endfor*/
    r[5] = t2 + t1;

    /* V.34/9-13 */
    e = r[4]%g2[c];
    /* V.34/9-14 */
    f = (r[4] - e)/g2[c];

    /* V.34/9-15 */
    g = r[5]%g2[d];
    /* V.34/9-16 */

    h = (r[5] - g)/g2[d];

    if (c < s->parms.m)
    {
        /* V.34/9-17 */
        s->mjk[0] = e;
        s->mjk[1] = c - s->mjk[0];
    }
    else
    {
        /* V.34/9-18 */
        s->mjk[1] = s->parms.m - 1 - e;
        s->mjk[0] = c - s->mjk[1];
    }
    /*endif*/

    if (b - c < s->parms.m)
    {
        /* V.34/9-19 */
        s->mjk[2] = f;
        s->mjk[3] = b - c - s->mjk[2];
    }
    else
    {
        /* V.34/9-20 */
        s->mjk[3] = s->parms.m - 1 - f;
        s->mjk[2] = b - c - s->mjk[3];
    }
    /*endif*/

    if (d < s->parms.m)
    {
        /* V.34/9-21 */
        s->mjk[4] = g;
        s->mjk[5] = d - s->mjk[4];
    }
    else
    {
        /* V.34/9-22 */
        s->mjk[5] = s->parms.m - 1 - g;
        s->mjk[4] = d - s->mjk[5];
    }
    /*endif*/

    if (a - b - d < s->parms.m)
    {
        /* V.34/9-23 */
        s->mjk[6] = h;
        s->mjk[7] = a - b - d - s->mjk[6];
    }
    else
    {
        /* V.34/9-24 */
        s->mjk[7] = s->parms.m - 1 - h;
        s->mjk[6] = a - b - d - s->mjk[7];
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static complexi16_t v34_non_linear_encoder(complexi16_t *pre)
{
    int32_t zeta;
    int32_t x;
    complexi16_t post;

    /* V.34/9.7 for the 0.3125 case */
    /* 341/2048 is 1/6 */
    zeta = ((((int32_t) pre->re*(int32_t) pre->re + (int32_t) pre->im*(int32_t) pre->im + 0x800) >> 12)*341 + 0x800) >> 12;
    /* 15127/16384 is 0.92328 */
    /* 19661/65536 is 6*6/120 */
    x = (zeta*zeta + 0x2000) >> 14;
    x = (zeta + ((x*19661) >> 16)*15127 + 0x4000) >> 14;
    post.re = (int16_t) ((int32_t) pre->re*x >> 14);
    post.im = (int16_t) ((int32_t) pre->im*x >> 14);
    return post;
}
/*- End of function --------------------------------------------------------*/

static complexi16_t rotate90_clockwise(complexi16_t *x, int quads)
{
    complexi16_t y;

    /* Rotate a point clockwise by "quads" 90 degree steps */
    /* These are simple negate and swap operations */
    switch (quads & 3)
    {
    case 0:
        y.re = x->re;
        y.im = x->im;
        break;
    case 1:
        y.re = x->im;
        y.im = -x->re;
        break;
    case 2:
        y.re = -x->re;
        y.im = -x->im;
        break;
    case 3:
        y.re = -x->im;
        y.im = x->re;
        break;
    }
    /*endswitch*/
    return y;
}
/*- End of function --------------------------------------------------------*/

/* Determine the 3 bits subset label for a particular constellation point */
static int16_t get_binary_subset_label(complexi16_t *pos)
{
    int16_t x;
    int16_t xored;
    int16_t subset;

    /* See V.34/9.6.3.1 */
    xored = pos->re ^ pos->im;
    x = xored & 2;
    subset = ((xored & 4) ^ (x << 1)) | (pos->re & 2) | (x >> 1);
    //printf("XXX Pre subset %d,%d => %d\n", pos->re, pos->im, subset);
    return subset;
}
/*- End of function --------------------------------------------------------*/

static complexi16_t quantize_tx(v34_tx_state_t *s, complexi16_t *x)
{
    complexi16_t y;

    /* Value is stored in Q9.7 format. */
    y.re = abs(x->re);
    y.im = abs(x->im);
    if (s->parms.b >= 56)
    {
        /* 2w is 4 */
        /* Output integer values. i.e. 16:0 */
        /* We must mask out the 1st and 2nd bits, because we are rounding to the 3rd bit.
           All numbers coming out of this routine should be multiples of 4. */
        y.re = (y.re + 0x0FF) >> 7;
        y.re &= ~0x03;
        y.im = (y.im + 0x0FF) >> 7;
        y.im &= ~0x03;
    }
    else
    {
        /* 2w is 2 */
        /* Output integer values. i.e. Q16.0 */
        /* We must mask out the 1st bit because we are rounding to the 2nd bit
           All numbers coming out of this routine should be multiples of 2 (i.e. even). */
        y.re = (y.re + 0x07F) >> 7;
        y.re &= ~0x01;
        y.im = (y.im + 0x07F) >> 7;
        y.im &= ~0x01;
    }
    /*endif*/
    if (x->re < 0)
        y.re = -y.re;
    /*endif*/
    if (x->im < 0)
        y.im = -y.im;
    /*endif*/
    return y;
}
/*- End of function --------------------------------------------------------*/

static complexi16_t precoder_tx_filter(v34_tx_state_t *s)
{
    int i;
    int j;
    complexi32_t sum;
    complexi16_t p;

#if 1
#elif 0
s->precoder_coeffs[0].re = 0x1aee;
s->precoder_coeffs[0].re = 0x0308;
s->precoder_coeffs[1].re = 0xf995;
s->precoder_coeffs[1].re = 0x0065;
s->precoder_coeffs[2].re = 0x0de0;
s->precoder_coeffs[2].re = 0xfe37;
#else
s->precoder_coeffs[0].re = 0x1adb;
s->precoder_coeffs[0].re = 0x0230;
s->precoder_coeffs[1].re = 0xf95b;
s->precoder_coeffs[1].re = 0x0069;
s->precoder_coeffs[2].re = 0x0def;
s->precoder_coeffs[2].re = 0xfe34;
#endif
    /* 9.6.2/V.34 */
    /* h's are stored in Q2.14
       x's are stored in Q9.7
       not sure about x's
       so product is in Q11.21 */
    sum = complex_seti32(0, 0);
    for (i = 0;  i < 3;  i++)
    {
        j = V34_XOFF + s->step_2d - i;
        sum.re += (s->x[j].re*s->precoder_coeffs[i].re - s->x[j].im*s->precoder_coeffs[i].im);
        sum.im += (s->x[j].re*s->precoder_coeffs[i].im + s->x[j].im*s->precoder_coeffs[i].re);
    }
    /*endfor*/
    /* 9.6.2/V.34 item 2 - Round Q11.21 number format to Q9.7 */
    p.re = (abs(sum.re) + 0x01FFFL) >> 14;
    if (sum.re < 0)
        p.re = -p.re;
    /*endif*/
    p.im = (abs(sum.im) + 0x01FFFL) >> 14;
    if (sum.im < 0)
        p.im = -p.im;
    /*endif*/
    return p;
}
/*- End of function --------------------------------------------------------*/

static void qam_mod(v34_tx_state_t *s)
{
//    printf("QAM %p [%6d, %6d] [%8.3f, %8.3f] [%8.3f, %8.3f]\n",
//           s,
//           s->x[V34_XOFF + s->step_2d].re,
//           s->x[V34_XOFF + s->step_2d].im,
//           FP_Q9_7_TO_F(s->x[V34_XOFF + s->step_2d].re),
//           FP_Q9_7_TO_F(s->x[V34_XOFF + s->step_2d].im),
//           35.77*s->x[V34_XOFF + s->step_2d].re,
//           35.77*s->x[V34_XOFF + s->step_2d].im);
//    fflush(stdout);
}
/*- End of function --------------------------------------------------------*/

/* Keep this global until the modem is VERY well tested */
SPAN_DECLARE(int) v34_get_mapping_frame(v34_tx_state_t *s, int16_t bits[16])
{
    int len;
    int y4321;
    int c0;
    int u0;
    int v0;
    int rot;
    int32_t sum1;
    int32_t sum2;
    complexi16_t u;
    complexi16_t v;
    complexi16_t y;
    complexi16_t c_prev;
    int subsets[2];
    int mapping_index;

    /* This gets the four 4D symbols (eight 2D symbols) of a mapping frame */
    parse_primary_channel_bitstream(s);
    shell_map(s);

    u0 = 0;
    for (s->step_2d = 0;  s->step_2d < 8;  s->step_2d++)
    {
        /* Steps to map, precode and trellis code a 4D symbol (2 x 2D symbols)
           Step    Inputs                              Operation               Outputs
            1      Z(m), v(2m)                         9.6.1                   u(2m)
            2      u(2m), c(2m), p(2m)                 9.6.2, item 4           y(2m), x(2m)
            3      x(2m)                               9.6.2, items 1 to 3     c(2m + 1), p(2m + 1)
            4      c(2m), c(2m + 1)                    9.6.3.3                 C0(m)
            5      C0(m), Y0(m), V0(m)                 9.6.3                   U0(m)
            6      Z(m), U0(m), v(2m + 1)              9.6.1                   u(2m + 1)
            7      u(2m + 1), c(2m + 1), p(2m + 1)     9.6.2, item 4           y(2m + 1), x(2m + 1)
            8      x(2m + 1)                           9.6.2, items 1 to 3     c(2m + 2), p(2m + 2)
            9      y(2m), y(2m + 1)                    9.6.3.1, 9.6.3.2        Y0(m + 1) */
        /* 9.6.1/V.34 - Get the initial unrotated constellation point from the table. */
        mapping_index = ((s->mjk[s->step_2d] << s->parms.q) + s->qbits[s->step_2d]);
        v.re = v34_superconstellation[mapping_index][0];
        v.im = v34_superconstellation[mapping_index][1];
//printf("W %d %d %d %d\n", s->step_2d, s->mjk[s->step_2d], s->parms.q, s->qbits[s->step_2d]);
//printf("V %d - %d %d\n", mapping_index, v.re, v.im);
        if ((s->step_2d & 1) == 0)
        {
            /* Figure 6/V.34, 9.5/V.34 - Differential encoder */
            s->z = (s->z + (s->ibits[s->step_2d >> 1] >> 1)) & 3;
            /* Table 11/V.34 step 1, 9.6.1/V.34 - Rotation factor */
            rot = s->z;
        }
        else
        {
            /* Table 11/V.34 step 6, 9.6.1/V.34 - Compute rotation factor */
            rot = (s->z + ((s->ibits[s->step_2d >> 1] & 1) << 1) + u0) & 3;
        }
        /*endif*/
        u = rotate90_clockwise(&v, rot);
//printf("QMA %p [%6d, %6d] [%6d, %6d] (%d)\n", s, v.re, v.im, u.re, u.im, rot);

        /* Table 11/V.34 step 2/7, 9.6.2/V.34 item 4 - Compute the channel output signal y(n), and the precoded signal x(n) */
        y.re = u.re + s->c.re;
        y.im = u.im + s->c.im;
        s->x[V34_XOFF + s->step_2d].re = (y.re << 7) - s->p.re;
        s->x[V34_XOFF + s->step_2d].im = (y.im << 7) - s->p.im;

        subsets[s->step_2d & 1] = get_binary_subset_label(&y);
        qam_mod(s);
        bits[2*s->step_2d] = s->x[V34_XOFF + s->step_2d].re;
        bits[2*s->step_2d + 1] = s->x[V34_XOFF + s->step_2d].im;

        /* Table 11/V.34 step 3/8, 9.6.2/V.34 items 1 and 2 */
        s->p = precoder_tx_filter(s);
        if (s->use_non_linear_encoder)
            s->p = v34_non_linear_encoder(&s->p);
        /*endif*/
        c_prev = s->c;
        /* Table 11/V.34 step 3/8, 9.6.2/V.34 item 3 */
        s->c = quantize_tx(s, &s->p);

        if ((s->step_2d & 1) == 0)
        {
            /* Table 11/V.34 step 4, 9.6.3.3/V.34 */
            sum1 = (c_prev.re + c_prev.im) >> 1;
            sum2 = (s->c.re + s->c.im) >> 1;
            c0 = (sum1 ^ sum2) & 1;
            /* Superframe synchronisation bit inversion indicator */
            /* From Table 12/V.34. If J is 7, then 14 bits of this are used. If J is 8,
               all 16 bits are used. */
            /* Inversions are applied to the first 4D symbol in each half data frame. If P
               is 12, 14 or 16, the inversion will be in the first 4D symbol of a mapping frame.
               If P is 15, the inversions will alternate between being in the first and third 4D
               symbols of a mapping frame. */
            if ((s->data_frame*8 + s->step_2d)%(4*s->parms.p) == 0)
                v0 = (0x5FEE >> s->v0_pattern++) & 1;
            else
                v0 = 0;
            /*endif*/
            /* Table 11/V.34 step 5, 9.6.3/V.34 */
            u0 = (s->y0 ^ c0 ^ v0) & 1;
        }
        else
        {
            y4321 = conv_encode_input[subsets[0]][subsets[1]];
            /* Table 11/V.34 step 9, 9.6.3.1/V.34 and 9.6.3.2/V.34 */
            s->y0 = s->state & 1;
            s->state = (*s->conv_encode_table)[s->state][y4321];
//printf("Y4321 %d %d - %d %d %d\n", subsets[0], subsets[1], y4321, s->y0, s->state);
//printf("WWW 0x%x 0x%x -> 0x%x\n", v0, y4321, s->state);
        }
        /*endif*/
    }
    /*endfor*/
    /* At the end of the eight 2D symbols of a mapping frame. We need to reset some buffers.
       These values are remembered from one mapping frame to the next. */
    s->x[V34_XOFF - 3] = s->x[V34_XOFF + 5];
    s->x[V34_XOFF - 2] = s->x[V34_XOFF + 6];
    s->x[V34_XOFF - 1] = s->x[V34_XOFF + 7];

    if (++s->data_frame >= s->parms.p)
    {
        s->data_frame = 0;

        if (++s->super_frame >= s->parms.j)
        {
            s->super_frame = 0;
            s->v0_pattern = 0;
        }
        /*endif*/
    }
    /*endif*/
//printf("QAZ data frame %d, super frame %d\n", s->data_frame, s->super_frame);

    len = 2*8;
    return len;
}
/*- End of function --------------------------------------------------------*/

static __inline__ float exact_baud_rate(int symbol_rate_code)
{
    float a;
    float c;

    a = baud_rate_parameters[symbol_rate_code].a;
    c = baud_rate_parameters[symbol_rate_code].c;
    return 2400.0f*a/c;
}
/*- End of function --------------------------------------------------------*/

static __inline__ float carrier_frequency(int symbol_rate_code, int low_high)
{
    float d;
    float e;

    d = baud_rate_parameters[symbol_rate_code].low_high[low_high].d;
    e = baud_rate_parameters[symbol_rate_code].low_high[low_high].e;
    return exact_baud_rate(symbol_rate_code)*d/e;
}
/*- End of function --------------------------------------------------------*/

static int get_data_bit(v34_tx_state_t *s)
{
    int bit;

    if (s->txptr >= s->txbits)
        return -1;
    /*endif*/
    bit = (s->txbuf[s->txptr >> 3] >> (s->txptr & 7)) & 1;
    s->txptr++;
    return bit;
}
/*- End of function --------------------------------------------------------*/

static complex_sig_t get_transmission_preamble_baud(v34_state_t *s)
{
    if (++s->tx.txptr >= s->tx.txbits)
        info0_baud_init(s);
    /*endif*/
    return s->tx.lastbit;
}
/*- End of function --------------------------------------------------------*/

static void transmission_preamble_init(v34_state_t *s)
{
    /* Send some bits as the modulator starts up, to allow things to stabilise before the
       important data goes out. */
    span_log(&s->logging, SPAN_LOG_FLOW, "Tx - transmission_preamble_init()\n");
    s->tx.txbits = 16;
    s->tx.txptr = 0;
    s->tx.lastbit = complex_sig_set(TRAINING_SCALE(TRAINING_AMP), TRAINING_SCALE(0.0f));
    s->tx.current_modulator = V34_MODULATION_CC;
    s->tx.current_getbaud = get_transmission_preamble_baud;
    s->tx.stage = V34_TX_STAGE_INITIAL_PREAMBLE;
}
/*- End of function --------------------------------------------------------*/

static complex_sig_t get_info0_baud(v34_state_t *s)
{
    int bit;

    bit = get_data_bit(&s->tx);
    if (s->tx.txptr >= s->tx.txbits)
    {
        /* Are we at the initial stage, where A or B comes next, or at the retry
           stage, where we keep repeating INFO0 */
        if (s->tx.stage == V34_TX_STAGE_INFO0)
            initial_ab_not_ab_baud_init(s);
        else
            info0_baud_init(s);
        /*endif*/
    }
    /*endif*/
    if (bit)
        s->tx.lastbit.re = -s->tx.lastbit.re;
    /*endif*/
    return s->tx.lastbit;
}
/*- End of function --------------------------------------------------------*/

static void info0_baud_init(v34_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Tx - info0_baud_init()\n");
    s->tx.txbits = info0_sequence_tx(&s->tx);
    /* Round up to a whole number of bytes */
    s->tx.txbits = (s->tx.txbits + 7) & ~7;
    s->tx.txptr = 0;
    s->tx.lastbit = complex_sig_set(TRAINING_SCALE(TRAINING_AMP), TRAINING_SCALE(0.0f));
    s->tx.current_modulator = V34_MODULATION_CC;
    s->tx.stage = (s->tx.stage >= V34_TX_STAGE_INFO0)  ?  V34_TX_STAGE_INFO0_RETRY  :  V34_TX_STAGE_INFO0;
    s->tx.current_getbaud = get_info0_baud;
}
/*- End of function --------------------------------------------------------*/

static complex_sig_t get_initial_fdx_a_not_a_baud(v34_state_t *s)
{
    /* Answering side */
    switch (s->tx.stage)
    {
    case V34_TX_STAGE_INITIAL_A:
        /* Send pure tone for at least 50ms (V.34/11.2.1.2.1) */
        if (++s->tx.tone_duration == 30)
        {
            /* 50ms minimum A period has passed - accept an incoming INFO0c */
            s->tx.stage = V34_TX_STAGE_FIRST_A;
        }
        /*endif*/
        break;
    case V34_TX_STAGE_FIRST_A:
        /* Continue sending pure tone until we see an INFO0c message (V.34/11.2.1.2.3) */
        if (s->rx.received_event == V34_EVENT_INFO0_OK)
        {
            /* First reversal seen - send a phase reversal back */
            s->tx.lastbit.re = -s->tx.lastbit.re;
            s->tx.tone_duration = 1;
            s->tx.stage = V34_TX_STAGE_FIRST_NOT_A;
        }
        else if (s->rx.received_event == V34_EVENT_INFO0_BAD
                 ||
                 s->rx.received_event == V34_EVENT_TONE_SEEN)
        {
            /* Go back to sending INFO0a until we get a clean INFO0c */
            info0_baud_init(s);
        }
        /*endif*/
        break;
    case V34_TX_STAGE_FIRST_NOT_A:
        /* Send phase reversed pure tone until we see another phase reversal */
        if (s->rx.received_event == V34_EVENT_REVERSAL_1)
        {
            /* Second reversal seen - wait 40+=1ms */
            s->tx.tone_duration = 0;
            s->tx.stage = V34_TX_STAGE_FIRST_NOT_A_REVERSAL_SEEN;
        }
        /*endif*/
        break;
    case V34_TX_STAGE_FIRST_NOT_A_REVERSAL_SEEN:
        /* Continue sending phase reversed pure tone for 40+-1ms */
        if (++s->tx.tone_duration == 24)
        {
            /* 40ms has passed - send another reversal back */
            s->tx.lastbit.re = -s->tx.lastbit.re;
            s->tx.tone_duration = 0;
            s->tx.stage = V34_TX_STAGE_SECOND_A;
        }
        /*endif*/
        break;
    case V34_TX_STAGE_SECOND_A:
        /* Send phase reversed pure tone for 10ms */
        if (++s->tx.tone_duration == 6)
        {
            /* 10ms has passed - move on to sending L1/L2 */
            l1_l2_signal_init(s);
        }
        /*endif*/
        break;
    }
    /*endswitch*/
    return s->tx.lastbit;
}
/*- End of function --------------------------------------------------------*/

static complex_sig_t get_initial_fdx_b_not_b_baud(v34_state_t *s)
{
    /* Calling side */
    switch (s->tx.stage)
    {
    case V34_TX_STAGE_FIRST_B:
        /* Send pure tone (V.34/11.2.1.1.1) */
        if (s->rx.received_event == V34_EVENT_INFO0_OK)
        {
            s->tx.stage = V34_TX_STAGE_FIRST_B_INFO_SEEN;
        }
        else if (s->rx.received_event == V34_EVENT_INFO0_BAD
                 ||
                 s->rx.received_event == V34_EVENT_TONE_SEEN)
        {
            /* Go back to sending INFO0c until we get a clean INFO0a */
            info0_baud_init(s);
        }
        /*endif*/
        break;
    case V34_TX_STAGE_FIRST_B_INFO_SEEN:
        /* Continue sending pure tone (V.34/11.2.1.1.1) */
        if (s->rx.received_event == V34_EVENT_REVERSAL_1)
        {
            /* First reversal seen - continue sending pure tone for 40+-1ms */
            s->tx.tone_duration = 1;
            s->tx.stage = V34_TX_STAGE_FIRST_NOT_B_WAIT;
        }
        /*endif*/
        break;
    case V34_TX_STAGE_FIRST_NOT_B_WAIT:
        /* Continue sending pure tone for 40+-1ms (V.34/11.2.1.1.3) */
        if (++s->tx.tone_duration == 24)
        {
            /* 40ms has passed - send a phase reversal back */
            s->tx.lastbit.re = -s->tx.lastbit.re;
            s->tx.tone_duration = 1;
            s->tx.stage = V34_TX_STAGE_FIRST_NOT_B;
        }
        /*endif*/
        break;
    case V34_TX_STAGE_FIRST_NOT_B:
        /* Send phase reversed pure tone for 10ms (V.34/11.2.1.1.3) */
        if (++s->tx.tone_duration == 6)
        {
            /* 10ms has passed */
            /* Move on to sending silence */
            s->tx.tone_duration = 0;
            s->tx.stage = V34_TX_STAGE_FIRST_B_SILENCE;
        }
        /*endif*/
        break;
    case V34_TX_STAGE_FIRST_B_SILENCE:
        /* Send silence, as we wait for reversal (V.34/11.2.1.1.4) */
        if (s->rx.received_event == V34_EVENT_REVERSAL_1)
        {
            /* Second reversal seen. We now have the round trip timed */
            s->tx.tone_duration = 1;
            s->tx.stage = V34_TX_STAGE_FIRST_B_POST_REVERSAL_SILENCE;
        }
        else if (s->tx.tone_duration == (1200 - 30))
        {
            /* Timeout, as we have not received a round trip time indication after 2s */
        }
        /*endif*/
        return zero;
    case V34_TX_STAGE_FIRST_B_POST_REVERSAL_SILENCE:
        /* Send silence, as we wait for L2 (V.34/11.2.1.1.4) */
        if (s->rx.received_event == V34_EVENT_L2_SEEN
            ||
            ++s->tx.tone_duration >= 400)
        {
            /* L2 recognised */
            s->tx.lastbit.re = -s->tx.lastbit.re;
            s->tx.tone_duration = 1;
            s->tx.stage = V34_TX_STAGE_SECOND_B;
        }
        /*endif*/
        return zero;
    case V34_TX_STAGE_SECOND_B:
        /* Send pure tone (V.34/11.2.1.1.5) */
        if (++s->tx.tone_duration >= 100)
        //if (s->rx.received_event == V34_EVENT_REVERSAL_3)
        {
            /* Second reversal seen - continue sending pure tone for 40+-1ms */
            s->tx.tone_duration = 1;
            s->tx.stage = V34_TX_STAGE_SECOND_B_WAIT;
        }
        /*endif*/
        break;
    case V34_TX_STAGE_SECOND_B_WAIT:
        /* Continue sending pure tone for 40+-1ms (V.34/11.2.1.1.6) */
        if (++s->tx.tone_duration == 24)
        {
            /* 40ms has passed - send a phase reversal back */
            s->tx.lastbit.re = -s->tx.lastbit.re;
            s->tx.tone_duration = 1;
            s->tx.stage = V34_TX_STAGE_SECOND_NOT_B;
        }
        /*endif*/
        break;
    case V34_TX_STAGE_SECOND_NOT_B:
        /* Send phase reversed pure tone for 10ms (V.34/11.2.1.1.6) */
        if (++s->tx.tone_duration == 6)
        {
            /* 10ms has passed - move on to sending L1/L2 */
            s->tx.tone_duration = 0;
            l1_l2_signal_init(s);
        }
        /*endif*/
        break;
    }
    /*endswitch*/
    return s->tx.lastbit;
}
/*- End of function --------------------------------------------------------*/

static complex_sig_t get_initial_hdx_a_not_a_baud(v34_state_t *s)
{
    /* Answering side */
    switch (s->tx.stage)
    {
    case V34_TX_STAGE_HDX_INITIAL_A:
        /* Send pure tone (V.34/12.2.1.2.1) */
        if (++s->tx.tone_duration == 30)
        {
            /* 50ms minimum A period has passed - accept an incoming INFO0c */
            s->tx.stage = V34_TX_STAGE_HDX_FIRST_A;
        }
        /*endif*/
        break;
    case V34_TX_STAGE_HDX_FIRST_A:
        /* Continue sending pure tone until we see an INFO0c message (V.34/12.2.1.2.3) */
        if (s->rx.received_event == V34_EVENT_INFO0_OK)
        {
            /* First reversal seen - send a phase reversal back */
            s->tx.lastbit.re = -s->tx.lastbit.re;
            s->tx.tone_duration = 1;
            s->tx.stage = V34_TX_STAGE_HDX_FIRST_NOT_A;
        }
        else if (s->rx.received_event == V34_EVENT_INFO0_BAD
                 ||
                 s->rx.received_event == V34_EVENT_TONE_SEEN)
        {
            /* Go back to sending INFO0a until we get a clean INFO0c */
            info0_baud_init(s);
        }
        /*endif*/
        break;
    case V34_TX_STAGE_HDX_FIRST_NOT_A:
        /* Send phase reversed pure tone for 10ms (V.34/12.2.1.2.3) */
        if (++s->tx.tone_duration == 6)
        {
            /* 10ms has passed - send silence */
            s->tx.tone_duration = 0;
            s->tx.stage = V34_TX_STAGE_HDX_FIRST_A_SILENCE;
        }
        /*endif*/
        break;
    case V34_TX_STAGE_HDX_FIRST_A_SILENCE:
        /* Send silence, as we wait for L2 (V.34/12.2.1.2.3) */
        if (s->rx.received_event == V34_EVENT_L2_SEEN
            ||
            ++s->tx.tone_duration >= 400)
        {
            /* L2 recognised */
            s->tx.lastbit.re = -s->tx.lastbit.re;
            s->tx.tone_duration = 1;
            s->tx.stage = V34_TX_STAGE_HDX_SECOND_A;
        }
        /*endif*/
        return zero;
    case V34_TX_STAGE_HDX_SECOND_A:
        /* Send pure tone (V.34/12.2.1.2.5) */
        if (++s->tx.tone_duration >= 100)
        //if (s->rx.received_event == V34_EVENT_REVERSAL_2)
        {
            /* Second reversal seen - continue sending pure tone for 25ms */
            s->tx.lastbit.re = -s->tx.lastbit.re;
            s->tx.tone_duration = 1;
            s->tx.stage = V34_TX_STAGE_HDX_SECOND_A_WAIT;
        }
        /*endif*/
        break;
    case V34_TX_STAGE_HDX_SECOND_A_WAIT:
        /* Continue sending pure tone for 25ms (V.34/12.2.1.2.6) */
        if (++s->tx.tone_duration == 15)
        {
            /* 25ms has passed - send INFOh */
            s->tx.tone_duration = 0;
            infoh_baud_init(s);
        }
        /*endif*/
        break;
    }
    /*endswitch*/
    return s->tx.lastbit;
}
/*- End of function --------------------------------------------------------*/

static complex_sig_t get_initial_hdx_b_not_b_baud(v34_state_t *s)
{
    /* Calling side */
    switch (s->tx.stage)
    {
    case V34_TX_STAGE_HDX_FIRST_B:
        /* Send pure tone (V.34/12.2.1.1.1) */
        if (s->rx.received_event == V34_EVENT_INFO0_OK)
        {
            s->tx.stage = V34_TX_STAGE_HDX_FIRST_B_INFO_SEEN;
        }
        else if (s->rx.received_event == V34_EVENT_INFO0_BAD
                 ||
                 s->rx.received_event == V34_EVENT_TONE_SEEN)
        {
            /* Go back to sending INFO0c until we get a clean INFO0a */
            info0_baud_init(s);
        }
        /*endif*/
        break;
    case V34_TX_STAGE_HDX_FIRST_B_INFO_SEEN:
        /* Continue sending pure tone (V.34/12.2.1.1.1) */
        if (s->rx.received_event == V34_EVENT_REVERSAL_1)
        {
            /* First reversal seen - continue sending pure tone for 40+-1ms */
            s->tx.tone_duration = 1;
            s->tx.stage = V34_TX_STAGE_HDX_FIRST_NOT_B_WAIT;
        }
        /*endif*/
        break;
    case V34_TX_STAGE_HDX_FIRST_NOT_B_WAIT:
        /* Continue sending pure tone for 40+-10ms (V.34/12.2.1.1.3) */
        if (++s->tx.tone_duration == 24)
        {
            /* 40ms has passed - send a phase reversal back */
            s->tx.lastbit.re = -s->tx.lastbit.re;
            s->tx.tone_duration = 1;
            s->tx.stage = V34_TX_STAGE_HDX_FIRST_NOT_B;
        }
        /*endif*/
        break;
    case V34_TX_STAGE_HDX_FIRST_NOT_B:
        /* Send phase reversed pure tone for 10ms (V.34/12.2.1.1.3) */
        if (++s->tx.tone_duration == 6)
        {
            /* 10ms has passed */
            /* Move on to sending L1/L2 */
            s->tx.tone_duration = 0;
            l1_l2_signal_init(s);
        }
        /*endif*/
        break;
    }
    /*endswitch*/
    return s->tx.lastbit;
}
/*- End of function --------------------------------------------------------*/

static void initial_ab_not_ab_baud_init(v34_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Tx - initial_ab_not_ab_baud_init()\n");
    s->tx.tone_duration = 0;
    s->tx.current_modulator = V34_MODULATION_CC;
    s->tx.lastbit = complex_sig_set(TRAINING_SCALE(TRAINING_AMP), TRAINING_SCALE(0.0f));
    if (s->tx.duplex)
    {
        if (s->tx.calling_party)
        {
            s->tx.current_getbaud = get_initial_fdx_b_not_b_baud;
            s->tx.stage = V34_TX_STAGE_FIRST_B;
        }
        else
        {
            s->tx.current_getbaud = get_initial_fdx_a_not_a_baud;
            s->tx.stage = V34_TX_STAGE_INITIAL_A;
        }
        /*endif*/
    }
    else
    {
        if (s->tx.calling_party)
        {
            s->tx.current_getbaud = get_initial_hdx_b_not_b_baud;
            s->tx.stage = V34_TX_STAGE_HDX_FIRST_B;
        }
        else
        {
            s->tx.current_getbaud = get_initial_hdx_a_not_a_baud;
            s->tx.stage = V34_TX_STAGE_HDX_INITIAL_A;
        }
        /*endif*/
    }
    /*endif*/
    s->tx.persistence2 = 0;
}
/*- End of function --------------------------------------------------------*/

static int tx_l1_l2(v34_state_t *s, int16_t amp[], int max_len)
{
    int sample;

    /* This signal repeats every 160 samples, so we have the appropriate
       pattern stored, and we just scale and repeat it. We start 6dB above nominal
       power (L1) and then drop the amplitude to nominal power after the first 160ms
       (8 cycles) (L2). L2 should not last longer than 550ms + a round trip time. */
    /* This can occur between:
            !B and INFO1c for a FDX caller
            !B and B for a HDX caller
            A and A for a FDX answerer
            !A and A for a HDX answerer
     */
    for (sample = 0;  sample < max_len;  sample++)
    {
        amp[sample] = (int16_t) lfastrintf(line_probe_samples[s->tx.line_probe_step]*s->tx.line_probe_scaling);
        if (++s->tx.line_probe_step >= LINE_PROBE_SAMPLES)
        {
            s->tx.line_probe_step = 0;
            if (++s->tx.line_probe_cycles == 8)
            {
                /* Move to the L2 stage, by dropping 6dB */
                s->tx.line_probe_scaling *= 0.5f;
                s->tx.state = V34_TX_STAGE_L2;
            }
            else if (s->tx.line_probe_cycles == (8 + 20))
            {
                /* End of line probe sequence */
                if (s->tx.duplex)
                {
                    if (s->tx.calling_party)
                        info1_baud_init(s);
                    else
                        second_a_baud_init(s);
                    /*endif*/
                }
                else
                {
                    if (s->tx.calling_party)
                        second_b_baud_init(s);
                    else
                        second_a_baud_init(s);
                    /*endif*/
                }
                /*endif*/
                break;
            }
            /*endif*/
        }
        /*endif*/
    }
    /*endfor*/
    return sample;
}
/*- End of function --------------------------------------------------------*/

static void l1_l2_signal_init(v34_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Tx - l2_l2_signal_init()\n");
    s->tx.line_probe_step = 0;
    s->tx.line_probe_cycles = 0;
    s->tx.line_probe_scaling = 0.0008f*s->tx.gain;
    s->tx.current_modulator = V34_MODULATION_L1_L2;
    s->tx.state = V34_TX_STAGE_L1;
}
/*- End of function --------------------------------------------------------*/

static complex_sig_t get_second_a_baud(v34_state_t *s)
{
    switch (s->tx.stage)
    {
    case V34_TX_STAGE_POST_L2_A:
        /* Send pure tone for 50ms (V.34/11.2.1.2.6) */
        if (++s->tx.tone_duration == 30)
        {
            /* 50ms has passed - reverse */
            s->tx.lastbit.re = -s->tx.lastbit.re;
            s->tx.tone_duration = 0;
            s->tx.stage = V34_TX_STAGE_POST_L2_NOT_A;
        }
        /*endif*/
        break;
    case V34_TX_STAGE_POST_L2_NOT_A:
        /* Send phase reversed pure tone for 10ms (V.34/11.2.1.2.6) */
        if (++s->tx.tone_duration == 6)
        {
            /* 10ms has passed - change to silence */
            s->tx.tone_duration = 0;
            s->tx.stage = V34_TX_STAGE_A_SILENCE;
        }
        /*endif*/
        break;
    case V34_TX_STAGE_A_SILENCE:
        /* Send silence, as we wait for L2 (V.34/11.2.1.2.6) */
        if (s->rx.received_event == V34_EVENT_L2_SEEN
            ||
            ++s->tx.tone_duration >= 390)
        {
            /* 650ms has passed - wait for INFO1c message */
            s->tx.lastbit.re = -s->tx.lastbit.re;
            s->tx.tone_duration = 0;
            s->tx.stage = V34_TX_STAGE_PRE_INFO1_A;
        }
        /*endif*/
        return zero;
    case V34_TX_STAGE_PRE_INFO1_A:
        //if (s->rx.received_event == V34_EVENT_INFO1_OK)
        if (++s->tx.tone_duration == 180)
        {
            /* INFO1c received - send INFO1a */
            s->tx.tone_duration = 0;
            info1_baud_init(s);
        }
        else if (s->rx.received_event == V34_EVENT_INFO1_BAD
                 ||
                 s->rx.received_event == V34_EVENT_TONE_SEEN)
        {
        }
        else if (s->tx.tone_duration == 1200)
        {
            /* Timeout, as we have not received INFO1c after 2s */
        }
        /*endif*/
        break;
    }
    /*endswitch*/
    return s->tx.lastbit;
}
/*- End of function --------------------------------------------------------*/

static void second_a_baud_init(v34_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Tx - second_a_baud_init()\n");
    s->tx.tone_duration = 0;
    s->tx.current_modulator = V34_MODULATION_CC;
    s->tx.lastbit = complex_sig_set(TRAINING_SCALE(TRAINING_AMP), TRAINING_SCALE(0.0f));
    s->tx.stage = V34_TX_STAGE_POST_L2_A;
    s->tx.current_getbaud = get_second_a_baud;
}
/*- End of function --------------------------------------------------------*/

static complex_sig_t get_second_b_baud(v34_state_t *s)
{
    switch (s->tx.stage)
    {
    case V34_TX_STAGE_HDX_POST_L2_B:
        /* Send pure tone until we receive INFOh (V.34/12.2.1.1.4) */
        if (s->rx.received_event == V34_EVENT_INFOH_OK)
        {
            s->tx.tone_duration = 0;
            s->tx.stage = V34_TX_STAGE_HDX_POST_L2_SILENCE;
        }
        else if (s->rx.received_event == V34_EVENT_INFO0_BAD
                 ||
                 s->rx.received_event == V34_EVENT_TONE_SEEN)
        {
        }
        else if (++s->tx.tone_duration == 1200)
        {
            /* Timeout, as we have not received INFOh after 2s */
        }
        /*endif*/
        break;
    case V34_TX_STAGE_HDX_POST_L2_SILENCE:
        /* Send silence for 75ms (V.34/12.3.1.1) */
        if (++s->tx.tone_duration == 45)
        {
            s->tx.tone_duration = 0;
        }
        /*endif*/
        return zero;
    }
    /*endswitch*/
    return s->tx.lastbit;
}
/*- End of function --------------------------------------------------------*/

static void second_b_baud_init(v34_state_t *s)
{
    /* This is for half-duplex */
    span_log(&s->logging, SPAN_LOG_FLOW, "Tx - second_b_baud_init()\n");
    s->tx.tone_duration = 0;
    s->tx.current_modulator = V34_MODULATION_CC;
    s->tx.lastbit = complex_sig_set(TRAINING_SCALE(TRAINING_AMP), TRAINING_SCALE(0.0f));
    s->tx.stage = V34_TX_STAGE_HDX_POST_L2_B;
    s->tx.current_getbaud = get_second_b_baud;
}
/*- End of function --------------------------------------------------------*/

static complex_sig_t get_infoh_baud(v34_state_t *s)
{
    int bit;

    bit = get_data_bit(&s->tx);
    if (s->tx.txptr >= s->tx.txbits)
    {
        if (s->tx.calling_party)
            tx_silence_init(s, 30000);
        else
            s_not_s_baud_init(s);
        /*endif*/
    }
    /*endif*/
    if (bit)
        s->tx.lastbit.re = -s->tx.lastbit.re;
    /*endif*/
    return s->tx.lastbit;
}
/*- End of function --------------------------------------------------------*/

static void infoh_baud_init(v34_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Tx - infoh_baud_init()\n");
    prepare_infoh(s);
    s->tx.txbits = infoh_sequence_tx(&s->tx, &s->tx.infoh);
    s->tx.txbits += 8;
    s->tx.txptr = 0;
#if 0
#if defined(SPANDSP_USE_FIXED_POINT)
    cvec_zeroi16(s->tx.rrc_filter, sizeof(s->tx.rrc_filter)/sizeof(s->tx.rrc_filter[0]));
#else
    cvec_zerof(s->tx.rrc_filter, sizeof(s->tx.rrc_filter)/sizeof(s->tx.rrc_filter[0]));
#endif
    s->tx.lastbit = complex_sig_set(TRAINING_SCALE(0.0f), TRAINING_SCALE(0.0f));
    s->tx.rrc_filter_step = 0;
    s->tx.baud_phase = 0;
#endif

    s->tx.lastbit = complex_sig_set(TRAINING_SCALE(TRAINING_AMP), TRAINING_SCALE(0.0f));
    /* Round up to a whole number of bytes */
    s->tx.txbits = (s->tx.txbits + 7) & ~7;
    s->tx.current_modulator = V34_MODULATION_CC;
    s->tx.current_getbaud = get_infoh_baud;
}
/*- End of function --------------------------------------------------------*/

static complex_sig_t get_info1_baud(v34_state_t *s)
{
    int bit;

    bit = get_data_bit(&s->tx);
    if (s->tx.txptr >= s->tx.txbits)
    {
        if (s->tx.calling_party)
        {
printf("info 1 Tx silence\n");
            tx_silence_init(s, 30000);
        }
        else
        {
printf("info 1 Tx S !S\n");
            s_not_s_baud_init(s);
        }
        /*endif*/
    }
    /*endif*/
    if (bit)
        s->tx.lastbit.re = -s->tx.lastbit.re;
    /*endif*/
    return s->tx.lastbit;
}
/*- End of function --------------------------------------------------------*/

static void info1_baud_init(v34_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Tx - info1_baud_init()\n");
    if (s->tx.calling_party)
    {
        prepare_info1c(s);
        s->tx.txbits = info1c_sequence_tx(&s->tx, &s->tx.info1c);
        s->tx.txbits += 8;
    }
    else
    {
        prepare_info1a(s);
        s->tx.txbits = info1a_sequence_tx(&s->tx, &s->tx.info1a);
    }
    /*endif*/
    /* Round up to a whole number of bytes */
    s->tx.txbits = (s->tx.txbits + 7) & ~7;
    s->tx.txptr = 0;
#if 0
#if defined(SPANDSP_USE_FIXED_POINT)
    cvec_zeroi16(s->tx.rrc_filter, sizeof(s->tx.rrc_filter)/sizeof(s->tx.rrc_filter[0]));
#else
    cvec_zerof(s->tx.rrc_filter, sizeof(s->tx.rrc_filter)/sizeof(s->tx.rrc_filter[0]));
#endif
    s->tx.lastbit = complex_sig_set(TRAINING_SCALE(0.0f), TRAINING_SCALE(0.0f));
    s->tx.rrc_filter_step = 0;
    s->tx.baud_phase = 0;
#endif

    s->tx.lastbit = complex_sig_set(TRAINING_SCALE(TRAINING_AMP), TRAINING_SCALE(0.0f));
    s->tx.current_modulator = V34_MODULATION_CC;
    s->tx.stage = V34_TX_STAGE_INFO1;
    s->tx.current_getbaud = get_info1_baud;
}
/*- End of function --------------------------------------------------------*/

static complex_sig_t get_s_not_s_baud(v34_state_t *s)
{
#if defined(SPANDSP_USE_FIXED_POINT)
    int16_t x;
#else
    float x;
#endif

    switch (s->tx.stage)
    {
    case V34_TX_STAGE_FIRST_S:
        if (++s->tx.tone_duration < 180)
            return zero;
        /*endif*/
        if (s->tx.tone_duration == (128 + 180))
        {
            s->tx.lastbit.re = -s->tx.lastbit.re;
            s->tx.stage = V34_TX_STAGE_FIRST_NOT_S;
            s->tx.tone_duration = 0;
        }
        /*endif*/
        break;
    case V34_TX_STAGE_FIRST_NOT_S:
        if (++s->tx.tone_duration == 16)
        {
            s->tx.lastbit.re = -s->tx.lastbit.re;
            if (s->tx.duplex  &&  s->tx.info1c.md)
                s->tx.stage = V34_TX_STAGE_SECOND_S;
            else
                pp_baud_init(s);
            /*endif*/
            s->tx.tone_duration = 0;
        }
        /*endif*/
        break;
    case V34_TX_STAGE_MD:
        /* This is where MD would go */
        break;
    case V34_TX_STAGE_SECOND_S:
        if (++s->tx.tone_duration == 128)
        {
            s->tx.lastbit.re = -s->tx.lastbit.re;
            s->tx.stage = V34_TX_STAGE_SECOND_NOT_S;
            s->tx.tone_duration = 0;
        }
        /*endif*/
        break;
    case V34_TX_STAGE_SECOND_NOT_S:
        if (++s->tx.tone_duration == 16)
            pp_baud_init(s);
        /*endif*/
        break;
    }
    /*endswitch*/
    x = s->tx.lastbit.re;
    s->tx.lastbit.re = s->tx.lastbit.im;
    s->tx.lastbit.im = x;
    return s->tx.lastbit;
}
/*- End of function --------------------------------------------------------*/

static void s_not_s_baud_init(v34_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Tx - s_not_s_baud_init()\n");
    s->tx.lastbit = complex_sig_set(TRAINING_SCALE(TRAINING_AMP), TRAINING_SCALE(0.0f));
    s->tx.tone_duration = 0;
    s->tx.current_modulator = V34_MODULATION_V34;
    s->tx.stage = V34_TX_STAGE_FIRST_S;
    s->tx.current_getbaud = get_s_not_s_baud;
}
/*- End of function --------------------------------------------------------*/

static complex_sig_t get_pp_baud(v34_state_t *s)
{
    complex_sig_t x;
    int i;

    /* The 48 symbol PP signal, which is repeated 6 times, to make a 288 symbol sequence */
    /* See V.34/10.1.3.6 */
    i = s->tx.tone_duration%48;
    if (++s->tx.tone_duration == PP_SYMBOLS*PP_REPEATS)
        trn_baud_init(s);
    /*endif*/
    x = pp_symbols[i];
    x.re *= TRAINING_SCALE(TRAINING_AMP);
    x.im *= TRAINING_SCALE(TRAINING_AMP);
    return x;
}
/*- End of function --------------------------------------------------------*/

static void pp_baud_init(v34_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Tx - pp_baud_init()\n");
    s->tx.tone_duration = 0;
    s->tx.current_getbaud = get_pp_baud;
}
/*- End of function --------------------------------------------------------*/

static complex_sig_t get_trn_baud(v34_state_t *s)
{
    static const uint16_t j_pattern[2] =
    {
        0x8990, /* 4 point constellation */
        0x89B0  /* 16 point constellation */
    };
    int bit;

    /* See V.34/10.1.3.8 */
    bit = 0;
    switch (s->tx.stage)
    {
    case V34_TX_STAGE_TRN:
        /* Send the TRN signal */
        bit = scramble(&s->tx, 1);
        bit = (scramble(&s->tx, 1) << 1) | bit;
        /* In half-duplex modem the length of the training comes from the INFOh message, in 35ms increments */
        if ((!s->tx.duplex  &&  ++s->tx.tone_duration >= s->rx.infoh.length_of_trn*35*s->rx.infoh.baud_rate/1000)
            ||
            (s->tx.duplex  &&  ++s->tx.tone_duration >= 512))
        {
            s->tx.stage = V34_TX_STAGE_J;
            s->tx.persistence2 = j_pattern[0];
            s->tx.tone_duration = 0;
        }
        /*endif*/
        break;
    case V34_TX_STAGE_J:
        /* Send the terminal J signal */
        bit = scramble(&s->tx, (s->tx.persistence2 & 1));
        s->tx.persistence2 >>= 1;
        bit = (scramble(&s->tx, (s->tx.persistence2 & 1)) << 1) | bit;
        s->tx.persistence2 >>= 1;
        if (++s->tx.tone_duration >= 16)
        {
            if (s->tx.duplex)
            {
                if (s->rx.received_event == V34_EVENT_S)
                {
                    if (s->tx.calling_party)
                    {
                        /* Change to J' */
                        s->tx.stage = V34_TX_STAGE_J_DASHED;
                        s->tx.persistence2 = j_pattern[0];
                        s->tx.tone_duration = 0;
                    }
                    else
                    {
                        /* Send silence */
                    }
                    /*endif*/
                }
                else
                {
                    /* Continue with repeats of J */
                    s->tx.persistence2 = j_pattern[0];
                    s->tx.tone_duration = 0;
                }
                /*endif*/
            }
            else
            {
                mp_or_mph_baud_init(s);
            }
            /*endif*/
        }
        /*endif*/
        break;
    case V34_TX_STAGE_J_DASHED:
        /* Send J' */
        bit = scramble(&s->tx, (s->tx.persistence2 & 1));
        s->tx.persistence2 >>= 1;
        bit = (scramble(&s->tx, (s->tx.persistence2 & 1)) << 1) | bit;
        s->tx.persistence2 >>= 1;
        if (++s->tx.tone_duration >= 16)
        {
        }
        /*endif*/
        break;
    }
    /*endswitch*/
    return training_constellation_4[bit];
}
/*- End of function --------------------------------------------------------*/

static void trn_baud_init(v34_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Tx - trn_baud_init()\n");
    s->tx.tone_duration = 0;
    s->tx.stage = V34_TX_STAGE_TRN;
    s->tx.current_getbaud = get_trn_baud;
}
/*- End of function --------------------------------------------------------*/

static complex_sig_t get_mp_or_mph_baud(v34_state_t *s)
{
    int bit;

    bit = scramble(&s->tx, get_data_bit(&s->tx));
    bit = (scramble(&s->tx, get_data_bit(&s->tx)) << 1) | bit;
    if (s->tx.txptr >= s->tx.txbits)
    {
        if (1)
        {
            if (s->tx.duplex)
            {
                /* See if we need to set the acknowledge bit, so MP becomes MP' */
                if (1)
                {
                    s->tx.mp.mp_acknowledged = 1;
                    /* We need to rebuild the message we send */
                    s->tx.txbits = mp_sequence_tx(&s->tx, &s->tx.mp);
                }
                /*endif*/
            }
            /*endif*/
            /* Restart the message */
            s->tx.txptr = 0;
        }
        else
        {
            e_baud_init(s);
        }
        /*endif*/
    }
    /*endif*/
    s->tx.diff = (s->tx.diff + bit) & 3;
    return training_constellation_4[s->tx.diff];
}
/*- End of function --------------------------------------------------------*/

static void mp_or_mph_baud_init(v34_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Tx - mp_baud_init()\n");
    s->tx.current_modulator = V34_MODULATION_V34;
    if (s->tx.duplex)
    {
        s->tx.txbits = mp_sequence_tx(&s->tx, &s->tx.mp);
        s->tx.stage = V34_TX_STAGE_MP;
    }
    else
    {
        s->tx.txbits = mph_sequence_tx(&s->tx, &s->tx.mph);
        s->tx.stage = V34_TX_STAGE_HDX_MPH;
    }
    /*endif*/
    s->tx.txptr = 0;
    s->tx.current_getbaud = get_mp_or_mph_baud;
}
/*- End of function --------------------------------------------------------*/

static complex_sig_t get_e_baud(v34_state_t *s)
{
    static const uint16_t e_pattern[2] =
    {
        0x8990, /* 4 point constellation */
        0x89B0  /* 16 point constellation */
    };
    int bit;

    bit = (e_pattern[0] >> s->tx.tone_duration) & 1;
    if (++s->tx.tone_duration == 16)
    {
        //if (s->tx.duplex)
            /* CC comes next */
        //else
            /* B1 comes next */
        ///*endif*/
    }
    /*endif*/
    return training_constellation_4[bit];
}
/*- End of function --------------------------------------------------------*/

static void e_baud_init(v34_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Tx - e_baud_init()\n");
    s->tx.tone_duration = 0;
    s->tx.stage = V34_TX_STAGE_HDX_E;
    s->tx.current_getbaud = get_e_baud;
}
/*- End of function --------------------------------------------------------*/

static complex_sig_t get_pph_baud(v34_state_t *s)
{
    int i;

    /* This is the beginning of half-duplex control channel restart */
    /* The 8 symbol PPh signal, which is repeated 4 times, to make a 32 symbol sequence */
    /* See V.34/10.2.4.5 */
    i = s->tx.tone_duration & 0x7;
    if (++s->tx.tone_duration == PPH_SYMBOLS*PPH_REPEATS)
        second_alt_baud_init(s);
    /*endif*/
    return pph_symbols[i];
}
/*- End of function --------------------------------------------------------*/

static void pph_baud_init(v34_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Tx - pph_baud_init()\n");
    s->tx.tone_duration = 0;
    s->tx.current_modulator = V34_MODULATION_CC;
    s->tx.stage = V34_TX_STAGE_HDX_PPH;
    s->tx.current_getbaud = get_pph_baud;
}
/*- End of function --------------------------------------------------------*/

static complex_sig_t get_second_alt_baud(v34_state_t *s)
{
    int bit;

    /* Signal ALT is transmitted using the control channel modulation with the differential
       encoder enabled and consists of scrambled alternations of binary 0 and 1 at 1200 bit/s.
       The initial state of the scrambler shall be all zeroes. */
    /* See V.34/10.2.4.2 */
    bit = scramble(&s->tx, 0);
    bit = (scramble(&s->tx, 1) << 1) | bit;
    s->tx.diff = (s->tx.diff + bit) & 3;
    if (++s->tx.tone_duration >= 16)
    {
        /* We have reached the absolute minimum allowed for the duration of ALT */
        if (s->tx.tone_duration >= 120)
        {
            /* TODO: Should allow for early termination. */
            if (1)
            {
                /* Control channel training */
                mp_or_mph_baud_init(s);
            }
            else
            {
                /* Control channel resynchronisation */
                e_baud_init(s);
            }
            /*endif*/
        }
        /*endif*/
    }
    /*endif*/
    return training_constellation_4[s->tx.diff];
}
/*- End of function --------------------------------------------------------*/

static void second_alt_baud_init(v34_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Tx - second_alt_baud_init()\n");
    s->tx.tone_duration = 0;
    s->tx.current_modulator = V34_MODULATION_V34;
    s->tx.scramble_reg = 0;
    s->tx.diff = 0;
    s->tx.stage = V34_TX_STAGE_HDX_SECOND_ALT;
    s->tx.current_getbaud = get_second_alt_baud;
}
/*- End of function --------------------------------------------------------*/

static complex_sig_t get_first_alt_baud(v34_state_t *s)
{
    int bit;

    /* Signal ALT is transmitted using the control channel modulation with the differential
       encoder enabled and consists of scrambled alternations of binary 0 and 1 at 1200 bit/s.
       The initial state of the scrambler shall be all zeroes. */
    /* See V.34/10.2.4.2 */
    bit = scramble(&s->tx, 0);
    bit = (scramble(&s->tx, 1) << 1) | bit;
    s->tx.diff = (s->tx.diff + bit) & 3;
    if (++s->tx.tone_duration >= 16)
    {
        /* We have reached the absolute minimum allowed for the duration of ALT */
        if (s->tx.tone_duration >= 120)
        {
            /* TODO: Should allow for early termination. */
            /* Control channel training */
            pph_baud_init(s);
        }
        /*endif*/
    }
    /*endif*/
    return training_constellation_4[s->tx.diff];
}
/*- End of function --------------------------------------------------------*/

static void first_alt_baud_init(v34_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Tx - first_alt_baud_init()\n");
    s->tx.tone_duration = 0;
    s->tx.current_modulator = V34_MODULATION_V34;
    s->tx.scramble_reg = 0;
    s->tx.diff = 0;
    s->tx.stage = V34_TX_STAGE_HDX_FIRST_ALT;
    s->tx.current_getbaud = get_first_alt_baud;
}
/*- End of function --------------------------------------------------------*/

static complex_sig_t get_sh_baud(v34_state_t *s)
{
#define SH_PLUS_NO_SH_SYMBOLS       32
    static const uint8_t sh_plus_not_sh[SH_PLUS_NO_SH_SYMBOLS] =
    {
        2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1,     /* Sh */
        0, 3, 0, 3, 0, 3, 0, 3                                                      /* !Sh */
    };
    int i;

    /* See V.34/10.2.3.3 */
    i = s->tx.tone_duration;
    if (++s->tx.tone_duration == SH_PLUS_NO_SH_SYMBOLS)
    {
        /* The Sh and !Sh have finished */
        first_alt_baud_init(s);
    }
    /*endif*/
    return training_constellation_4[sh_plus_not_sh[i]];
}
/*- End of function --------------------------------------------------------*/

static void sh_baud_init(v34_state_t *s)
{
    /* This is the beginning of half-duplex control channel startup */
    span_log(&s->logging, SPAN_LOG_FLOW, "Tx - sh_baud_init()\n");
    s->tx.lastbit = complex_sig_set(TRAINING_SCALE(TRAINING_AMP), TRAINING_SCALE(0.0f));
    s->tx.tone_duration = 0;
    s->tx.current_modulator = V34_MODULATION_V34;
    s->tx.stage = V34_TX_STAGE_HDX_SH;
    s->tx.current_getbaud = get_sh_baud;
}
/*- End of function --------------------------------------------------------*/

#if defined(SPANDSP_USE_FIXED_POINT)
static __inline__ uint32_t dist_sq(const complexi_t *x, const complexi_t *y)
{
    return (x->re - y->re)*(x->re - y->re) + (x->im - y->im)*(x->im - y->im);
}
/*- End of function --------------------------------------------------------*/
#else
static __inline__ float dist_sq(const complexf_t *x, const complexf_t *y)
{
    return (x->re - y->re)*(x->re - y->re) + (x->im - y->im)*(x->im - y->im);
}
/*- End of function --------------------------------------------------------*/
#endif

static __inline__ complex_sig_t training_get(v34_tx_state_t *s)
{
    return zero;
}
/*- End of function --------------------------------------------------------*/

static __inline__ complex_sig_t connect_sequence_get(v34_tx_state_t *s)
{
    return zero;
}
/*- End of function --------------------------------------------------------*/

static int tx_v34_modulation(v34_state_t *s, int16_t amp[], int max_len)
{
#if defined(SPANDSP_USE_FIXED_POINT)
    complexi16_t v;
    complexi32_t x;
    complexi_t z;
#else
    complexf_t v;
    complexf_t x;
    complexf_t z;
#endif
    const tx_shaper_t *shaper;
    int num;
    int den;
    int i;
    int sample;

    /* The V.34 modulator. */
printf("ZZZ baud rate %d\n", s->tx.baud_rate);
    num = s->tx.parms.samples_per_symbol_numerator;
    den = s->tx.parms.samples_per_symbol_denominator;
    shaper = v34_tx_shapers[s->tx.baud_rate];
    for (sample = 0;  sample < max_len;  sample++)
    {
        if ((s->tx.baud_phase += den) >= num)
        {
            s->tx.baud_phase -= num;
            v = s->tx.current_getbaud(s);
            s->tx.rrc_filter_re[s->tx.rrc_filter_step] = v.re;
            s->tx.rrc_filter_im[s->tx.rrc_filter_step] = v.im;
printf("V.34 baud %10.5f %10.5f - %10.5f\n", s->tx.rrc_filter_re[s->tx.rrc_filter_step], s->tx.rrc_filter_im[s->tx.rrc_filter_step], s->tx.gain);
            if (++s->tx.rrc_filter_step >= V34_TX_FILTER_STEPS)
                s->tx.rrc_filter_step = 0;
            /*endif*/
        }
        /*endif*/
        /* Root raised cosine pulse shaping at baseband */
#if defined(SPANDSP_USE_FIXED_POINT)
        x = complex_seti32(0, 0);
        for (i = 0;  i < V34_TX_FILTER_STEPS;  i++)
        {
            x.re += (int32_t) shaper[num - 1 - s->tx.baud_phase][i]*(int32_t) s->tx.rrc_filter[i + s->tx.rrc_filter_step].re;
            x.im += (int32_t) shaper[num - 1 - s->tx.baud_phase][i]*(int32_t) s->tx.rrc_filter[i + s->tx.rrc_filter_step].im;
        }
        /*endfor*/
        /* Now create and modulate the carrier */
        x.re >>= 4;
        x.im >>= 4;
        z = dds_complexi(&(s->tx.carrier_phase), s->tx.v34_carrier_phase_rate);
        /* Don't bother saturating. We should never clip. */
        i = (x.re*z.re - x.im*z.im) >> 15;
        amp[sample] = (int16_t) ((i*s->tx.gain) >> 15);
#else
        x = zero;
        for (i = 0;  i < V34_TX_FILTER_STEPS;  i++)
        {
            x.re += shaper[num - 1 - s->tx.baud_phase][i]*s->tx.rrc_filter_re[i + s->tx.rrc_filter_step];
            x.im += shaper[num - 1 - s->tx.baud_phase][i]*s->tx.rrc_filter_im[i + s->tx.rrc_filter_step];
        }
        /*endfor*/
        /* Now create and modulate the carrier */
        z = dds_complexf(&(s->tx.carrier_phase), s->tx.v34_carrier_phase_rate);
        /* Don't bother saturating. We should never clip. */
        amp[sample] = (int16_t) lfastrintf((x.re*z.re - x.im*z.im)*s->tx.gain);
#endif
printf("V.34 sample %d\n", amp[sample]);
    }
    /*endfor*/
    return sample;
}
/*- End of function --------------------------------------------------------*/

static int tx_cc_modulation(v34_state_t *s, int16_t amp[], int max_len)
{
#if defined(SPANDSP_USE_FIXED_POINT)
    complexi16_t v;
    complexi32_t x;
    complexi_t z;
    int16_t iamp;
#else
    complexf_t v;
    complexf_t x;
    complexf_t z;
    float famp;
#endif
    int sample;

    /* The V.22bis like split band modulator for configuration data and the
       half-duplex control channel. */
    for (sample = 0;  sample < max_len;  sample++)
    {
        if ((s->tx.baud_phase += 3) >= 40)
        {
            s->tx.baud_phase -= 40;
            v = s->tx.current_getbaud(s);
            s->tx.rrc_filter_re[s->tx.rrc_filter_step] = v.re;
            s->tx.rrc_filter_im[s->tx.rrc_filter_step] = v.im;
printf("CC baud %10.5f %10.5f - %10.5f\n",
       s->tx.rrc_filter_re[s->tx.rrc_filter_step],
       s->tx.rrc_filter_im[s->tx.rrc_filter_step],
       s->tx.gain);
            if (++s->tx.rrc_filter_step >= V34_INFO_TX_FILTER_STEPS)
                s->tx.rrc_filter_step = 0;
            /*endif*/
        }
        /*endif*/
        /* Root raised cosine pulse shaping at baseband */
#if defined(SPANDSP_USE_FIXED_POINT)
        x.re = vec_circular_dot_prodi16(s->tx.rrc_filter_re, tx_pulseshaper[TX_PULSESHAPER_COEFF_SETS - 1 - s->tx.baud_phase], V34_INFO_TX_FILTER_STEPS, s->tx.rrc_filter_step) >> 4;
        x.im = vec_circular_dot_prodi16(s->tx.rrc_filter_im, tx_pulseshaper[TX_PULSESHAPER_COEFF_SETS - 1 - s->tx.baud_phase], V34_INFO_TX_FILTER_STEPS, s->tx.rrc_filter_step) >> 4;
        /* Now create and modulate the carrier */
        z = dds_complexi(&s->tx.carrier_phase, s->tx.cc_carrier_phase_rate);
        /* Don't bother saturating. We should never clip. */
        iamp = (x.re*z.re - x.im*z.im) >> 15;
        if (s->tx.guard_phase_rate  &&  (s->tx.rrc_filter[s->tx.rrc_filter_step].re != 0  ||  s->tx.rrc_filter[s->tx.rrc_filter_step].im != 0))
        {
            /* Add the guard tone */
            iamp += dds_mod(&s->tx.guard_phase, s->tx.guard_phase_rate, s->tx.cjo, 0);
        }
        /*endif*/
        amp[sample] = (int16_t) (((int32_t) iamp*s->tx.gain) >> 15);
#else
        x.re = vec_circular_dot_prodf(s->tx.rrc_filter_re, tx_pulseshaper[TX_PULSESHAPER_COEFF_SETS - 1 - s->tx.baud_phase], V34_INFO_TX_FILTER_STEPS, s->tx.rrc_filter_step);
        x.im = vec_circular_dot_prodf(s->tx.rrc_filter_im, tx_pulseshaper[TX_PULSESHAPER_COEFF_SETS - 1 - s->tx.baud_phase], V34_INFO_TX_FILTER_STEPS, s->tx.rrc_filter_step);
        /* Now create and modulate the carrier */
        z = dds_complexf(&s->tx.carrier_phase, s->tx.cc_carrier_phase_rate);
        famp = x.re*z.re - x.im*z.im;
        if (s->tx.guard_phase_rate  &&  (s->tx.rrc_filter_re[s->tx.rrc_filter_step] != 0.0f  ||  s->tx.rrc_filter_im[s->tx.rrc_filter_step] != 0.0f))
        {
            /* Add the guard tone */
            famp += dds_modf(&s->tx.guard_phase, s->tx.guard_phase_rate, s->tx.guard_level, 0);
        }
        /*endif*/
        /* Don't bother saturating. We should never clip. */
        amp[sample] = (int16_t) lfastrintf(famp*s->tx.gain);
#endif
printf("CC sample %d\n", amp[sample]);
    }
    return sample;
}
/*- End of function --------------------------------------------------------*/

static int tx_silence(v34_state_t *s, int16_t amp[], int max_len)
{
    if (s->tx.tone_duration <= max_len)
    {
        max_len = s->tx.tone_duration;
        s->tx.tone_duration = 0;
        if (s->tx.training_stage == 0x100)
        {
            s->tx.training_stage = 0x101;
            transmission_preamble_init(s);
        }
        /*endif*/
    }
    else
    {
        s->tx.tone_duration -= max_len;
    }
    /*endif*/
    vec_zeroi16(amp, max_len);
    return max_len;
}
/*- End of function --------------------------------------------------------*/

static void tx_silence_init(v34_state_t *s, int duration)
{
    s->tx.tone_duration = milliseconds_to_samples(duration);
    s->tx.current_modulator = V34_MODULATION_SILENCE;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v34_tx(v34_state_t *s, int16_t amp[], int max_len)
{
    int len;
    int lenx;

    len = 0;
    lenx = -1;
    do
    {
        switch (s->tx.current_modulator)
        {
        case V34_MODULATION_V34:
            lenx = tx_v34_modulation(s, &amp[len], max_len - len);
            break;
        case V34_MODULATION_CC:
            lenx = tx_cc_modulation(s, &amp[len], max_len - len);
            break;
        case V34_MODULATION_L1_L2:
            lenx = tx_l1_l2(s, &amp[len], max_len - len);
            break;
        case V34_MODULATION_SILENCE:
            lenx = tx_silence(s, &amp[len], max_len - len);
            break;
        }
        /*endswitch*/
        len += lenx;
        /* Add step by step, so each segment is seen up to date */
        s->tx.sample_time += lenx;
    }
    while (lenx > 0  &&  len < max_len);
    /* If the transmission is short, this should be the end of operation of the modem,
       so we don't really need to worry about the residue and keeping the sample time
       current. */
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v34_tx_power(v34_state_t *s, float power)
{
    /* The constellation design seems to keep the average power the same, regardless
       of which bit rate is in use. */
#if defined(SPANDSP_USE_FIXED_POINT)
    s->tx.gain = 0.223f*db_to_amplitude_ratio(power - DBM0_MAX_SINE_POWER)*16.0f*(32767.0f/30672.52f)*32768.0f/TX_PULSESHAPER_GAIN;
#else
    s->tx.gain = 0.223f*db_to_amplitude_ratio(power - DBM0_MAX_SINE_POWER)*32768.0f/TX_PULSESHAPER_GAIN;
#endif
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v34_set_get_bit(v34_state_t *s, span_get_bit_func_t get_bit, void *user_data)
{
    if (s->tx.get_bit == s->tx.current_get_bit)
        s->tx.current_get_bit = get_bit;
    /*endif*/
    s->tx.get_bit = get_bit;
    s->tx.get_bit_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v34_set_get_aux_bit(v34_state_t *s, span_get_bit_func_t get_bit, void *user_data)
{
    s->tx.get_aux_bit = get_bit;
    s->tx.get_aux_bit_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) v34_get_logging_state(v34_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

void v34_set_working_parameters(v34_parameters_t *s, int baud_rate, int bit_rate, int expanded)
{
    /* This should be one of the normal V.34 modes. Not a control channel mode. */
    s->bit_rate = ((bit_rate >> 1) + 1)*2400 + (bit_rate & 1)*200;

    s->b = baud_rate_parameters[baud_rate].mappings[bit_rate].b;
    /* V.34/9.2 */
    if (s->b <= 12)
    {
        /* There are so few bits per mapping frame, that there are only I bits */
        s->k = 0;
        s->q = 0;
    }
    else
    {
        /* We have some K bits and maybe some Q bits */
        /* The baseline for K is the total bits less the I bits */
        s->k = s->b - 12;
        s->q = 0;
        /* If there are too many k bits, we need to trade some of them for
           uncoded Q bits, until the number of K bits is in range. We add
           Q bits in groups of 8, as the rule is each of the Q bit chunks
           in the 8 2D symbols of a mapping frame is the same size. */
        while (s->k >= 32)
        {
            s->k -= 8;
            s->q++;
        }
        /*endwhile*/
    }
    /*endif*/
    s->q_mask = ((1 << s->q) - 1);

    /* Calculating m, as described in V.34/9.2, doesn't always match the values in
       V.34/Table 10, so we use a table, to ensure an exact match. */
    s->m = baud_rate_parameters[baud_rate].mappings[bit_rate].m[(expanded)  ?  1  :  0];

    /* l is easy to calculate from m. We don't need to get it from a table, as
       shown in V.34/Table 10. */
    s->l = 4*s->m*(1 << s->q);
    s->j = baud_rate_parameters[baud_rate].j;
    s->p = baud_rate_parameters[baud_rate].p;
    /* We don't need to use a table entry for w. It is trivial to calculate it from j */
    s->w = (bit_rate & 1)  ?  (15 - s->j)  :  0;
    /* V.34/8.2 */
    s->r = (s->bit_rate*28)/(s->j*100) - (s->b - 1)*s->p;
    
    s->max_bit_rate_code = baud_rate_parameters[baud_rate].max_bit_rate_code;
    /* The numerator of the number of samples per symbol ratio. */
    s->samples_per_symbol_numerator = baud_rate_parameters[baud_rate].samples_per_symbol_numerator;
    /* The denominator of the number of samples per symbol ratio. */
    s->samples_per_symbol_denominator = baud_rate_parameters[baud_rate].samples_per_symbol_denominator;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v34_get_current_bit_rate(v34_state_t *s)
{
    return s->bit_rate;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v34_half_duplex_change_mode(v34_state_t *s, int mode)
{
    switch (mode)
    {
    case V34_HALF_DUPLEX_SOURCE:
    case V34_HALF_DUPLEX_RECIPIENT:
        s->rx.half_duplex_source =
        s->tx.half_duplex_source =
        s->half_duplex_source = mode;
        break;
    case V34_HALF_DUPLEX_CONTROL_CHANNEL:
        s->rx.half_duplex_state =
        s->tx.half_duplex_state =
        s->half_duplex_state = mode;
        break;
    case V34_HALF_DUPLEX_PRIMARY_CHANNEL:
        s->rx.half_duplex_state =
        s->tx.half_duplex_state =
        s->half_duplex_state = mode;
        break;
    case V34_HALF_DUPLEX_SILENCE:
        s->rx.half_duplex_state =
        s->tx.half_duplex_state =
        s->half_duplex_state = mode;
        break;
    }
    /*endswitch*/

    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v34_tx_restart(v34_state_t *s, int baud_rate, int bit_rate, int high_carrier)
{
    s->tx.bit_rate = bit_rate;
    s->tx.baud_rate = baud_rate;
    s->tx.high_carrier = high_carrier;

    s->tx.v34_carrier_phase_rate = dds_phase_ratef(carrier_frequency(s->tx.baud_rate, s->tx.high_carrier));
    if (s->calling_party)
    {
        s->tx.cc_carrier_phase_rate = dds_phase_ratef(1200.0f);
        s->tx.guard_phase_rate = 0;
        s->tx.guard_level = 0.0f;
    }
    else
    {
        s->tx.cc_carrier_phase_rate = dds_phase_ratef(2400.0f);
        s->tx.guard_phase_rate = 0; //dds_phase_ratef(1800.0f);
        s->tx.guard_level = 4.0f;
    }
    /*endif*/
    v34_set_working_parameters(&s->tx.parms, s->tx.baud_rate, s->tx.bit_rate, true);

#if defined(SPANDSP_USE_FIXED_POINT)
    vec_zeroi16(s->tx.rrc_filter_re, sizeof(s->tx.rrc_filter_re)/sizeof(s->tx.rrc_filter_re[0]));
    vec_zeroi16(s->tx.rrc_filter_im, sizeof(s->tx.rrc_filter_im)/sizeof(s->tx.rrc_filter_im[0]));
#else
    vec_zerof(s->tx.rrc_filter_re, sizeof(s->tx.rrc_filter_re)/sizeof(s->tx.rrc_filter_re[0]));
    vec_zerof(s->tx.rrc_filter_im, sizeof(s->tx.rrc_filter_im)/sizeof(s->tx.rrc_filter_im[0]));
#endif
    s->tx.lastbit = complex_sig_set(TRAINING_SCALE(0.0f), TRAINING_SCALE(0.0f));
    s->tx.rrc_filter_step = 0;
    s->tx.convolution = 0;
    s->tx.scramble_reg = 0;
    s->tx.carrier_phase = 0;

    s->tx.txbits = 0;
    s->tx.txptr = 0;
    s->tx.diff = 0;

    s->tx.line_probe_step = 0;
    s->tx.line_probe_cycles = 0;
    s->tx.line_probe_scaling = 0.0008f*s->tx.gain;

    s->tx.training_stage = 0x100;
    tx_silence_init(s, 75);

    s->tx.v0_pattern = 0;
    s->tx.super_frame = 0;
    s->tx.data_frame = 0;
    s->tx.s_bit_cnt = 0;
    s->tx.aux_bit_cnt = 0;

    s->tx.conv_encode_table = v34_conv16_encode_table;

    s->tx.current_get_bit = s->tx.get_bit;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int bit_rate_to_code(int bit_rate)
{
    int code;
    int rate;

    /* Translate between the bit rate as an integer and an internal code that
       represents the N*2400 bps and the possible extra 200 bps for auxilliary data. */
    if (bit_rate > 36800)
        return -1;
    /*endif*/
    code = bit_rate/2400;
    rate = code*2400;
    code = (code - 1) << 1;
    if (rate == bit_rate)
        return code;
    /*endif*/
    if ((rate + 200) == bit_rate)
        return (code | 1);
    /*endif*/
    return -1;
}
/*- End of function --------------------------------------------------------*/

static int baud_rate_to_code(int baud_rate)
{
    int i;

    /* Translate between the baud rate, as the integer nearest approaximation to the
       actual baud rate, and a 0-5 code used internally */
    for (i = 0;  i < 6;  i++)
    {
        if (baud_rate_parameters[i].baud_rate == baud_rate)
            return i;
        /*endif*/
    }
    /*endfor*/
    return -1;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v34_restart(v34_state_t *s, int baud_rate, int bit_rate, bool duplex)
{
    int bit_rate_code;
    int baud_rate_code;
    int high_carrier;

    span_log(&s->logging, SPAN_LOG_FLOW, "Tx - Restarting V.34, %d baud, %dbps\n", baud_rate, bit_rate);
    high_carrier = true;
    if ((bit_rate_code = bit_rate_to_code(bit_rate)) < 0)
        return -1;
    /*endif*/
    if ((baud_rate_code = baud_rate_to_code(baud_rate)) < 0)
        return -1;
    /*endif*/
    /* Check the bit rate and baud rate combination is valid */
    if (baud_rate_parameters[baud_rate_code].mappings[bit_rate_code].b == 0)
        return -1;
    /*endif*/
    s->duplex =
    s->rx.duplex =
    s->tx.duplex = duplex;

    /* Select the default half-duplex configuration */
    s->rx.half_duplex_source =
    s->tx.half_duplex_source =
    s->half_duplex_source = (s->calling_party)  ?  V34_HALF_DUPLEX_SOURCE  :  V34_HALF_DUPLEX_RECIPIENT;

    v34_tx_restart(s, baud_rate_code, bit_rate_code, high_carrier);
    v34_rx_restart(s, baud_rate_code, bit_rate_code, high_carrier);

    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(v34_state_t *) v34_init(v34_state_t *s,
                                     int baud_rate,
                                     int bit_rate,
                                     bool calling_party,
                                     bool duplex,
                                     span_get_bit_func_t get_bit,
                                     void *get_bit_user_data,
                                     span_put_bit_func_t put_bit,
                                     void *put_bit_user_data)
{
    int bit_rate_code;
    int baud_rate_code;

    if ((baud_rate_code = baud_rate_to_code(baud_rate)) < 0)
        return NULL;
    /*endif*/
    if ((bit_rate_code = bit_rate_to_code(bit_rate)) < 0)
        return NULL;
    /*endif*/
    /* Check the bit rate and baud rate combination is valid */
    if (baud_rate_parameters[baud_rate_code].mappings[bit_rate_code].b == 0)
        return NULL;
    /*endif*/
    if (s == NULL)
    {
        if ((s = (v34_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
        /*endif*/
    }
    /*endif*/
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "V.34");
    s->rx.logging = &s->logging;
    s->tx.logging = &s->logging;
    s->bit_rate = bit_rate;
    s->calling_party =
    s->rx.calling_party =
    s->tx.calling_party = calling_party;

    s->rx.stage = V34_RX_STAGE_INFO0;

    s->tx.get_bit = get_bit;
    s->tx.get_bit_user_data = get_bit_user_data;
    v34_tx_power(s, -14.0f);
    v34_restart(s, baud_rate, bit_rate, duplex);

    s->rx.put_bit = put_bit;
    s->rx.put_bit_user_data = put_bit_user_data;
    v34_rx_set_signal_cutoff(s, -45.5f);
    s->rx.agc_scaling = 0.0017f/V34_RX_PULSESHAPER_GAIN;
    s->rx.agc_scaling_save = 0.0f;
    s->rx.carrier_phase_rate_save = 0;

    if (calling_party)
    {
        s->tx.scrambler_tap = 17;
        s->rx.scrambler_tap = 4;
    }
    else
    {
        s->tx.scrambler_tap = 4;
        s->rx.scrambler_tap = 17;
    }
    /*endif*/
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v34_release(v34_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v34_free(v34_state_t *s)
{
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
