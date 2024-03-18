/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v34rx.c - ITU V.34 modem, receive part
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
#include <stddef.h>
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
#include "spandsp/private/modem_echo.h"
#include "spandsp/private/power_meter.h"
#include "spandsp/private/v34.h"

#include "v22bis_rx_1200_rrc.h"
#include "v22bis_rx_2400_rrc.h"

#include "v34_rx_2400_low_carrier_rrc.h"
#include "v34_rx_2400_high_carrier_rrc.h"
#include "v34_rx_2743_low_carrier_rrc.h"
#include "v34_rx_2743_high_carrier_rrc.h"
#include "v34_rx_2800_low_carrier_rrc.h"
#include "v34_rx_2800_high_carrier_rrc.h"
#include "v34_rx_3000_low_carrier_rrc.h"
#include "v34_rx_3000_high_carrier_rrc.h"
#include "v34_rx_3200_low_carrier_rrc.h"
#include "v34_rx_3200_high_carrier_rrc.h"
#include "v34_rx_3429_rrc.h"

#include "v34_local.h"
#include "v34_tables.h"
#include "v34_superconstellation_map.h"
#include "v34_convolutional_coders.h"
#include "v34_shell_map.h"
#include "v34_probe_signals.h"

#if !defined(M_PI)
/* C99 systems may not define M_PI */
#define M_PI 3.14159265358979323846264338327
#endif

#if defined(SPANDSP_USE_FIXED_POINT)
#define FP_FACTOR                       4096
#define FP_SHIFT_FACTOR                 12
#endif

#define FP_Q9_7_TO_F(x)                 ((float) x/128.0f)

#define CARRIER_NOMINAL_FREQ            1800.0f
#define EQUALIZER_DELTA                 0.21f
#define EQUALIZER_SLOW_ADAPT_RATIO      0.1f

#define V34_TRAINING_SEG_1              0
#define V34_TRAINING_SEG_4              0
#define V34_TRAINING_END                0
#define V34_TRAINING_SHUTDOWN_END       0

enum
{
    TRAINING_TX_STAGE_NORMAL_OPERATION_V34 = 0,
    TRAINING_TX_STAGE_NORMAL_OPERATION_CC = 1,
    TRAINING_TX_STAGE_PARKED
};

static const v34_rx_shaper_t *v34_rx_shapers_re[6][2] =
{
    {&rx_pulseshaper_2400_low_carrier_re, &rx_pulseshaper_2400_high_carrier_re},
    {&rx_pulseshaper_2743_low_carrier_re, &rx_pulseshaper_2743_high_carrier_re},
    {&rx_pulseshaper_2800_low_carrier_re, &rx_pulseshaper_2800_high_carrier_re},
    {&rx_pulseshaper_3000_low_carrier_re, &rx_pulseshaper_3000_high_carrier_re},
    {&rx_pulseshaper_3200_low_carrier_re, &rx_pulseshaper_3200_high_carrier_re},
    {&rx_pulseshaper_3429_re, &rx_pulseshaper_3429_re}
};

static const v34_rx_shaper_t *v34_rx_shapers_im[6][2] =
{
    {&rx_pulseshaper_2400_low_carrier_im, &rx_pulseshaper_2400_high_carrier_im},
    {&rx_pulseshaper_2743_low_carrier_im, &rx_pulseshaper_2743_high_carrier_im},
    {&rx_pulseshaper_2800_low_carrier_im, &rx_pulseshaper_2800_high_carrier_im},
    {&rx_pulseshaper_3000_low_carrier_im, &rx_pulseshaper_3000_high_carrier_im},
    {&rx_pulseshaper_3200_low_carrier_im, &rx_pulseshaper_3200_high_carrier_im},
    {&rx_pulseshaper_3429_im, &rx_pulseshaper_3429_im}
};

#if defined(SPANDSP_USE_FIXED_POINT)
#define complex_sig_set(re,im) complex_seti16(re,im)
#define complex_sig_t complexi16_t
#else
#define complex_sig_set(re,im) complex_setf(re,im)
#define complex_sig_t complexf_t
#endif

#if defined(SPANDSP_USE_FIXED_POINT)
#define TRAINING_SCALE(x)       ((int16_t) (32767.0f*x + ((x >= 0.0)  ?  0.5  :  -0.5)))
#else
#define TRAINING_SCALE(x)       (x)
#endif

static const complex_sig_t zero = {TRAINING_SCALE(0.0f), TRAINING_SCALE(0.0f)};

static void process_cc_half_baud(v34_rx_state_t *s, const complexf_t *sample);
static void process_primary_half_baud(v34_rx_state_t *s, const complexf_t *sample);
static void l1_l2_analysis_init(v34_rx_state_t *s);

static int descramble(v34_rx_state_t *s, int in_bit)
{
    int out_bit;

    /* One of the scrambler taps is a variable, so it can be adjusted for caller or answerer operation. */
    out_bit = (in_bit ^ (s->scramble_reg >> s->scrambler_tap) ^ (s->scramble_reg >> (23 - 1))) & 1;
    s->scramble_reg = (s->scramble_reg << 1) | in_bit;
    return out_bit;
}
/*- End of function --------------------------------------------------------*/

static void pack_output_bitstream(v34_rx_state_t *s)
{
    uint8_t *t;
    const uint8_t *u;
    int i;
    int n;
    int bit;
    int bb;
    int kk;

    span_log(s->logging,
             SPAN_LOG_FLOW,
             "Rx - Packed %p %8X - %X %X %X %X - %2X %2X %2X %2X %2X %2X %2X %2X\n",
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
    bitstream_init(&s->bs, true);
    t = s->rxbuf;
    bb = s->parms.b;
    kk = s->parms.k;
    /* If there are S bits, we switch between high mapping frames and low mapping frames based
       on the SWP pattern. We derive SWP algorithmically. Note that high/low mapping is only
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
    if (s->parms.k)
    {
        /* k is always < 32, so we always put the entire k bits into a single word */
        bitstream_put(&s->bs, &t, s->r0, kk);
        /* We can rely on this calculation always producing a value for chunk with no
           fractional part? */
        for (i = 0;  i < 4;  i++)
        {
            /* Some I bits */
            bitstream_put(&s->bs, &t, s->ibits[i], 3);
            if (s->parms.q)
            {
                /* Some Q bits */
                bitstream_put(&s->bs, &t, s->qbits[2*i], s->parms.q);
                bitstream_put(&s->bs, &t, s->qbits[2*i + 1], s->parms.q);
            }
            /*endif*/
        }
        /*endfor*/
    }
    else
    {
        /* If K is zero (i.e. b = 8, 9, 11, or 12), things need slightly special treatment */
        /* Pack 4 'i' fields */
        /* Need to treat 8, 9, 11, and 12 individually */
        n = bb - 8;
        for (i = 0;  i < n;  i++)
            bitstream_put(&s->bs, &t, s->ibits[i], 3);
        /*endfor*/
        for (  ;  i < 4;  i++)
            bitstream_put(&s->bs, &t, s->ibits[i], 2);
        /*endfor*/
    }
    /*endif*/
    bitstream_flush(&s->bs, &t);
#if 0
    printf("Block ");
    for (i = 0;  i < (s->b + 7)/8;  i++)
        printf("%02X ", s->rxbuf[i]);
    /*endfor*/
    printf("\n");
#endif

    bitstream_init(&s->bs, true);
    u = s->rxbuf;
    /* The first of the I bits might be auxiliary data */
    i = 0;
    s->aux_bit_cnt += s->parms.w;
    if (s->aux_bit_cnt >= s->parms.p)
    {
        s->aux_bit_cnt -= s->parms.p;
        for (  ;  i < kk;  i++)
        {
            bit = bitstream_get(&s->bs, &u, 1);
            s->put_bit(s->put_bit_user_data, descramble(s, bit));
        }
        /*endfor*/
        /* Auxiliary data bits are not scrambled (V.34/7) */
        bit = bitstream_get(&s->bs, &u, 1);
        if (s->put_aux_bit)
            s->put_aux_bit(s->put_bit_user_data, bit);
        /*endif*/
        i++;
    }
    for (  ;  i < bb;  i++)
    {
        bit = bitstream_get(&s->bs, &u, 1);
        s->put_bit(s->put_bit_user_data, descramble(s, bit));
    }
    /*endfor*/
}
/*- End of function --------------------------------------------------------*/

static void shell_unmap(v34_rx_state_t *s)
{
    int n21;
    int n22;
    int n23;
    int n24;
    int n41;
    int n42;
    int32_t n8;
    int k;
    int w41;
    int w42;
    int w2;
    int w8;
    const uint32_t *g2;
    const uint32_t *g4;
    const uint32_t *z8;

    g2 = g2s[s->parms.m];
    g4 = g4s[s->parms.m];
    z8 = z8s[s->parms.m];

    /* TODO: This code comes directly from the equations in V.34. Can it be improved? */
    n21 = (s->mjk[6] < s->parms.m - s->mjk[7])  ?  s->mjk[6]  :  (s->parms.m - 1 - s->mjk[7]);
    n22 = (s->mjk[4] < s->parms.m - s->mjk[5])  ?  s->mjk[4]  :  (s->parms.m - 1 - s->mjk[5]);
    n23 = (s->mjk[2] < s->parms.m - s->mjk[3])  ?  s->mjk[2]  :  (s->parms.m - 1 - s->mjk[3]);
    n24 = (s->mjk[0] < s->parms.m - s->mjk[1])  ?  s->mjk[0]  :  (s->parms.m - 1 - s->mjk[1]);

    w2 = s->mjk[4] + s->mjk[5];
    w41 = w2 + s->mjk[6] + s->mjk[7];
    n41 = 0;
    for (k = 0;  k < w2;  k++)
        n41 += g2[k]*g2[w41 - k];
    /*endfor*/
    n41 += n21*g2[w2];
    n41 += n22;

    w2 = s->mjk[0] + s->mjk[1];
    w42 = w2 + s->mjk[2] + s->mjk[3];
    n42 = 0;
    for (k = 0;  k < w2;  k++)
        n42 += g2[k]*g2[w42 - k];
    /*endfor*/
    n42 += n23*g2[w2];
    n42 += n24;

    w8 = w41 + w42;
    n8 = 0;
    for (k = 0;  k < w42;  k++)
        n8 += g4[k]*g4[w8 - k];
    /*endfor*/
    n8 += n41*g4[w42];
    n8 += n42;

    s->r0 = z8[w8] + n8;
}
/*- End of function --------------------------------------------------------*/

static int get_inverse_constellation_point(complexi16_t *point)
{
    int x;
    int y;

    x = point->re + 1;
    x = (x + 43)/4;
    if (x < 0)
        x = 0;
    else if (x > 22)
        x = 22;
    /*endif*/
    y = point->im + 1;
    y = (y + 43)/4;
    if (y < 0)
        y = 0;
    else if (y > 22)
        y = 22;
    /*endif*/
    return v34_inverse_superconstellation[y][x];
}
/*- End of function --------------------------------------------------------*/

static complexi16_t rotate90_counterclockwise(complexi16_t *x, int quads)
{
    complexi16_t y;

    /* Rotate a point counter-clockwise by quads 90 degree steps */
    switch (quads & 3)
    {
    case 0:
        y.re = x->re;
        y.im = x->im;
        break;
    case 1:
        y.re = -x->im;
        y.im = x->re;
        break;
    case 2:
        y.re = -x->re;
        y.im = -x->im;
        break;
    case 3:
        y.re = x->im;
        y.im = -x->re;
        break;
    }
    /*endswitch*/
    return y;
}
/*- End of function --------------------------------------------------------*/

/* Determine the 3 bits subset label for a particular constellation point */
static int16_t get_binary_subset_label(complexi16_t *pos)
{
    int x;
    int xored;
    int16_t subset;

    /* See V.34/9.6.3.1 */
    xored = pos->re ^ pos->im;
    x = xored & 2;
    subset = ((xored & 4) ^ (x << 1)) | (pos->re & 2) | (x >> 1);
    //printf("XXX Pre subset %d,%d => %d\n", pos->re, pos->im, subset);
    return subset;
}
/*- End of function --------------------------------------------------------*/

static complexi16_t quantize_rx(v34_rx_state_t *s, complexi16_t *x)
{
    complexi16_t y;

    /* Value is stored in Q9.7 format. */
    /* Output integer values. i.e. Q16.0 */
    y.re = abs(x->re);
    y.im = abs(x->im);
    if (s->parms.b >= 56)
    {
        /* 2w is 4 */
        /* We must mask out the 1st and 2nd bits, because we are rounding to the 3rd bit.
           All numbers coming out of this routine should be a multiple of 4. */
        y.re = (y.re + 0x0FF) >> 7;
        y.re &= ~0x03;
        y.im = (y.im + 0x0FF) >> 7;
        y.im &= ~0x03;
    }
    else
    {
        /* 2w is 2 */
        /* We must mask out the 1st bit, because we are rounding to the 2nd bit.
           All numbers coming out of this routine should be even. */
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

static complexi16_t precoder_rx_filter(v34_rx_state_t *s)
{
    /* h's are stored in Q2.14
       x's are stored in Q9.7
       not sure about x's
       so product is stored in Q11.21 */
    int i;
    complexi32_t sum;
    complexi16_t p;

    sum.re = 0;
    sum.im = 0;
    for (i = 0;  i < 3;  i++)
    {
        sum.re += ((int32_t) s->x[i].re*s->h[i].re - (int32_t) s->x[i].im*s->h[i].im);
        sum.im += ((int32_t) s->x[i].re*s->h[i].im + (int32_t) s->x[i].im*s->h[i].re);
    }
    /*endfor*/
    /* Round Q11.21 number format to Q9.7 */
    p.re = (abs(sum.re) + 0x01FFFL) >> 14;
    if (sum.re < 0)
        p.re = -p.re;
    /*endif*/
    p.im = (abs(sum.im) + 0x01FFFL) >> 14;
    if (sum.im < 0)
        p.im = -p.im;
    /*endif*/
    for (i = 2;  i > 0;  i--)
        s->x[i] = s->x[i - 1];
    /*endfor*/
    return p;
}
/*- End of function --------------------------------------------------------*/

static complexi16_t prediction_error_filter(v34_rx_state_t *s)
{
    int i;
    complexi32_t sum;
    complexi16_t yt;

    sum.re = (int32_t) s->xt[0].re*16384;
    sum.im = (int32_t) s->xt[0].im*16384;
    for (i = 0;  i < 3;  i++)
    {
        sum.re += ((int32_t) s->xt[i + 1].re*s->h[i].re - (int32_t) s->xt[i + 1].im*s->h[i].im);
        sum.im += ((int32_t) s->xt[i + 1].im*s->h[i].re + (int32_t) s->xt[i + 1].re*s->h[i].im);
    }
    /*endfor*/
    for (i = 3;  i > 0;  i--)
        s->xt[i] = s->xt[i - 1];
    /*endfor*/
    /* Round Q11.21 number format to Q9.7 */
    yt.re = (abs(sum.re) + 0x01FFFL) >> 14;
    if (sum.re < 0)
        yt.re = -yt.re;
    /*endif*/
    yt.im = (abs(sum.im) + 0x01FFFL) >> 14;
    if (sum.im < 0)
        yt.im = -yt.im;
    /*endif*/
    return yt;
}
/*- End of function --------------------------------------------------------*/

static void quantize_n_ways(complexi16_t xy[], complexi16_t *yt)
{
    int16_t q;

    /* Quantize the current x,y point to points in the 4 2D subsets */
    /* TODO: This suits the 16 way convolutional code. The 32 and 64 way codes need 8 way quantization here */

    /* We want to quantize to a -7, -3, 1, 5, 9 grid, but -8, -4, 0, 4, 8 is easier to deal with.
       We subtract 1, quantize to the nearest multiple of 4, and add the 1 back. */
    /* Note that this works in Q9.7 format. */

    /* Offset by one */
    xy[0].re = yt->re - FP_Q9_7(1);
    xy[0].im = yt->im - FP_Q9_7(1);
    /* Round to the nearest multiple of 4 towards zero */
    q = xy[0].re;
    xy[0].re = (abs(xy[0].re) + FP_Q9_7(2)) & ~(FP_Q9_7(4) - 1);
    if (q < 0)
        xy[0].re = -xy[0].re;
    /*endif*/
    q = xy[0].im;
    xy[0].im = (abs(xy[0].im) + FP_Q9_7(2)) & ~(FP_Q9_7(4) - 1);
    if (q < 0)
        xy[0].im = -xy[0].im;
    /*endif*/
    /* Restore the offset of one */
    xy[0].re += FP_Q9_7(1);
    xy[0].im += FP_Q9_7(1);

    /* Subset 0 done. Figure out the rest as offsets from subset 0 */
    xy[1].re = xy[0].re;
    if (yt->re < xy[0].re)
    {
        xy[2].re = xy[0].re - FP_Q9_7(2);
        xy[3].re = xy[0].re - FP_Q9_7(2);
    }
    else
    {
        xy[2].re = xy[0].re + FP_Q9_7(2);
        xy[3].re = xy[0].re + FP_Q9_7(2);
    }
    /*endif*/
    if (yt->im < xy[0].im)
    {
        xy[1].im = xy[0].im - FP_Q9_7(2);
        xy[2].im = xy[0].im - FP_Q9_7(2);
    }
    else
    {
        xy[1].im = xy[0].im + FP_Q9_7(2);
        xy[2].im = xy[0].im + FP_Q9_7(2);
    }
    /*endif*/
    xy[3].im = xy[0].im;
}
/*- End of function --------------------------------------------------------*/

static void viterbi_calculate_candidate_errors(int16_t error[4], complexi16_t xy[4], complexi16_t *yt)
{
    int i;
    complexi32_t diff;
    int32_t err;

    /* Calculate the errors between yt and the four 2D candidates. Errors are stored as 6:10 */
//printf("CIC");
    for (i = 0;  i < 4;  i++)
    {
        diff.re = (int32_t) xy[i].re - yt->re;
        diff.im = (int32_t) xy[i].im - yt->im;
        err = diff.re*diff.re + diff.im*diff.im;
        error[i] = err >> 4;
//printf(" %3d", error[i]);
    }
    /*endfor*/
//printf("\n");
}
/*- End of function --------------------------------------------------------*/

static void viterbi_calculate_branch_errors(viterbi_t *s, complexi16_t xy[2][4], int invert)
{
    static const uint8_t kk[8][4] =
    {
        {0, 0, 2, 2},
        {0, 1, 2, 3},
        {0, 2, 2, 0},
        {0, 3, 2, 1},
        {1, 1, 3, 3},
        {1, 2, 3, 0},
        {1, 3, 3, 1},
        {1, 0, 3, 2}
    };
    int br;
    int n;
    int inv;
    int error0;
    int error1;
    int smaller;
    int k0;
    int k1;

    inv = (invert)  ?  4  :  0;
    for (br = 0;  br < 8;  br++)
    {
        n = br ^ inv;
        error0 = s->error[0][kk[n][0]] + s->error[1][kk[n][1]];
        error1 = s->error[0][kk[n][2]] + s->error[1][kk[n][3]];
        if (error0 < error1)
        {
            smaller = error0;
            k0 = kk[n][0];
            k1 = kk[n][1];
        }
        else
        {
            smaller = error1;
            k0 = kk[n][2];
            k1 = kk[n][3];
        }
        /*endif*/
        s->branch_error[br] = smaller;
        s->vit[s->ptr].branch_error_x[br] = smaller;
        s->vit[s->ptr].bb[0][br] = xy[0][k0];
        s->vit[s->ptr].bb[1][br] = xy[1][k1];
    }
    /*endfor*/
}
/*- End of function --------------------------------------------------------*/

static void viterbi_update_path_metrics(viterbi_t *s)
{
    int16_t i;
    int16_t j;
    int16_t prev_state;
    int16_t branch;
    uint32_t curr_min_metric;
    uint32_t min_metric;
    uint32_t metric;
    uint16_t min_state;
    uint16_t min_branch;
    int prev_ptr;

    curr_min_metric = UINT32_MAX;
    /* Loop through each state */
    prev_ptr = (s->ptr - 1) & 0xF;
    for (i = 0;  i < 16;  i++)
    {
        min_metric = UINT32_MAX;
        min_state = 0;
        min_branch = 0;
        /* Loop through each possible branch from the previous state */
        for (j = 0;  j < 4;  j++)
        {
            prev_state = (*s->conv_decode_table)[i][j] >> 3;
            branch = (*s->conv_decode_table)[i][j] & 0x7;
            metric = s->vit[prev_ptr].cumulative_path_metric[prev_state] + s->branch_error[branch];

//if (metric == 0)
//    printf("HHH %p metric is zero - %2d %2d %2d %2d %2d\n", s, prev_ptr, i, j, prev_state, branch);
///*endif*/
//if (s->branch_error[branch] == 0)
//    printf("HHX %p metric is zero - %2d %2d %2d %2d %2d\n", s, prev_ptr, i, j, prev_state, branch);
///*endif*/
            if (metric < min_metric)
            {
                min_metric = metric;
                min_state = prev_state;
                min_branch = branch;
            }
            /*endif*/
        }
        /*endfor*/
        s->vit[s->ptr].cumulative_path_metric[i] = min_metric;
        s->vit[s->ptr].previous_path_ptr[i] = min_state;
        s->vit[s->ptr].pts[i] = min_branch;
        if (min_metric < curr_min_metric)
        {
            curr_min_metric = min_metric;
            s->curr_min_state = i;
        }
        /*endif*/
    }
    /*endfor*/
//printf("GGG %p min metric %d, state %d\n", s, curr_min_metric, s->curr_min_state);
//printf("JJJ %p ", s);
    for (i = 0;  i < 16;  i++)
    {
        s->vit[s->ptr].cumulative_path_metric[i] -= curr_min_metric;
//printf("%4d ", s->cumulative_path_metric[s->ptr][i]);
    }
    /*endfor*/
//printf("\n");
}
/*- End of function --------------------------------------------------------*/

static void viterbi_trace_back(viterbi_t *s, complexi16_t y[2])
{
    int branch;
    int next_state;
    int last_baud;
    int i;

    next_state = s->curr_min_state;
    last_baud = (s->ptr - 15) & 0xF;
//printf("FFF %p %2d", s, next_state);
    for (i = s->ptr;  i != last_baud;  i = (i - 1) & 0xF)
    {
        next_state = s->vit[i].previous_path_ptr[next_state];
//printf(" %2d", next_state);
    }
    /*endfor*/
    for (i = 0;  i < 8;  i++)
    {
        if (s->vit[last_baud].branch_error_x[i] == 0)
        {
            branch = i;
            break;
        }
    }
    /*endfor*/
    branch = s->vit[last_baud].pts[next_state];
//printf(" (%d)\n", branch);

    y[0] = s->vit[last_baud].bb[0][branch];
    y[1] = s->vit[last_baud].bb[1][branch];
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

static int process_rx_info0(v34_rx_state_t *s, uint8_t buf[])
{
    bitstream_state_t bs;
    const uint8_t *t;

    memset(&s->far_capabilities, 0, sizeof(s->far_capabilities));
    bitstream_init(&bs, true);
    t = buf;
    s->far_capabilities.support_baud_rate_low_carrier[V34_BAUD_RATE_2400] =
    s->far_capabilities.support_baud_rate_high_carrier[V34_BAUD_RATE_2400] = true;
    s->far_capabilities.support_baud_rate_low_carrier[V34_BAUD_RATE_2743] =
    s->far_capabilities.support_baud_rate_high_carrier[V34_BAUD_RATE_2743] = bitstream_get(&bs, &t, 1);
    s->far_capabilities.support_baud_rate_low_carrier[V34_BAUD_RATE_2800] =
    s->far_capabilities.support_baud_rate_high_carrier[V34_BAUD_RATE_2800] = bitstream_get(&bs, &t, 1);
    s->far_capabilities.support_baud_rate_low_carrier[V34_BAUD_RATE_3429] =
    s->far_capabilities.support_baud_rate_high_carrier[V34_BAUD_RATE_3429] = bitstream_get(&bs, &t, 1);
    s->far_capabilities.support_baud_rate_low_carrier[V34_BAUD_RATE_3000] = bitstream_get(&bs, &t, 1);
    s->far_capabilities.support_baud_rate_high_carrier[V34_BAUD_RATE_3000] = bitstream_get(&bs, &t, 1);
    s->far_capabilities.support_baud_rate_low_carrier[V34_BAUD_RATE_3200] = bitstream_get(&bs, &t, 1);
    s->far_capabilities.support_baud_rate_high_carrier[V34_BAUD_RATE_3200] = bitstream_get(&bs, &t, 1);
    s->far_capabilities.rate_3429_allowed = bitstream_get(&bs, &t, 1);
    s->far_capabilities.support_power_reduction = bitstream_get(&bs, &t, 1);
    s->far_capabilities.max_baud_rate_difference = bitstream_get(&bs, &t, 3);
    s->far_capabilities.from_cme_modem = bitstream_get(&bs, &t, 1);
    s->far_capabilities.support_1664_point_constellation = bitstream_get(&bs, &t, 1);
    s->far_capabilities.tx_clock_source = bitstream_get(&bs, &t, 2);
    s->info0_acknowledgement = bitstream_get(&bs, &t, 1);

    log_info0(s->logging, false, &s->far_capabilities, s->info0_acknowledgement);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int process_rx_info1c(v34_rx_state_t *s, info1c_t *info1c, uint8_t buf[])
{
    bitstream_state_t bs;
    const uint8_t *t;
    int i;

    bitstream_init(&bs, true);
    t = buf;
    /* 12:14    Minimum power reduction to be implemented by the answer modem transmitter. An integer between 0 and 7
                gives the recommended power reduction in dB. These bits shall indicate 0 if INFO0a indicated that the answer
                modem transmitter cannot reduce its power. */
    info1c->power_reduction = bitstream_get(&bs, &t, 3);
    /* 15:17    Additional power reduction, below that indicated by bits 12-14, which can be tolerated by the call modem
                receiver. An integer between 0 and 7 gives the additional power reduction in dB. These bits shall indicate 0 if
                INFO0a indicated that the answer modem transmitter cannot reduce its power. */
    info1c->additional_power_reduction = bitstream_get(&bs, &t, 3);
    /* 18:24    Length of MD to be transmitted by the call modem during Phase 3. An integer between 0 and 127 gives the
                length of this sequence in 35 ms increments. */
    info1c->md = bitstream_get(&bs, &t, 7);
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
        info1c->rate_data[i].use_high_carrier = bitstream_get(&bs, &t, 1);
        info1c->rate_data[i].pre_emphasis = bitstream_get(&bs, &t, 4);
        info1c->rate_data[i].max_bit_rate = bitstream_get(&bs, &t, 4);
    }
    /*endfor*/
    /* 79:88    Frequency offset of the probing tones as measured by the call modem receiver. The frequency offset number
                shall be the difference between the nominal 1050 Hz line probing signal tone received and the 1050 Hz tone
                transmitted, f(received) and f(transmitted). A two's complement signed integer between -511 and 511 gives the
                measured offset in 0.02 Hz increments. Bit 88 is the sign bit of this integer. The frequency offset measurement
                shall be accurate to 0.25 Hz. Under conditions where this accuracy cannot be achieved, the integer shall be set
                to -512 indicating that this field is to be ignored. */
    info1c->freq_offset = bitstream_get(&bs, &t, 10);
    if ((info1c->freq_offset & 0x200))
        info1c->freq_offset = -(info1c->freq_offset ^ 0x3FF) - 1;
    /*endif*/

    log_info1c(s->logging, false, info1c);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int process_rx_info1a(v34_rx_state_t *s, info1a_t *info1a, uint8_t buf[])
{
    bitstream_state_t bs;
    const uint8_t *t;

    bitstream_init(&bs, true);
    t = buf;
    /* 12:14    Minimum power reduction to be implemented by the call modem transmitter. An integer between 0 and 7 gives
                the recommended power reduction in dB. These bits shall indicate 0 if INFO0c indicated that the call modem
                transmitter cannot reduce its power. */
    info1a->power_reduction = bitstream_get(&bs, &t, 3);
    /* 15:17    Additional power reduction, below that indicated by bits 12:14, which can be tolerated by the answer modem
                receiver. An integer between 0 and 7 gives the additional power reduction in dB. These bits shall indicate 0 if
                INFO0c indicated that the call modem transmitter cannot reduce its power. */
    info1a->additional_power_reduction = bitstream_get(&bs, &t, 3);
    /* 18:24    Length of MD to be transmitted by the answer modem during Phase 3. An integer between 0 and 127 gives the
                length of this sequence in 35 ms increments. */
    info1a->md = bitstream_get(&bs, &t, 7);
    /* 25       Set to 1 indicates that the high carrier frequency is to be used in transmitting from the call modem to the answer
                modem. This shall be consistent with the capabilities of the call modem indicated in INFO0c. */
    info1a->use_high_carrier = bitstream_get(&bs, &t, 1);
    /* 26:29    Pre-emphasis filter to be used in transmitting from the call modem to the answer modem. These bits form an
                integer between 0 and 10 which represents the pre-emphasis filter index (see Tables 3 and 4). */
    info1a->preemphasis_filter = bitstream_get(&bs, &t, 4);
    /* 30:33    Projected maximum data rate for the selected symbol rate from the call modem to the answer modem. These bits
                form an integer between 0 and 14 which gives the projected data rate as a multiple of 2400 bits/s. */
    info1a->max_data_rate = bitstream_get(&bs, &t, 4);
    /* 34:36    Symbol rate to be used in transmitting from the answer modem to the call modem. An integer between 0 and 5
                gives the symbol rate, where 0 represents 2400 and a 5 represents 3429. The symbol rate selected shall be
                consistent with information in INFO1c and consistent with the symbol rate asymmetry allowed as indicated in
                INFO0a and INFO0c. The carrier frequency and pre-emphasis filter to be used are those already indicated for
                this symbol rate in info1c. */
    info1a->baud_rate_a_to_c = bitstream_get(&bs, &t, 3);
    /* 37:39    Symbol rate to be used in transmitting from the call modem to the answer modem. An integer between 0 and 5
                gives the symbol rate, where 0 represents 2400 and a 5 represents 3429. The symbol rate selected shall be
                consistent with the capabilities indicated in INFO0a and consistent with the symbol rate asymmetry allowed as
                indicated in INFO0a and INFO0c. */
    info1a->baud_rate_c_to_a = bitstream_get(&bs, &t, 3);
    /* 40:49    Frequency offset of the probing tones as measured by the answer modem receiver. The frequency offset number
                shall be the difference between the nominal 1050 Hz line probing signal tone received and the 1050 Hz tone
                transmitted, f(received) and f(transmitted). A two's complement signed integer between -511 and 511 gives the
                measured offset in 0.02 Hz increments. Bit 49 is the sign bit of this integer. The frequency offset measurement
                shall be accurate to 0.25 Hz. Under conditions where this accuracy cannot be achieved, the integer shall be set
                to -512 indicating that this field is to be ignored. */
    info1a->freq_offset = bitstream_get(&bs, &t, 10);
    if ((info1a->freq_offset & 0x200))
        info1a->freq_offset = -(info1a->freq_offset ^ 0x3FF) - 1;
    /*endif*/
    s->baud_rate = info1a->baud_rate_c_to_a;
    s->v34_carrier_phase_rate = dds_phase_ratef(carrier_frequency(s->baud_rate, s->high_carrier));

    log_info1a(s->logging, false, info1a);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int process_rx_infoh(v34_rx_state_t *s, infoh_t *infoh, uint8_t buf[])
{
    bitstream_state_t bs;
    const uint8_t *t;

    memset(infoh, 0, sizeof(*infoh));
    bitstream_init(&bs, true);
    t = buf;
    /* 12:14    Power reduction requested by the recipient modem receiver. An integer between 0 and 7
                gives the requested power reduction in dB. These bits shall indicate 0 if the source
                modem's INFO0 indicated that the source modem transmitter cannot reduce its power. */
    infoh->power_reduction = bitstream_get(&bs, &t, 3);
    /* 15:21    Length of TRN to be transmitted by the source modem during Phase 3. An integer between
                0 and 127 gives the length of this sequence in 35 ms increments. */
    infoh->length_of_trn = bitstream_get(&bs, &t, 7);
    /* 22       Set to 1 indicates the high carrier frequency is to be used in data mode transmission. This
                must be consistent with the capabilities indicated in the source modem's INFO0. */
    infoh->use_high_carrier = bitstream_get(&bs, &t, 1);
    /* 23:26    Pre-emphasis filter to be used in transmitting from the source modem to the recipient modem.
                These bits form an integer between 0 and 10 which represents the pre-emphasis filter index
                (see Tables 3 and 4). */
    infoh->preemphasis_filter = bitstream_get(&bs, &t, 4);
    /* 27:29    Symbol rate to be used for data transmission. An integer between 0 and 5 gives the symbol rate, where 0
                represents 2400 and a 5 represents 3429. */
    infoh->baud_rate = bitstream_get(&bs, &t, 3);
    /* 30       Set to 1 indicates TRN uses a 16-point constellation, 0 indicates TRN uses a 4-point constellation. */
    infoh->trn16 = bitstream_get(&bs, &t, 1);

    log_infoh(s->logging, false, infoh);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int process_rx_mp(v34_rx_state_t *s, mp_t *mp, uint8_t buf[])
{
    int i;
    const uint8_t *t;
    bitstream_state_t bs;

    bitstream_init(&bs, true);
    t = buf;
    /* 18       Type */
    mp->type = bitstream_get(&bs, &t, 1);
    /* 19       Reserved by the ITU */
    bitstream_get(&bs, &t, 1);
    /* 20:23    Maximum call modem to answer modem data signalling rate: Data rate = N * 2400
                where N is a four-bit integer between 1 and 14. */
    mp->bit_rate_c_to_a = bitstream_get(&bs, &t, 4);
    /* 24:27    Maximum answer modem to call modem data signalling rate: Data rate = N * 2400
                where N is a four-bit integer between 1 and 14. */
    mp->bit_rate_a_to_c = bitstream_get(&bs, &t, 4);
    /* 28       Auxiliary channel select bit. Set to 1 if modem is capable of supporting and
                enables auxiliary channel. Auxiliary channel is used only if both modems set
                this bit to 1. */
    mp->aux_channel_supported = bitstream_get(&bs, &t, 1);
    /* 29:30    Trellis encoder select bits:
                0 = 16 state; 1 = 32 state; 2 = 64 state; 3 = Reserved for ITU-T.
                Receiver requires remote-end transmitter to use selected trellis encoder. */
    mp->trellis_size = bitstream_get(&bs, &t, 2);
    /* 31       Non-linear encoder parameter select bit for the remote-end transmitter.
                0: Q = 0, 1: Q = 0.3125. */
    mp->use_non_linear_encoder = bitstream_get(&bs, &t, 1);
    /* 32       Constellation shaping select bit for the remote-end transmitter. 0: minimum,
                1: expanded (see Table 10). */
    mp->expanded_shaping = bitstream_get(&bs, &t, 1);
    /* 33       Acknowledge bit. 0 = modem has not received MP from far end. 1 = received MP from far end. */
    mp->mp_acknowledged = bitstream_get(&bs, &t, 1);
    /* 34       Start bit: 0. */
    bitstream_get(&bs, &t, 1);
    /* 35:49    Data signalling rate capability mask.
                Bit 35:2400; bit 36:4800; bit 37:7200;...; bit 46:28800; bit 47:31200; bit 48:33600;
                bit 49: Reserved for ITU-T. (This bit is set to 0 by the transmitting modem and is not
                interpreted by the receiving modem.) Bits set to 1 indicate data signalling rates supported
                and enabled in both transmitter and receiver of modem. */
    mp->signalling_rate_mask = bitstream_get(&bs, &t, 15);
    /* 50       Asymmetric data signalling rate enable. 1 indicates a modem capable of
                asymmetric data signalling rates. */
    mp->asymmetric_rates_allowed = bitstream_get(&bs, &t, 1);
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
            bitstream_get(&bs, &t, 1);
            mp->precoder_coeffs[i].re = bitstream_get(&bs, &t, 16);
            bitstream_get(&bs, &t, 1);
            mp->precoder_coeffs[i].im = bitstream_get(&bs, &t, 16);
        }
        /*endfor*/
    }
    else
    {
        /* The following are not included in an MP0 message */
        for (i = 0;  i < 3;  i++)
        {
            mp->precoder_coeffs[i].re = 0;
            mp->precoder_coeffs[i].im = 0;
        }
        /*endfor*/
    }
    /*endif*/
    /* We can ignore the remaining bits. They are not used. */

    log_mp(s->logging, false, mp);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int process_rx_mph(v34_rx_state_t *s, mph_t *mph, uint8_t buf[])
{
    int i;
    const uint8_t *t;
    bitstream_state_t bs;

    bitstream_init(&bs, true);
    t = buf;
    /* 18       Type */
    mph->type = bitstream_get(&bs, &t, 1);
    /* 19       Reserved by the ITU */
    bitstream_get(&bs, &t, 1);
    /* 20:23    Maximum data signalling rate:
                Data rate = N * 2400 where N is a 4-bit integer between 1 and 14. */
    mph->max_data_rate = bitstream_get(&bs, &t, 4);
    /* 24:26    Reserved for ITU-T: These bits are set to 0 by the transmitting modem and are
                not interpreted by the receiving modem. */
    bitstream_get(&bs, &t, 3);
    /* 27       Control channel data signalling rate selected for remote transmitter.
                0 = 1200 bit/s, 1 = 2400 bit/s (see bit 50 below). */
    mph->control_channel_2400 = bitstream_get(&bs, &t, 1);
    /* 28       Reserved for ITU-T: This bit is set to 0 by the transmitting modem and is not
                interpreted by the receiving modem. */
    bitstream_get(&bs, &t, 1);
    /* 29:30    Trellis encoder select bits:
                0 = 16 state; 1 = 32 state; 2 = 64 state; 3 = Reserved for ITU-T.
                Receiver requires remote-end transmitter to use selected trellis encoder. */
    mph->trellis_size = bitstream_get(&bs, &t, 2);
    /* 31       Non-linear encoder parameter select bit for the remote-end transmitter.
                0: Q = 0, 1: Q = 0.3125. */
    mph->use_non_linear_encoder = bitstream_get(&bs, &t, 1);
    /* 32       Constellation shaping select bit for the remote-end transmitter.
                0: minimum, 1: expanded (see Table 10). */
    mph->expanded_shaping = bitstream_get(&bs, &t, 1);
    /* 33       Reserved for ITU-T: This bit is set to 0 by the transmitting modem and is not
                interpreted by the receiving modem. */
    /* 34       Start bit: 0. */
    bitstream_get(&bs, &t, 2);
    /* 35:49    Data signalling rate capability mask.
                Bit 35:2400; bit 36:4800; bit 37:7200;...; bit 46:28800; bit 47:31200; bit 48:33600;
                bit 49: Reserved for ITU-T. (This bit is set to 0 by the transmitting modem and is not
                interpreted by the receiving modem.) Bits set to 1 indicate data signalling rates supported
                and enabled in both transmitter and receiver of modem. */
    mph->signalling_rate_mask = bitstream_get(&bs, &t, 15);
    /* 50       Enables asymmetric control channel data rates:
                0 = Asymmetric mode not allowed; 1 = Asymmetric mode allowed.
                Asymmetric mode shall be used only when both modems set bit 50 to 1. If different data rates are selected
                in symmetric mode, both modems shall transmit at the lower rate. */
    mph->asymmetric_rates_allowed = bitstream_get(&bs, &t, 1);
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
            bitstream_get(&bs, &t, 1);
            mph->precoder_coeffs[i].re = bitstream_get(&bs, &t, 16);
            bitstream_get(&bs, &t, 1);
            mph->precoder_coeffs[i].im = bitstream_get(&bs, &t, 16);
        }
        /*endfor*/
    }
    else
    {
        for (i = 0;  i < 3;  i++)
        {
            mph->precoder_coeffs[i].re = 0;
            mph->precoder_coeffs[i].im = 0;
        }
        /*endfor*/
    }
    /*endif*/
    /* We can ignore the remaining bits. They are not used. */
    log_mph(s->logging, false, mph);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void put_info_bit(v34_rx_state_t *s, int bit, int time_offset)
{
    /* Put info0, info1, tone A or tone B bits */
    printf("Rx bit = %d\n", bit);
    s->bitstream = (s->bitstream << 1) | bit;
    switch (s->stage)
    {
    case V34_RX_STAGE_TONE_A:
        /* Calling side */
        if (++s->persistence1 < 10)
            break;
        /*endif*/
        if (bit == 0)
        {
            if (++s->persistence2 == 20)
            {
                //s->received_event = V34_EVENT_TONE_SEEN;
            }
            /*endif*/
            break;
        }
        /*endif*/
        if (!s->signal_present)
            s->persistence2 = 0;
        /*endif*/
        /* We have a reversal, but we should only recognise it if it has been
           a little while since the last one */
        if (s->persistence2 > 20)
        {
            printf("Rx bit reversal in tone A\n");
            switch (s->received_event)
            {
            case V34_EVENT_REVERSAL_1:
                span_log(s->logging, SPAN_LOG_FLOW, "Rx - reversal 2 in tone A\n");
                s->tone_ab_hop_time = s->sample_time + time_offset;
                s->received_event = V34_EVENT_REVERSAL_2;
                l1_l2_analysis_init(s);
                break;
            case V34_EVENT_REVERSAL_2:
            case V34_EVENT_L2_SEEN:
                span_log(s->logging, SPAN_LOG_FLOW, "Rx - reversal 3 in tone A\n");
                s->tone_ab_hop_time = s->sample_time + time_offset;
                s->received_event = V34_EVENT_REVERSAL_3;
                /* The next info message will be INFO1a */
                s->target_bits = 70 - (4 + 8 + 4);
                s->stage = V34_RX_STAGE_INFO1A;
                break;
            default:
                span_log(s->logging, SPAN_LOG_FLOW, "Rx - reversal 1 in tone A\n");
                s->tone_ab_hop_time = s->sample_time + time_offset;
                s->received_event = V34_EVENT_REVERSAL_1;
                break;
            }
            /*endswitch*/
            s->persistence1 = 0;
        }
        /*endif*/
        s->persistence2 = 0;
        break;
    case V34_RX_STAGE_TONE_B:
        /* Answering side */
        if (++s->persistence1 < 10)
            break;
        /*endif*/
        if (bit == 0)
        {
            if (++s->persistence2 == 20)
            {
                //s->received_event = V34_EVENT_TONE_SEEN;
            }
            /*endif*/
            break;
        }
        /*endif*/
        if (!s->signal_present)
            s->persistence2 = 0;
        /*endif*/
        /* We have a reversal, but we should only recognise it if it has been
           a little while since the last one */
        if (s->persistence2 > 20)
        {
            printf("Rx bit reversal in tone B\n");
            switch (s->received_event)
            {
            case V34_EVENT_REVERSAL_2:
                span_log(s->logging, SPAN_LOG_FLOW, "Rx - reversal 3 in tone B\n");
                s->tone_ab_hop_time = s->sample_time + time_offset;
                s->received_event = V34_EVENT_REVERSAL_3;
                break;
            case V34_EVENT_REVERSAL_1:
                /* TODO: Need to avoid getting here falsely, just because the tone has resumed */
                span_log(s->logging, SPAN_LOG_FLOW, "Rx - reversal 2 in tone B\n");
                s->tone_ab_hop_time = s->sample_time + time_offset;
                s->received_event = V34_EVENT_REVERSAL_2;
                /* The next info message will be INFO1c */
                s->target_bits = 109 - (4 + 8 + 4);
                l1_l2_analysis_init(s);
                break;
            default:
                span_log(s->logging, SPAN_LOG_FLOW, "Rx - reversal 1 in tone B\n");
                s->tone_ab_hop_time = s->sample_time + time_offset;
                s->received_event = V34_EVENT_REVERSAL_1;
                break;
            }
            /*endswitch*/
            s->persistence1 = 0;
        }
        /*endif*/
        s->persistence2 = 0;
        break;
    }
    /* Search for INFO0, INFOh, INFO1a or INFO1c messages. */
    if (s->bit_count == 0)
    {
        /* Look for info message sync code */
        if ((s->bitstream & 0x3FF) == 0x372)
        {
            span_log(s->logging, SPAN_LOG_FLOW, "Rx - info sync code detected\n");
            printf("Rx bit info sync code detected\n");
            s->crc = 0xFFFF;
            s->bit_count = 1;
        }
        /*endif*/
    }
    else
    {
        /* Every 8 bits save the resulting byte */
        if ((s->bit_count & 0x07) == 0)
            s->info_buf[(s->bit_count >> 3) - 1] = bit_reverse8(s->bitstream & 0xFF);
        /*endif*/
        s->crc = crc_itu16_bits(bit, 1, s->crc);
        if (s->bit_count++ == s->target_bits)
        {
            span_log(s->logging, SPAN_LOG_FLOW, "Rx - info CRC result 0x%x\n", s->crc);
            printf("Rx bit CRC result 0x%X\n", s->crc);
            printf("Rx 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
                   s->info_buf[0],
                   s->info_buf[1],
                   s->info_buf[2],
                   s->info_buf[3],
                   s->info_buf[4],
                   s->info_buf[5],
                   s->info_buf[6],
                   s->info_buf[7],
                   s->info_buf[8]);
            if (s->crc == 0)
            {
                switch (s->stage)
                {
                case V34_RX_STAGE_TONE_A:
                case V34_RX_STAGE_TONE_B:
                case V34_RX_STAGE_INFO0:
                    process_rx_info0(s, s->info_buf);
                    s->stage = (s->calling_party)  ?   V34_RX_STAGE_TONE_A  :  V34_RX_STAGE_TONE_B;
                    s->received_event = V34_EVENT_INFO0_OK;
                    break;
                case V34_RX_STAGE_INFOH:
                    process_rx_infoh(s, &s->infoh, s->info_buf);
                    s->received_event = V34_EVENT_INFO1_OK;
                    break;
                case V34_RX_STAGE_INFO1C:
                    process_rx_info1c(s, &s->info1c, s->info_buf);
                    s->received_event = V34_EVENT_INFO1_OK;
                    break;
                case V34_RX_STAGE_INFO1A:
                    process_rx_info1a(s, &s->info1a, s->info_buf);
                    s->received_event = V34_EVENT_INFO1_OK;
                    break;
                }
                /*endswitch*/
            }
            else
            {
                switch (s->stage)
                {
                case V34_RX_STAGE_TONE_A:
                case V34_RX_STAGE_TONE_B:
                case V34_RX_STAGE_INFO0:
                    s->received_event = V34_EVENT_INFO0_BAD;
                case V34_RX_STAGE_INFOH:
                    break;
                case V34_RX_STAGE_INFO1C:
                case V34_RX_STAGE_INFO1A:
                    s->received_event = V34_EVENT_INFO1_BAD;
                    break;
                }
                /*endswitch*/
            }
            /*endif*/
            s->bit_count = 0;
        }
        /*endif*/
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static int info_rx(v34_rx_state_t *s, const int16_t amp[], int len)
{
    int i;
    int step;
    complexf_t z;
    complexf_t zz;
    complexf_t sample;
    float ii;
    float qq;
    uint32_t angle;
    int32_t power;

    s->agc_scaling = 0.01f;
    step = 6;
    for (i = 0;  i < len;  i++)
    {
        power = power_meter_update(&s->power, amp[i]);
        if (s->signal_present)
        {
            if (power < s->carrier_off_power)
            {
span_log(s->logging, SPAN_LOG_FLOW, "Signal down\n");
                s->signal_present = false;
                s->persistence2 = 0;
            }
            /*endif*/
        }
        else
        {
            if (power > s->carrier_on_power)
            {
span_log(s->logging, SPAN_LOG_FLOW, "Signal up\n");
                s->signal_present = true;
                s->persistence2 = 0;
            }
            /*endif*/
        }
        /*endif*/
        s->rrc_filter[s->rrc_filter_step] = amp[i];
        if (++s->rrc_filter_step >= V34_RX_FILTER_STEPS)
            s->rrc_filter_step = 0;
        /*endif*/
        if (s->calling_party)
        {
#if defined(SPANDSP_USE_FIXED_POINT)
            ii = vec_circular_dot_prodi16(s->rrc_filter, rx_pulseshaper_2400_re[step], V34_RX_FILTER_STEPS, s->rrc_filter_step);
            qq = vec_circular_dot_prodi16(s->rrc_filter, rx_pulseshaper_2400_im[step], V34_RX_FILTER_STEPS, s->rrc_filter_step);
#else
            ii = vec_circular_dot_prodf(s->rrc_filter, rx_pulseshaper_2400_re[step], V34_RX_FILTER_STEPS, s->rrc_filter_step);
            qq = vec_circular_dot_prodf(s->rrc_filter, rx_pulseshaper_2400_im[step], V34_RX_FILTER_STEPS, s->rrc_filter_step);
#endif
        }
        else
        {
#if defined(SPANDSP_USE_FIXED_POINT)
            ii = vec_circular_dot_prodi16(s->rrc_filter, rx_pulseshaper_1200_re[step], V34_RX_FILTER_STEPS, s->rrc_filter_step);
            qq = vec_circular_dot_prodi16(s->rrc_filter, rx_pulseshaper_1200_im[step], V34_RX_FILTER_STEPS, s->rrc_filter_step);
#else
            ii = vec_circular_dot_prodf(s->rrc_filter, rx_pulseshaper_1200_re[step], V34_RX_FILTER_STEPS, s->rrc_filter_step);
            qq = vec_circular_dot_prodf(s->rrc_filter, rx_pulseshaper_1200_im[step], V34_RX_FILTER_STEPS, s->rrc_filter_step);
#endif
        }
        /*endif*/
        sample.re = ii*s->agc_scaling;
        sample.im = qq*s->agc_scaling;
        /* Shift to baseband - since this is done in full complex form, the result is clean. */
        z = dds_lookup_complexf(s->carrier_phase);
        zz.re = sample.re*z.re - sample.im*z.im;
        zz.im = -sample.re*z.im - sample.im*z.re;
        angle = arctan2(zz.im, zz.re);
        printf("XXX%d, %7d, %f, %f, 0x%08X, %d\n", s->calling_party, amp[i], zz.re, zz.im, angle, angle);
        if (abs(angle - s->last_angles[1]) > DDS_PHASE(90.0f)  &&  s->blip_duration > 3)
        {
            put_info_bit(s, 1, i);
            s->duration = 0;
            s->blip_duration = 0;
        }
        else
        {
            if (s->blip_duration > 60)
            {
                /* We are getting rather late for a transition. This must be a zero bit. */
                put_info_bit(s, 0, i);
                /* Step on by one bit time. */
                s->blip_duration -= 40;
            }
            /*endif*/
        }
        /*endif*/
        s->last_angles[1] = s->last_angles[0];
        s->last_angles[0] = angle;
        s->duration++;
        s->blip_duration += 3;
        dds_advancef(&s->carrier_phase, s->cc_carrier_phase_rate);
    }
    /*endfor*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static __inline__ void cc_symbol_sync(v34_rx_state_t *s)
{
    int i;
#if defined(SPANDSP_USE_FIXED_POINT)
    int32_t v;
    int32_t p;
#else
    float v;
    float p;
#endif

    /* This routine adapts the position of the half baud samples entering the equalizer. */

    /* This symbol sync scheme is based on the technique first described by Dominique Godard in
        Passband Timing Recovery in an All-Digital Modem Receiver
        IEEE TRANSACTIONS ON COMMUNICATIONS, VOL. COM-26, NO. 5, MAY 1978 */

    /* This is slightly rearranged from figure 3b of the Godard paper, as this saves a couple of
       maths operations */
#if defined(SPANDSP_USE_FIXED_POINT)
    /* TODO: The scalings used here need more thorough evaluation, to see if overflows are possible. */
    /* Cross correlate */
    v = (((s->cc_ted.symbol_sync_low[1] >> 5)*(s->cc_ted.symbol_sync_high[0] >> 4)) >> 15)*s->cc_ted.low_band_edge_coeff[2]
      - (((s->cc_ted.symbol_sync_low[0] >> 5)*(s->cc_ted.symbol_sync_high[1] >> 4)) >> 15)*s->cc_ted.high_band_edge_coeff[2]
      + (((s->cc_ted.symbol_sync_low[1] >> 5)*(s->cc_ted.symbol_sync_high[1] >> 4)) >> 15)*s->cc_ted.mixed_edges_coeff_3;
    /* Filter away any DC component */
    p = v - s->cc_ted.symbol_sync_dc_filter[1];
    s->cc_ted.symbol_sync_dc_filter[1] = s->cc_ted.symbol_sync_dc_filter[0];
    s->cc_ted.symbol_sync_dc_filter[0] = v;
    /* A little integration will now filter away much of the HF noise */
    s->cc_ted.baud_phase -= p;
    v = abs(s->cc_ted.baud_phase);
    if (v > 100*FP_FACTOR)
    {
        i = (v > 1000*FP_FACTOR)  ?  15  :  1;
        if (s->cc_ted.baud_phase < 0)
            i = -i;
        /*endif*/
        //printf("v = %10.5f %5d - %f %f %d %d\n", v, i, p, s->cc_ted.baud_phase, s->total_baud_timing_correction);
        s->eq_put_step += i;
        s->total_baud_timing_correction += i;
    }
    /*endif*/
#else
    /* Cross correlate */
    v = s->cc_ted.symbol_sync_low[1]*s->cc_ted.symbol_sync_high[0]*s->cc_ted.low_band_edge_coeff[2]
      - s->cc_ted.symbol_sync_low[0]*s->cc_ted.symbol_sync_high[1]*s->cc_ted.high_band_edge_coeff[2]
      + s->cc_ted.symbol_sync_low[1]*s->cc_ted.symbol_sync_high[1]*s->cc_ted.mixed_edges_coeff_3;
    /* Filter away any DC component  */
    p = v - s->cc_ted.symbol_sync_dc_filter[1];
    s->cc_ted.symbol_sync_dc_filter[1] = s->cc_ted.symbol_sync_dc_filter[0];
    s->cc_ted.symbol_sync_dc_filter[0] = v;
    /* A little integration will now filter away much of the HF noise */
    s->cc_ted.baud_phase -= p;
    v = fabsf(s->cc_ted.baud_phase);
    if (v > 100.0f)
    {
        i = (v > 200.0f)  ?  2  :  1;
        if (s->cc_ted.baud_phase < 0.0f)
            i = -i;
        /*endif*/
        //printf("v = %10.5f %5d - %f %f %d\n", v, i, p, s->cc_ted.baud_phase, s->total_baud_timing_correction);
        s->eq_put_step += i;
        s->total_baud_timing_correction += i;
    }
    /*endif*/
#endif
}
/*- End of function --------------------------------------------------------*/

static __inline__ void pri_symbol_sync(v34_rx_state_t *s)
{
    int i;
#if defined(SPANDSP_USE_FIXED_POINT)
    int32_t v;
    int32_t p;
#else
    float v;
    float p;
#endif

    /* This routine adapts the position of the half baud samples entering the equalizer. */

    /* This symbol sync scheme is based on the technique first described by Dominique Godard in
        Passband Timing Recovery in an All-Digital Modem Receiver
        IEEE TRANSACTIONS ON COMMUNICATIONS, VOL. COM-26, NO. 5, MAY 1978 */

    /* This is slightly rearranged from figure 3b of the Godard paper, as this saves a couple of
       maths operations */
#if defined(SPANDSP_USE_FIXED_POINT)
    /* TODO: The scalings used here need more thorough evaluation, to see if overflows are possible. */
    /* Cross correlate */
    v = (((s->pri_ted.symbol_sync_low[1] >> 5)*(s->pri_ted.symbol_sync_high[0] >> 4)) >> 15)*s->pri_ted.low_band_edge_coeff[2]
      - (((s->pri_ted.symbol_sync_low[0] >> 5)*(s->pri_ted.symbol_sync_high[1] >> 4)) >> 15)*s->pri_ted.high_band_edge_coeff[2]
      + (((s->pri_ted.symbol_sync_low[1] >> 5)*(s->pri_ted.symbol_sync_high[1] >> 4)) >> 15)*s->pri_ted.mixed_edges_coeff_3;
    /* Filter away any DC component */
    p = v - s->pri_ted.symbol_sync_dc_filter[1];
    s->pri_ted.symbol_sync_dc_filter[1] = s->pri_ted.symbol_sync_dc_filter[0];
    s->pri_ted.symbol_sync_dc_filter[0] = v;
    /* A little integration will now filter away much of the HF noise */
    s->pri_ted.baud_phase -= p;
    v = abs(s->pri_ted.baud_phase);
    if (v > 100*FP_FACTOR)
    {
        i = (v > 1000*FP_FACTOR)  ?  15  :  1;
        if (s->pri_ted.baud_phase < 0)
            i = -i;
        /*endif*/
        //printf("v = %10.5f %5d - %f %f %d %d\n", v, i, p, s->pri_ted.baud_phase, s->total_baud_timing_correction);
        s->eq_put_step += i;
        s->total_baud_timing_correction += i;
    }
    /*endif*/
#else
    /* Cross correlate */
    v = s->pri_ted.symbol_sync_low[1]*s->pri_ted.symbol_sync_high[0]*s->pri_ted.low_band_edge_coeff[2]
      - s->pri_ted.symbol_sync_low[0]*s->pri_ted.symbol_sync_high[1]*s->pri_ted.high_band_edge_coeff[2]
      + s->pri_ted.symbol_sync_low[1]*s->pri_ted.symbol_sync_high[1]*s->pri_ted.mixed_edges_coeff_3;
    /* Filter away any DC component  */
    p = v - s->pri_ted.symbol_sync_dc_filter[1];
    s->pri_ted.symbol_sync_dc_filter[1] = s->pri_ted.symbol_sync_dc_filter[0];
    s->pri_ted.symbol_sync_dc_filter[0] = v;
    /* A little integration will now filter away much of the HF noise */
    s->pri_ted.baud_phase -= p;
    v = fabsf(s->pri_ted.baud_phase);
    if (v > 100.0f)
    {
        i = (v > 200.0f)  ?  2  :  1;
        if (s->pri_ted.baud_phase < 0.0f)
            i = -i;
        /*endif*/
        //printf("v = %10.5f %5d - %f %f %d\n", v, i, p, s->pri_ted.baud_phase, s->total_baud_timing_correction);
        s->eq_put_step += i;
        s->total_baud_timing_correction += i;
    }
    /*endif*/
#endif
}
/*- End of function --------------------------------------------------------*/

static void create_godard_coeffs(ted_t *coeffs, float carrier, float baud_rate, float alpha)
{
    float low_edge;
    float high_edge;

    /* Create the coefficient set for an arbitrary Godard TED/symbol sync filter */
    low_edge = 2.0*M_PI*(carrier - baud_rate/2.0)/SAMPLE_RATE;
    high_edge = 2.0*M_PI*(carrier + baud_rate/2.0)/SAMPLE_RATE;

#if defined(SPANDSP_USE_FIXED_POINT)
    coeffs->low_band_edge_coeff[0] = ((int32_t)(FP_FACTOR*(2.0*alpha*cos(low_edge))));
    coeffs->high_band_edge_coeff[0] = ((int32_t)(FP_FACTOR*(2.0*alpha*cos(high_edge))));
    coeffs->low_band_edge_coeff[1] =
    coeffs->high_band_edge_coeff[1] = ((int32_t)(FP_FACTOR*(-alpha*alpha)));
    coeffs->low_band_edge_coeff[2] = ((int32_t)(FP_FACTOR*(-alpha*sin(low_edge))));
    coeffs->high_band_edge_coeff[2] = ((int32_t)(FP_FACTOR*(-alpha*sin(high_edge))));
    coeffs->mixed_edges_coeff_3 = ((int32_t)(FP_FACTOR*(-alpha*alpha*(sin(high_edge)*cos(low_edge) - sin(low_edge)*cos(high_edge)))));
#else
    coeffs->low_band_edge_coeff[0] = 2.0*alpha*cos(low_edge);
    coeffs->high_band_edge_coeff[0] = 2.0*alpha*cos(high_edge);
    coeffs->low_band_edge_coeff[1] =
    coeffs->high_band_edge_coeff[1] = -alpha*alpha;
    coeffs->low_band_edge_coeff[2] = -alpha*sin(low_edge);
    coeffs->high_band_edge_coeff[2] = -alpha*sin(high_edge);
    coeffs->mixed_edges_coeff_3 = -alpha*alpha*(sin(high_edge)*cos(low_edge) - sin(low_edge)*cos(high_edge));
#endif
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(float) v34_rx_carrier_frequency(v34_state_t *s)
{
    return dds_frequency(s->rx.v34_carrier_phase_rate);
}
/*- End of function --------------------------------------------------------*/

#if 0

SPAN_DECLARE(float) v34_rx_symbol_timing_correction(v34_state_t *s)
{
    return (float) s->rx.total_baud_timing_correction/((float) V34_RX_PULSESHAPER_COEFF_SETS*10.0f/3.0f);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(float) v34_rx_signal_power(v34_state_t *s)
{
    return power_meter_current_dbm0(&s->rx.power);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v34_equalizer_state(v34_state_t *s, complexf_t **coeffs)
{
    *coeffs = s->rx.eq_coeff;
    return V34_EQUALIZER_PRE_LEN + 1 + V34_EQUALIZER_POST_LEN;
}
/*- End of function --------------------------------------------------------*/

static void report_status_change(v34_rx_state_t *s, int status)
{
    if (s->status_handler)
        s->status_handler(s->status_user_data, status);
    else if (s->put_bit)
        s->put_bit(s->put_bit_user_data, status);
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static void equalizer_save(v34_rx_state_t *s)
{
    cvec_copyf(s->eq_coeff_save, s->eq_coeff, V34_EQUALIZER_PRE_LEN + 1 + V34_EQUALIZER_POST_LEN);
}
/*- End of function --------------------------------------------------------*/

static void equalizer_restore(v34_rx_state_t *s)
{
    cvec_copyf(s->eq_coeff, s->eq_coeff_save, V34_EQUALIZER_PRE_LEN + 1 + V34_EQUALIZER_POST_LEN);
    cvec_zerof(s->eq_buf, V34_EQUALIZER_MASK);

    s->eq_put_step = V34_RX_PULSESHAPER_COEFF_SETS*10/(3*2) - 1;
    s->eq_step = 0;
    s->eq_delta = EQUALIZER_SLOW_ADAPT_RATIO*EQUALIZER_DELTA/(V34_EQUALIZER_PRE_LEN + 1 + V34_EQUALIZER_POST_LEN);
}
/*- End of function --------------------------------------------------------*/

static void equalizer_reset(v34_rx_state_t *s)
{
    /* Start with an equalizer based on everything being perfect */
    cvec_zerof(s->eq_coeff, V34_EQUALIZER_PRE_LEN + 1 + V34_EQUALIZER_POST_LEN);
    s->eq_coeff[V34_EQUALIZER_PRE_LEN] = complex_sig_set(TRAINING_SCALE(3.0f), TRAINING_SCALE(0.0f));
    cvec_zerof(s->eq_buf, V34_EQUALIZER_MASK);

    s->eq_put_step = V34_RX_PULSESHAPER_COEFF_SETS*10/(3*2) - 1;
    s->eq_step = 0;
    s->eq_delta = EQUALIZER_DELTA/(V34_EQUALIZER_PRE_LEN + 1 + V34_EQUALIZER_POST_LEN);
}
/*- End of function --------------------------------------------------------*/

static __inline__ complexf_t equalizer_get(v34_rx_state_t *s)
{
    int i;
    int p;
    complexf_t z;
    complexf_t z1;

    /* Get the next equalized value. */
    z = zero;
    p = s->eq_step - 1;
    for (i = 0;  i < V34_EQUALIZER_PRE_LEN + 1 + V34_EQUALIZER_POST_LEN;  i++)
    {
        p = (p - 1) & V34_EQUALIZER_MASK;
        z1 = complex_mulf(&s->eq_coeff[i], &s->eq_buf[p]);
        z = complex_addf(&z, &z1);
    }
    /*endfor*/
    return z;
}
/*- End of function --------------------------------------------------------*/

static void tune_equalizer(v34_rx_state_t *s, const complexf_t *z, const complexf_t *target)
{
    int i;
    int p;
    complexf_t ez;
    complexf_t z1;

    /* Find the x and y mismatch from the exact constellation position. */
    ez = complex_subf(target, z);
    //span_log(s->logging, SPAN_LOG_FLOW, "Rx - Equalizer error %f\n", sqrt(ez.re*ez.re + ez.im*ez.im));
    ez.re *= s->eq_delta;
    ez.im *= s->eq_delta;

    p = s->eq_step - 1;
    for (i = 0;  i < V34_EQUALIZER_PRE_LEN + 1 + V34_EQUALIZER_POST_LEN;  i++)
    {
        p = (p - 1) & V34_EQUALIZER_MASK;
        z1 = complex_conjf(&s->eq_buf[p]);
        z1 = complex_mulf(&ez, &z1);
        s->eq_coeff[i] = complex_addf(&s->eq_coeff[i], &z1);
        /* Leak a little to tame uncontrolled wandering */
        s->eq_coeff[i].re *= 0.9999f;
        s->eq_coeff[i].im *= 0.9999f;
    }
    /*endfor*/
}
/*- End of function --------------------------------------------------------*/

static void track_carrier(v34_rx_state_t *s, const complexf_t *z, const complexf_t *target)
{
    float error;

    /* For small errors the imaginary part of the difference between the actual and the target
       positions is proportional to the phase error, for any particular target. However, the
       different amplitudes of the various target positions scale things. */
    error = z->im*target->re - z->re*target->im;

    s->v34_carrier_phase_rate += (int32_t) (s->carrier_track_i*error);
    s->carrier_phase += (int32_t) (s->carrier_track_p*error);
    //span_log(s->logging, SPAN_LOG_FLOW, "Rx - Im = %15.5f   f = %15.5f\n", error, dds_frequencyf(s->v34_carrier_phase_rate));
    //printf("XXX Im = %15.5f   f = %15.5f   %f %f %f %f (%f %f)\n", error, dds_frequencyf(s->v34_carrier_phase_rate), target->re, target->im, z->re, z->im, s->carrier_track_i, s->carrier_track_p);
}
/*- End of function --------------------------------------------------------*/

static __inline__ void put_bit(v34_rx_state_t *s, int bit)
{
    int out_bit;

    /* We need to strip the last part of the training - the test period of all 1s -
       before we let data go to the application. */
    if (s->training_stage == TRAINING_TX_STAGE_NORMAL_OPERATION_V34)
    {
        out_bit = descramble(s, bit);
        s->put_bit(s->put_bit_user_data, out_bit);
    }
    else if (s->training_stage == TRAINING_STAGE_TEST_ONES)
    {
        /* The bits during the final stage of training should be all ones. However,
           buggy modems mean you cannot rely on this. Therefore we don't bother
           testing for ones, but just rely on a constellation mismatch measurement. */
        out_bit = descramble(s, bit);
        //span_log(s->logging, SPAN_LOG_FLOW, "Rx - A 1 is really %d\n", out_bit);
    }
    /*endif*/
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

#if defined(SPANDSP_USE_FIXED_POINT)
#else
static void straight_line_fit(float *slope, float *intercept, const float x[], const float y[], int data_points)
{
    float sum_x;
    float sum_y;
    float sum_xy;
    float sum_x2;
    float slopex;
    int i;

    sum_x = 0.0f;
    sum_y = 0.0f;
    sum_xy = 0.0f;
    sum_x2 = 0.0f;
    for (i = 0;  i < data_points;  i++)
    {
        sum_x += x[i];
        sum_y += y[i];
        sum_xy += x[i]*y[i];
        sum_x2 += x[i]*x[i];
    }
    /*endfor*/
    slopex = (sum_xy - sum_x*sum_y/data_points)/(sum_x2 - sum_x*sum_x/data_points);
    if (slope)
        *slope = slopex;
    /*endif*/
    if (intercept)
        *intercept = (sum_y - slopex*sum_x)/data_points;
    /*endif*/
}
/*- End of function --------------------------------------------------------*/
#endif

static void slow_dft(complexf_t data[], int len)
{
    int i;
    int bin;
    float arg;
    complexf_t buf[len];

    for (i = 0;  i < len;  i++)
    {
        buf[i].re = data[i].re;
        buf[i].im = data[i].im;
    }
    /*endfor*/

    for (bin = 0;  bin <= len/2;  bin++)
    {
        data[bin].re =
        data[bin].im = 0.0;
        for (i = 0;  i < len;  i++)
        {
            arg = bin*2.0f*3.1415926535f*i/(float) len;
            data[bin].re -= buf[i].re*sinf(arg);
            data[bin].im += buf[i].re*cosf(arg);
        }
        /*endfor*/
    }
    /*endfor*/
}
/*- End of function --------------------------------------------------------*/

static int perform_l1_l2_analysis(v34_rx_state_t *s)
{
    /* Phase adjustments to compensate for the tones which are sent phase inverted */
    static const float adjust[25] =
    {
        0.0f,           /**/
        3.14159265f,    /* 300 */
        0.0f,           /**/
        0.0f,           /**/
        0.0f,           /**/
        42.0f,          /* Tone not sent */
        0.0f,           /* 1050 nominal line probe frequency */
        42.0f,          /* Tone not sent */
        0.0f,           /**/
        0.0f,           /**/
        3.14159265f,    /* 1650 */
        42.0f,          /* Tone not sent */
        0.0f,           /**/
        0.0f,           /**/
        3.14159265f,    /* 2250 */
        42.0f,          /* Tone not sent */
        0.0f,           /**/
        3.14159265f,    /* 2700 */
        0.0f,           /**/
        3.14159265f,    /* 3000 */
        3.14159265f,    /* 3150 */
        3.14159265f,    /* 3300 */
        3.14159265f,    /* 3450 */
        0.0f,           /**/
        0.0f            /**/
    };
    int i;
    int j;

    slow_dft(s->dft_buffer, LINE_PROBE_SAMPLES);
    /* Now resolve the analysis into gain and phase values for the bins which contain the tones */ 
    /* Base things around what happens at 1050Hz the first time through. */
    if (s->l1_l2_duration == 0)
        s->base_phase = atan2f(s->dft_buffer[21].im, s->dft_buffer[21].re);
    /*endif*/
    for (i = 0;  i < 25;  i++)
    {
        if (adjust[i] < 7.0f)
        {
            /* This tone should be present in the transmitted signal. */
            j = 3*(i + 1);
            s->l1_l2_gains[i] = sqrtf(s->dft_buffer[j].re*s->dft_buffer[j].re
                                    + s->dft_buffer[j].im*s->dft_buffer[j].im);
            s->l1_l2_phases[i] = fmodf(atan2f(s->dft_buffer[j].im, s->dft_buffer[j].re) - s->base_phase + adjust[i],
                                       3.14159265f);
        }
        else
        {
            /* This tone should not be present in the transmitted signal. */
            s->l1_l2_gains[i] = 0.0f;
            s->l1_l2_phases[i] = 0.0f;
        }
        /*endif*/
    }
    /*endfor*/
    for (i = 0;  i < 25;  i++)
    {
        printf("DFT %4d, %12.5f, %12.5f, %12.5f\n",
               i,
               (i + 1)*150.0f,
               s->l1_l2_gains[i],
               s->l1_l2_phases[i]);
        span_log(s->logging, SPAN_LOG_FLOW, "DFT %4d, %12.5f, %12.5f, %12.5f\n",
                 i,
                 (i + 1)*150.0f,
                 s->l1_l2_gains[i],
                 s->l1_l2_phases[i]);
    }
    /*endfor*/
    //straight_line_fit(&slope, &intercept, x, y, data_points);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void l1_l2_analysis_init(v34_rx_state_t *s)
{
    span_log(s->logging, SPAN_LOG_FLOW, "Rx - Expect L1/L2\n");
    s->dft_ptr = 0;
    s->base_phase = 42.0;
    s->l1_l2_duration = 0;
    s->current_demodulator = V34_MODULATION_L1_L2;
    s->stage = V34_RX_STAGE_L1_L2;
}
/*- End of function --------------------------------------------------------*/

static int l1_l2_analysis(v34_rx_state_t *s, const int16_t amp[], int len)
{
    int i;

    /* We need to work over whole cycles of the L1/L2 pattern, to avoid windowing and
       all its ills. One cycle takes 160/3 samples at 8000 samples/second, so we will
       process groups of 3 cycles, and run a Fourier transform every 160 samples (20ms).
       Since this is not a suitable length for an FFT we have to run a slow DFT. However,
       we don't do this for much of the time, so its not that big a deal. */
    for (i = 0;  i < len;  i++)
    {
        s->dft_buffer[s->dft_ptr].re = amp[i];
        s->dft_buffer[s->dft_ptr].im = 0.0f;
        if (++s->dft_ptr >= LINE_PROBE_SAMPLES)
        {
            /* We now have 160 samples, so process the 3 cycles we should have in the buffer. */
            perform_l1_l2_analysis(s);
            s->dft_ptr = 0;
            span_log(s->logging, SPAN_LOG_FLOW, "L1/L2 analysis x %d\n", s->l1_l2_duration);
            if (++s->l1_l2_duration > 20)
            {
                span_log(s->logging, SPAN_LOG_FLOW, "L1/L2 analysis done\n");
                s->received_event = V34_EVENT_L2_SEEN;
                s->current_demodulator = V34_MODULATION_TONES;
                s->stage = (s->calling_party)  ?  V34_RX_STAGE_TONE_A  :  V34_RX_STAGE_INFO1C;
            }
            /*endif*/
        }
        /*endif*/
    }
    /*endfor*/
    /* Also run this signal through the info analysis, so we pick up A or B tones */
    info_rx(s, amp, len);

    return 0;
}
/*- End of function --------------------------------------------------------*/

static void process_cc_half_baud(v34_rx_state_t *s, const complexf_t *sample)
{
    int i;
    int data_bits;
    mp_t mp;
    mph_t mph;
    uint32_t ang1;
    uint32_t ang2;
    uint32_t ang3;
    int bits[4];
    v34_state_t *t;

    /* This routine processes every half a baud, as we put things into the equalizer
       at the T/2 rate. This routine adapts the position of the half baud samples,
       which the caller takes. */
#if 0
    /* Add a sample to the equalizer's circular buffer, but don't calculate anything at this time. */
    s->eq_buf[s->eq_step] = *sample;
    s->eq_step = (s->eq_step + 1) & V34_EQUALIZER_MASK;
#endif

    /* On alternate insertions we have a whole baud and must process it. */
    if ((s->baud_half ^= 1))
        return;
    /*endif*/
    cc_symbol_sync(s);

    /* Slice the phase difference, to get a pair of data bits */
    ang1 = arctan2(sample->re, sample->im);
    ang2 = arctan2(s->last_sample.re, s->last_sample.im);
    ang3 = ang1 - ang2 + DDS_PHASE(45.0f);
    data_bits = ang3 >> 30;

    /* Descramble the data bits. */
    for (i = 0;  i < 2;  i++)
    {
        bits[i] = descramble(s, data_bits & 1);
        data_bits >>= 1;
    }
    /*endfor*/

    /* Scan for MP/MPh and HDLC messages. */
    for (i = 0;  i < 2;  i++)
    {
        s->bitstream = (s->bitstream << 1) | bits[i];
        if (s->mp_seen >= 2)
        {
            /* Real control channel data */
            s->put_bit(s->put_bit_user_data, bits[i]);
            continue;
        }
        /*endif*/
        if (s->mp_seen == 1  &&  (s->bitstream & 0xFFFFF) == 0xFFFFF)
        {
            /* E is 20 consecutive ones, which signals the end of the MPh messages,
               and the start of actual user data */
            if (s->duplex)
            {
                /* TODO: start data reception */
            }
            else
            {
                s->mp_seen = 2;
            }
            /*endif*/
        }
        else if ((s->bitstream & 0x7FFFE) == 0x7FFFC)
        {
            s->crc = 0xFFFF;
            s->bit_count = 0;
            s->mp_count = 17;
            /* Check the type bit, and set the expected length accordingly. */
            if (bits[i])
            {
                s->mp_len = 186 + 1;
                s->mp_and_fill_len = 186 + 1 + 1;
            }
            else
            {
                s->mp_len = 84 + 1;
                s->mp_and_fill_len = 84 + 3 + 1;
            }
            /*endif*/
        }
        /*endif*/
        if (s->mp_count >= 0)
        {
            s->mp_count++;
            /* Don't include the start bits in the CRC calculation. These occur every 16 bits of
               real data - i.e. every 17 bits, including the start bits themselves. */
            if (s->mp_count%17 != 0)
                s->crc = crc_itu16_bits(bits[i], 1, s->crc);
            /*endif*/
            s->bit_count++;
            if ((s->bit_count & 0x07) == 0)
                s->info_buf[(s->bit_count >> 3) - 1] = bit_reverse8(s->bitstream & 0xFF);
            /*endif*/
            if (s->mp_count >= s->mp_len)
            {
                if (s->mp_count == s->mp_len)
                {
                    /* This should be the end of the MPh message */
                    if (s->crc == 0)
                    {
                        if (s->duplex)
                        {
                            process_rx_mp(s, &mp, s->info_buf);
                            t = span_container_of(s, v34_state_t, rx);
                            if (mp.type == 1)
                            {
                                /* Set the precoder coefficients we are to use */
                                memcpy(&t->tx.precoder_coeffs, mp.precoder_coeffs, sizeof(t->tx.precoder_coeffs));
                            }
                            /*endif*/
                            switch (mp.trellis_size)
                            {
                            case V34_TRELLIS_16:
                                t->tx.conv_encode_table = v34_conv16_encode_table;
                                break;
                            case V34_TRELLIS_32:
                                t->tx.conv_encode_table = v34_conv32_encode_table;
                                break;
                            case V34_TRELLIS_64:
                                t->tx.conv_encode_table = v34_conv64_encode_table;
                                break;
                            default:
                                span_log(&t->logging, SPAN_LOG_FLOW, "Rx - Unexpected trellis size code %d\n", mp.trellis_size);
                                break;
                            }
                            /*endswitch*/
                        }
                        else
                        {
                            process_rx_mph(s, &mph, s->info_buf);
                            t = span_container_of(s, v34_state_t, rx);
                            if (mph.type == 1)
                            {
                                /* Set the precoder coefficients we are to use */
                                memcpy(&t->tx.precoder_coeffs, mph.precoder_coeffs, sizeof(t->tx.precoder_coeffs));
                            }
                            /*endif*/
                            switch (mph.trellis_size)
                            {
                            case V34_TRELLIS_16:
                                t->tx.conv_encode_table = v34_conv16_encode_table;
                                break;
                            case V34_TRELLIS_32:
                                t->tx.conv_encode_table = v34_conv32_encode_table;
                                break;
                            case V34_TRELLIS_64:
                                t->tx.conv_encode_table = v34_conv64_encode_table;
                                break;
                            default:
                                span_log(&t->logging, SPAN_LOG_FLOW, "Rx - Unexpected trellis size code %d\n", mph.trellis_size);
                                break;
                            }
                            /*endswitch*/
                        }
                        /*endif*/
                        s->mp_seen = 1;
                    }
                    /*endif*/
                }
                /*endif*/
                /* Allow for the fill bits before ending the MP message */
                if (s->mp_count == s->mp_and_fill_len)
                    s->mp_count = -1;
                /*endif*/
            }
            /*endif*/
        }
        /*endif*/
    }
    /*endfor*/

    s->last_sample = *sample;
}
/*- End of function --------------------------------------------------------*/

static int cc_rx(v34_rx_state_t *s, const int16_t amp[], int len)
{
    int i;
    int step;
#if defined(SPANDSP_USE_FIXED_POINT)
    complexi16_t z;
    complexi16_t zz;
    complexi16_t sample;
#else
    complexf_t z;
    complexf_t zz;
    complexf_t sample;
#endif
    float ii;
    float qq;
    float v;

    step = 6;
printf("XYX0 %d\n", len);
    for (i = 0;  i < len;  i++)
    {
        s->rrc_filter[s->rrc_filter_step] = amp[i];
        if (++s->rrc_filter_step >= V34_RX_FILTER_STEPS)
            s->rrc_filter_step = 0;
        /*endif*/

        //if ((power = signal_detect(s, amp[i])) == 0)
        //    continue;
        ///*endif*/
        //if (s->training_stage == TRAINING_STAGE_PARKED)
        //    continue;
        ///*endif*/
        /* Only spend effort processing this data if the modem is not
           parked, after training failure. */
        s->eq_put_step -= RX_PULSESHAPER_2400_COEFF_SETS;
        step = -s->eq_put_step;
        if (step > RX_PULSESHAPER_2400_COEFF_SETS - 1)
            step = RX_PULSESHAPER_2400_COEFF_SETS - 1;
        /*endif*/
        while (step < 0)
            step += RX_PULSESHAPER_2400_COEFF_SETS;
        /*endwhile*/
        if (s->calling_party)
        {
#if defined(SPANDSP_USE_FIXED_POINT)
            ii = vec_circular_dot_prodi16(s->rrc_filter, rx_pulseshaper_2400_re[step], V34_RX_FILTER_STEPS, s->rrc_filter_step);
#else
            ii = vec_circular_dot_prodf(s->rrc_filter, rx_pulseshaper_2400_re[step], V34_RX_FILTER_STEPS, s->rrc_filter_step);
#endif
        }
        else
        {
#if defined(SPANDSP_USE_FIXED_POINT)
            ii = vec_circular_dot_prodi16(s->rrc_filter, rx_pulseshaper_1200_re[step], V34_RX_FILTER_STEPS, s->rrc_filter_step);
#else
            ii = vec_circular_dot_prodf(s->rrc_filter, rx_pulseshaper_1200_re[step], V34_RX_FILTER_STEPS, s->rrc_filter_step);
#endif
        }
        /*endif*/
#if defined(SPANDSP_USE_FIXED_POINT)
        //sample.re = (ii*(int32_t) s->agc_scaling) >> 15;
        sample.re = ii*s->agc_scaling;
#else
        sample.re = ii*s->agc_scaling;
#endif
        /* Symbol timing synchronisation band edge filters */
        /* Low Nyquist band edge filter */
        v = s->cc_ted.symbol_sync_low[0]*s->cc_ted.low_band_edge_coeff[0] + s->cc_ted.symbol_sync_low[1]*s->cc_ted.low_band_edge_coeff[1] + sample.re;
        s->cc_ted.symbol_sync_low[1] = s->cc_ted.symbol_sync_low[0];
        s->cc_ted.symbol_sync_low[0] = v;
        /* High Nyquist band edge filter */
        v = s->cc_ted.symbol_sync_high[0]*s->cc_ted.high_band_edge_coeff[0] + s->cc_ted.symbol_sync_high[1]*s->cc_ted.high_band_edge_coeff[1] + sample.re;
        s->cc_ted.symbol_sync_high[1] = s->cc_ted.symbol_sync_high[0];
        s->cc_ted.symbol_sync_high[0] = v;

        /* Put things into the equalization buffer at T/2 rate. The symbol synchcronisation
           will fiddle the step to align this with the symbols. */
        if (s->eq_put_step <= 0)
        {
            /* Only AGC until we have locked down the setting. */
            //if (s->agc_scaling_save == 0.0f)
#if defined(SPANDSP_USE_FIXED_POINT)
            //s->agc_scaling = saturate16(((int32_t) (1024.0f*FP_SCALE(2.17f)))/fixed_sqrt32(power));
#else
            //s->agc_scaling = (FP_SCALE(2.17f)/RX_PULSESHAPER_GAIN)/fixed_sqrt32(power);
#endif
            s->eq_put_step += RX_PULSESHAPER_2400_COEFF_SETS*40/(3*2);
            if (s->calling_party)
            {
#if defined(SPANDSP_USE_FIXED_POINT)
                qq = vec_circular_dot_prodi16(s->rrc_filter, rx_pulseshaper_2400_im[step], V34_RX_FILTER_STEPS, s->rrc_filter_step);
#else
                qq = vec_circular_dot_prodf(s->rrc_filter, rx_pulseshaper_2400_im[step], V34_RX_FILTER_STEPS, s->rrc_filter_step);
#endif
            }
            else
            {
#if defined(SPANDSP_USE_FIXED_POINT)
                qq = vec_circular_dot_prodi16(s->rrc_filter, rx_pulseshaper_1200_im[step], V34_RX_FILTER_STEPS, s->rrc_filter_step);
#else
                qq = vec_circular_dot_prodf(s->rrc_filter, rx_pulseshaper_1200_im[step], V34_RX_FILTER_STEPS, s->rrc_filter_step);
#endif
            }
            /*endif*/
#if defined(SPANDSP_USE_FIXED_POINT)
            //sample.im = (qq*(int32_t) s->agc_scaling) >> 15;
            sample.im = qq*s->agc_scaling;
            z = dds_lookup_complexi16(s->carrier_phase);
#else
            sample.im = qq*s->agc_scaling;
            z = dds_lookup_complexf(s->carrier_phase);
#endif
            zz.re = sample.re*z.re - sample.im*z.im;
            zz.im = -sample.re*z.im - sample.im*z.re;
            process_cc_half_baud(s, &zz);

            //angle = arctan2(zz.im, zz.re);
            //printf("XYX1 %10.5f %10.5f\n", atan2(zz.re, zz.im), sqrt(zz.re*zz.re + zz.im*zz.im));
            printf("XYX2 %10.5f %10.5f\n", zz.re, zz.im);
        }
        /*endif*/
#if defined(SPANDSP_USE_FIXED_POINT)
        dds_advance(&s->carrier_phase, s->v34_carrier_phase_rate);
#else
        dds_advancef(&s->carrier_phase, s->v34_carrier_phase_rate);
#endif
    }
    /*endfor*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void process_primary_half_baud(v34_rx_state_t *s, const complexf_t *sample)
{
#if 0
    int i;
    complexf_t z;
    complexf_t zz;
#if defined(SPANDSP_USE_FIXED_POINT)
    const complexi_t *target;
#else
    const complexf_t *target;
#endif
    float v;
    float p;
    int bit;
    int j;
    int32_t angle;
    int32_t ang;
    int constellation_state;
#endif

    /* This routine processes every half a baud, as we put things into the equalizer at the T/2 rate.
       This routine adapts the position of the half baud samples, which the caller takes. */
#if 0
    /* Add a sample to the equalizer's circular buffer, but don't calculate anything at this time. */
    s->eq_buf[s->eq_step] = *sample;
    s->eq_step = (s->eq_step + 1) & V34_EQUALIZER_MASK;
#endif

    /* On alternate insertions we have a whole baud and must process it. */
    if ((s->baud_half ^= 1))
        return;
    /*endif*/
    pri_symbol_sync(s);

    s->last_sample = *sample;

#if 0
    z = equalizer_get(s);

    switch (s->training_stage)
    {
    case TRAINING_TX_STAGE_NORMAL_OPERATION_V34:
        /* Normal operation. */
        constellation_state = decode_baud(s, &z);
        target = &s->constellation[constellation_state];
        break;
    default:
        /* We failed to train! */
        /* Park here until the carrier drops. */
        target = &z;
        break;
    }
    /*endswitch*/
    if (s->qam_report)
        s->qam_report(s->qam_user_data, &z, target, constellation_state);
    /*endif*/
#endif
}
/*- End of function --------------------------------------------------------*/

static int primary_channel_rx(v34_rx_state_t *s, const int16_t amp[], int len)
{
    int i;
    int step;
#if defined(SPANDSP_USE_FIXED_POINT)
    complexi16_t z;
    complexi16_t zz;
    complexi16_t sample;
#else
    complexf_t z;
    complexf_t zz;
    complexf_t sample;
#endif
    float ii;
    float qq;
    float v;
    /* The following lead to integer values for the rx increments per symbol, for each of the 6 baud rates */
    static const int steps_per_baud[6] =
    {
        192*8000/2400,
        192*8000*7/(2400*8),
        189*8000*6/(2400*7),
        192*8000*4/(2400*5),
        192*8000*3/(2400*4),
        192*8000*7/(2400*10)
    };

    s->baud_rate = 5;
    s->shaper_re = v34_rx_shapers_re[s->baud_rate][s->high_carrier];
    s->shaper_im = v34_rx_shapers_im[s->baud_rate][s->high_carrier];
    s->shaper_sets = steps_per_baud[s->baud_rate];
    s->v34_carrier_phase_rate = dds_phase_ratef(carrier_frequency(s->baud_rate, 0));
printf("XYX0 %d\n", len);
    for (i = 0;  i < len;  i++)
    {
        s->rrc_filter[s->rrc_filter_step] = amp[i];
        if (++s->rrc_filter_step >= V34_RX_FILTER_STEPS)
            s->rrc_filter_step = 0;
        /*endif*/
        //if ((power = signal_detect(s, amp[i])) == 0)
        //    continue;
        ///*endif*/
        //if (s->training_stage == TRAINING_STAGE_PARKED)
        //    continue;
        ///*endif*/
        /* Only spend effort processing this data if the modem is not parked, after training failure. */
        s->eq_put_step -= V34_RX_PULSESHAPER_COEFF_SETS;
        step = -s->eq_put_step;
        if (step > V34_RX_PULSESHAPER_COEFF_SETS - 1)
            step = V34_RX_PULSESHAPER_COEFF_SETS - 1;
        /*endif*/
        while (step < 0)
            step += V34_RX_PULSESHAPER_COEFF_SETS;
        /*endwhile*/
#if defined(SPANDSP_USE_FIXED_POINT)
        ii = vec_circular_dot_prodi16(s->rrc_filter, (*s->shaper_re)[step], V34_RX_FILTER_STEPS, s->rrc_filter_step);
#else
        ii = vec_circular_dot_prodf(s->rrc_filter, (*s->shaper_re)[step], V34_RX_FILTER_STEPS, s->rrc_filter_step);
#endif
#if defined(SPANDSP_USE_FIXED_POINT)
        //sample.re = (ii*(int32_t) s->agc_scaling) >> 15;
        sample.re = ii*s->agc_scaling;
#else
        sample.re = ii*s->agc_scaling;
#endif
        /* Symbol timing synchronisation band edge filters */
        /* Low Nyquist band edge filter */
        v = s->pri_ted.symbol_sync_low[0]*s->pri_ted.low_band_edge_coeff[0] + s->pri_ted.symbol_sync_low[1]*s->pri_ted.low_band_edge_coeff[1] + sample.re;
        s->pri_ted.symbol_sync_low[1] = s->pri_ted.symbol_sync_low[0];
        s->pri_ted.symbol_sync_low[0] = v;
        /* High Nyquist band edge filter */
        v = s->pri_ted.symbol_sync_high[0]*s->pri_ted.high_band_edge_coeff[0] + s->pri_ted.symbol_sync_high[1]*s->pri_ted.high_band_edge_coeff[1] + sample.re;
        s->pri_ted.symbol_sync_high[1] = s->pri_ted.symbol_sync_high[0];
        s->pri_ted.symbol_sync_high[0] = v;

        /* Put things into the equalization buffer at T/2 rate. The symbol synchcronisation
           will fiddle the step to align this with the symbols. */
        if (s->eq_put_step <= 0)
        {
            /* Only AGC until we have locked down the setting. */
#if defined(SPANDSP_USE_FIXED_POINT)
            //if (s->agc_scaling_save == 0)
            //    s->agc_scaling = saturate16(((int32_t) (1024.0f*FP_SCALE(2.17f)))/fixed_sqrt32(power));
            ///*endif*/
#else
            //if (s->agc_scaling_save == 0.0f)
            //    s->agc_scaling = (FP_SCALE(2.17f)/RX_PULSESHAPER_GAIN)/fixed_sqrt32(power);
            ///*endif*/
#endif
            s->eq_put_step += s->shaper_sets;
#if defined(SPANDSP_USE_FIXED_POINT)
            qq = vec_circular_dot_prodi16(s->rrc_filter, (*s->shaper_im)[step], V34_RX_FILTER_STEPS, s->rrc_filter_step);
#else
            qq = vec_circular_dot_prodf(s->rrc_filter, (*s->shaper_im)[step], V34_RX_FILTER_STEPS, s->rrc_filter_step);
#endif
#if defined(SPANDSP_USE_FIXED_POINT)
            //sample.im = (qq*(int32_t) s->agc_scaling) >> 15;
            sample.im = qq*s->agc_scaling;
            z = dds_lookup_complexi16(s->carrier_phase);
#else
            sample.im = qq*s->agc_scaling;
            z = dds_lookup_complexf(s->carrier_phase);
#endif
            zz.re = sample.re*z.re - sample.im*z.im;
            zz.im = -sample.re*z.im - sample.im*z.re;
            process_primary_half_baud(s, &zz);

            //angle = arctan2(zz.im, zz.re);
            printf("XYX1 %10.5f %10.5f\n", atan2(zz.re, zz.im), sqrt(zz.re*zz.re + zz.im*zz.im));
            printf("XYX2 %10.5f %10.5f\n", zz.re, zz.im);
        }
        /*endif*/
#if defined(SPANDSP_USE_FIXED_POINT)
        dds_advance(&s->carrier_phase, s->v34_carrier_phase_rate);
#else
        dds_advancef(&s->carrier_phase, s->v34_carrier_phase_rate);
#endif
    }
    /*endfor*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

/* Keep this global until the modem is VERY well tested */
SPAN_DECLARE(void) v34_put_mapping_frame(v34_rx_state_t *s, int16_t bits[16])
{
    int i;
    int j;
    int constel;
    int invert;
    complexi16_t c;
    complexi16_t p;
    complexi16_t u;
    complexi16_t v;
    complexi16_t y[2];

    /* Put the four 4D symbols (eight 2D symbols) of a mapping frame */
#define BYPASS_VITERBI
    for (i = 0;  i < 8;  i++)
    {
        s->xt[0].re = bits[2*i];
        s->xt[0].im = bits[2*i + 1];
//printf("AMZ %p [%6d, %6d] [%8.3f, %8.3f]\n", s, s->xt[0].re, s->xt[0].im, FP_Q9_7_TO_F(s->xt[0].re), FP_Q9_7_TO_F(s->xt[0].im));
        s->yt = prediction_error_filter(s);
        quantize_n_ways(s->xy[i & 1], &s->yt);
//printf("CCC %p [%8.3f, %8.3f] [%8.3f, %8.3f] [%8.3f, %8.3f] [%8.3f, %8.3f]\n",
//       s,
//       FP_Q9_7_TO_F(s->xy[i & 1][0].re),
//       FP_Q9_7_TO_F(s->xy[i & 1][0].im),
//       FP_Q9_7_TO_F(s->xy[i & 1][1].re),
//       FP_Q9_7_TO_F(s->xy[i & 1][1].im),
//       FP_Q9_7_TO_F(s->xy[i & 1][2].re),
//       FP_Q9_7_TO_F(s->xy[i & 1][2].im),
//       FP_Q9_7_TO_F(s->xy[i & 1][3].re),
//       FP_Q9_7_TO_F(s->xy[i & 1][3].im));
        viterbi_calculate_candidate_errors(s->viterbi.error[i & 1], s->xy[i & 1], &s->yt);
#if defined(BYPASS_VITERBI)
        y[i & 1].re = s->xt[0].re;
        y[i & 1].im = s->xt[0].im;
//printf("CCD %p [%8.3f, %8.3f]\n", s, FP_Q9_7_TO_F(y[i & 1].re), FP_Q9_7_TO_F(y[i & 1].im));
#endif
        if ((i & 1))
        {
            /* Deal with super-frame sync inversion */
            if ((s->data_frame*8 + s->step_2d)%(4*s->parms.p) == 0)
                invert = (0x5FEE >> s->v0_pattern++) & 1;
            else
                invert = false;
            /*endif*/
            viterbi_calculate_branch_errors(&s->viterbi, s->xy, invert);
            viterbi_update_path_metrics(&s->viterbi);
//printf("EEE %p %4d %4d %4d %4d %4d %4d %4d %4d (%d)\n",
//       s,
//       s->viterbi.branch_error[0],
//       s->viterbi.branch_error[1],
//       s->viterbi.branch_error[2],
//       s->viterbi.branch_error[3],
//       s->viterbi.branch_error[4],
//       s->viterbi.branch_error[5],
//       s->viterbi.branch_error[6],
//       s->viterbi.branch_error[7],
//       s->viterbi.windup);
#if defined(BYPASS_VITERBI)
            {
#else
            if (s->viterbi.windup)
            {
                /* Wait for the Viterbi buffer to fill with symbols. */
                s->viterbi.windup--;
            }
            else
            {
                viterbi_trace_back(&s->viterbi, y);
#endif
                /* We now have two points in y to be decoded. They are in Q9.7 format. */
//printf("AAA %p [%8.3f, %8.3f] [%8.3f, %8.3f]\n",
//       s,
//       FP_Q9_7_TO_F(y[0].re),
//       FP_Q9_7_TO_F(y[0].im),
//       FP_Q9_7_TO_F(y[1].re),
//       FP_Q9_7_TO_F(y[1].im));
                for (j = 0;  j < 2;  j++)
                {
                    p = precoder_rx_filter(s);

                    c = quantize_rx(s, &p);
                    s->x[0].re = y[j].re - p.re;
                    s->x[0].im = y[j].im - p.im;
                    u.re = (y[j].re >> 7) - c.re;
                    u.im = (y[j].im >> 7) - c.im;

                    s->ww[j + 1] = get_binary_subset_label(&u);
                    v = rotate90_counterclockwise(&u, s->ww[j + 1]);
                    constel = get_inverse_constellation_point(&v);
//printf("AMQ %p %d [%d, %d] [%d, %d] %d\n", s, constel, v.re, v.im, u.re, u.im, s->ww[j + 1]);
//printf("AMQ %p [%6d, %6d] (%d) [%6d, %6d] [%8.3f, %8.3f]\n", s, v.re, v.im, s->ww[j + 1], u.re, u.im, FP_Q9_7_TO_F(y[j].re), FP_Q9_7_TO_F(y[j].im));
                    s->qbits[s->step_2d + j] = constel & s->parms.q_mask;
                    s->mjk[s->step_2d + j] = constel >> s->parms.q;
                }
                /*endfor*/
                /* Compute the I bits */
                s->ibits[s->step_2d >> 1] = (((s->ww[1] - s->ww[0]) & 3) << 1)
                                          | (((s->ww[2] - s->ww[1]) >> 1) & 1);
                s->ww[0] = s->ww[1];
                s->step_2d += 2;
                if (s->step_2d == 8)
                {
                    shell_unmap(s);
                    pack_output_bitstream(s);
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
//printf("ZAQ data frame %d, super frame %d\n", s->data_frame, s->super_frame);
                    s->step_2d = 0;
                }
                /*endif*/
            }
            /*endif*/
            s->viterbi.ptr = (s->viterbi.ptr + 1) & 0xF;
        }
        /*endif*/
    }
    /*endfor*/
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v34_rx_fillin(v34_state_t *s, int len)
{
    int i;

    /* We want to sustain the current state (i.e carrier on<->carrier off), and
       try to sustain the carrier phase. We should probably push the filters, as well */
    span_log(&s->logging, SPAN_LOG_FLOW, "Rx - Fill-in %d samples\n", len);
    for (i = 0;  i < len;  i++)
    {
#if defined(SPANDSP_USE_FIXED_POINT)
        dds_advance(&s->rx.carrier_phase, s->rx.v34_carrier_phase_rate);
#else
        dds_advancef(&s->rx.carrier_phase, s->rx.v34_carrier_phase_rate);
#endif
    }
    /*endfor*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v34_rx(v34_state_t *s, const int16_t amp[], int len)
{
    int leny;
    int lenx;

    leny = 0;
    lenx = -1;
    do
    {
        switch (s->rx.current_demodulator)
        {
        case V34_MODULATION_V34:
            lenx = primary_channel_rx(&s->rx, &amp[leny], len - leny);
            break;
        case V34_MODULATION_CC:
            lenx = cc_rx(&s->rx, &amp[leny], len - leny);
            break;
        case V34_MODULATION_L1_L2:
            lenx = l1_l2_analysis(&s->rx, &amp[leny], len - leny);
            break;
        case V34_MODULATION_TONES:
            lenx = info_rx(&s->rx, &amp[leny], len - leny);
            break;
        }
        /*endswitch*/
        leny += lenx;
        /* Add step by step, so each segment is seen up to date */
        s->rx.sample_time += lenx;
    }
    while (lenx > 0  &&  leny < len);
    /* If there is any residue, this should be the end of operation of the modem,
       so we don't really need to add that residue to the sample time. */
    return leny;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v34_rx_set_signal_cutoff(v34_state_t *s, float cutoff)
{
    /* The 0.4 factor allows for the gain of the DC blocker */
    s->rx.carrier_on_power = (int32_t) (power_meter_level_dbm0(cutoff + 2.5f)*0.4f);
    s->rx.carrier_off_power = (int32_t) (power_meter_level_dbm0(cutoff - 2.5f)*0.4f);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v34_set_put_bit(v34_state_t *s, span_put_bit_func_t put_bit, void *user_data)
{
    s->rx.put_bit = put_bit;
    s->rx.put_bit_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v34_set_put_aux_bit(v34_state_t *s, span_put_bit_func_t put_bit, void *user_data)
{
    s->rx.put_aux_bit = put_bit;
    s->rx.put_aux_bit_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

int v34_rx_restart(v34_state_t *s, int baud_rate, int bit_rate, int high_carrier)
{
    int i;

    s->rx.baud_rate = baud_rate;
    s->rx.bit_rate = bit_rate;
    s->rx.high_carrier = high_carrier;

    s->rx.v34_carrier_phase_rate = dds_phase_ratef(carrier_frequency(s->rx.baud_rate, s->rx.high_carrier));
    s->rx.cc_carrier_phase_rate = dds_phase_ratef((s->calling_party)  ?  2400.0f  :  1200.0f);
    v34_set_working_parameters(&s->rx.parms, s->rx.baud_rate, s->rx.bit_rate, true);

    s->rx.high_sample = 0;
    s->rx.low_samples = 0;
    s->rx.carrier_drop_pending = false;

    power_meter_init(&s->rx.power, 4);

    s->rx.carrier_phase = 0;
    s->rx.agc_scaling_save = 0.0f;
    s->rx.agc_scaling = 0.0017f/V34_RX_PULSESHAPER_GAIN;
    //equalizer_reset(&s->rx);
    s->rx.carrier_track_i = 5000.0f;
    s->rx.carrier_track_p = 40000.0f;

    /* Create a default symbol sync filter */
    create_godard_coeffs(&s->rx.pri_ted,
                         s->rx.high_carrier,
                         s->rx.baud_rate,
                         0.99f);
    create_godard_coeffs(&s->rx.cc_ted,
                         (s->calling_party)  ?  2400.0f  :  1200.0f,
                         600,
                         0.99f);
    /* Initialise the working data for symbol timing synchronisation */
#if defined(SPANDSP_USE_FIXED_POINT)
    for (i = 0;  i < 2;  i++)
    {
        s->rx.pri_ted.symbol_sync_low[i] = 0;
        s->rx.pri_ted.symbol_sync_high[i] = 0;
        s->rx.pri_ted.symbol_sync_dc_filter[i] = 0;
    }
    /*endfor*/
    s->rx.pri_ted.baud_phase = 0;
    for (i = 0;  i < 2;  i++)
    {
        s->rx.cc_ted.symbol_sync_low[i] = 0;
        s->rx.cc_ted.symbol_sync_high[i] = 0;
        s->rx.cc_ted.symbol_sync_dc_filter[i] = 0;
    }
    /*endfor*/
    s->rx.cc_ted.baud_phase = 0;
#else
    for (i = 0;  i < 2;  i++)
    {
        s->rx.pri_ted.symbol_sync_low[i] = 0.0f;
        s->rx.pri_ted.symbol_sync_high[i] = 0.0f;
        s->rx.pri_ted.symbol_sync_dc_filter[i] = 0.0f;
    }
    /*endfor*/
    s->rx.pri_ted.baud_phase = 0.0f;
    for (i = 0;  i < 2;  i++)
    {
        s->rx.cc_ted.symbol_sync_low[i] = 0.0f;
        s->rx.cc_ted.symbol_sync_high[i] = 0.0f;
        s->rx.cc_ted.symbol_sync_dc_filter[i] = 0.0f;
    }
    /*endfor*/
    s->rx.cc_ted.baud_phase = 0.0f;
#endif
    s->rx.baud_half = 0;

    s->rx.bitstream = 0;
    s->rx.bit_count = 0;
    s->rx.duration = 0;
    s->rx.blip_duration = 0;
    s->rx.last_angles[0] = 0;
    s->rx.last_angles[1] = 0;
    s->rx.total_baud_timing_correction = 0;

    s->rx.stage = V34_RX_STAGE_INFO0;
    /* The next info message will be INFO0 or INFOH, depending whether we are in half or full duplex mode. */
    s->rx.target_bits = (s->rx.duplex)  ?  (49 - (4 + 8 + 4))  :  (51 - (4 + 8 + 4));

    s->rx.mp_count = -1;
    s->rx.mp_len = 0;
    s->rx.mp_seen = -1;

    s->rx.viterbi.ptr = 0;
    s->rx.viterbi.windup = 15;

    s->rx.eq_put_step = RX_PULSESHAPER_2400_COEFF_SETS*40/(3*2) - 1;
    s->rx.eq_step = 0;
    s->rx.scramble_reg = 0;

    s->rx.current_demodulator = V34_MODULATION_TONES;
    s->rx.viterbi.conv_decode_table = v34_conv16_decode_table;

    s->rx.v0_pattern = 0;
    s->rx.super_frame = 0;
    s->rx.data_frame = 0;
    s->rx.s_bit_cnt = 0;
    s->rx.aux_bit_cnt = 0;

    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v34_set_qam_report_handler(v34_state_t *s, qam_report_handler_t handler, void *user_data)
{
    s->rx.qam_report = handler;
    s->rx.qam_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
