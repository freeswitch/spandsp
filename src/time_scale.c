/*
 * SpanDSP - a series of DSP components for telephony
 *
 * time_scale.c - Time scaling for linear speech data
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004 Steve Underwood
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

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <limits.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#else
#include "spandsp3/stdbool.h"
#endif
#include "floating_fudge.h"

#include "spandsp3/telephony.h"
#include "spandsp3/alloc.h"
#include "spandsp3/fast_convert.h"
#include "spandsp3/vector_int.h"
#include "spandsp3/saturated.h"
#include "spandsp3/time_scale.h"

#include "spandsp3/private/time_scale.h"

/*
    Time scaling for speech, based on the Pointer Interval Controlled
    OverLap and Add (PICOLA) method, developed by Morita Naotaka.
 */

static __inline__ int amdf_pitch(int min_pitch, int max_pitch, int16_t amp[], int len)
{
    int i;
    int j;
    int acc;
    int min_acc;
    int pitch;

    pitch = min_pitch;
    min_acc = INT_MAX;
    for (i = max_pitch;  i <= min_pitch;  i++)
    {
        acc = 0;
        for (j = 0;  j < len;  j++)
            acc += abs(amp[i + j] - amp[j]);
        /*endfor*/
        if (acc < min_acc)
        {
            min_acc = acc;
            pitch = i;
        }
        /*endif*/
    }
    /*endfor*/
    return pitch;
}
/*- End of function --------------------------------------------------------*/

static __inline__ void overlap_add(int16_t amp1[], int16_t amp2[], int len)
{
    int i;
    float weight;
    float step;

    step = 1.0f/len;
    weight = 0.0f;
    for (i = 0;  i < len;  i++)
    {
        /* TODO: saturate */
        amp1[i] = (int16_t) ((float) amp2[i]*(1.0f - weight) + (float) amp1[i]*weight);
        weight += step;
    }
    /*endfor*/
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) time_scale_rate(time_scale_state_t *s, float playout_rate)
{
    if (playout_rate <= 0.0f)
        return -1;
    /*endif*/
    if (playout_rate >= 0.99f  &&  playout_rate <= 1.01f)
    {
        /* Treat rate close to normal speed as exactly normal speed, and
           avoid divide by zero, and other numerical problems. */
        playout_rate = 1.0f;
    }
    else if (playout_rate < 1.0f)
    {
        s->rcomp = playout_rate/(1.0f - playout_rate);
    }
    else
    {
        s->rcomp = 1.0f/(playout_rate - 1.0f);
    }
    /*endif*/
    s->playout_rate = playout_rate;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) time_scale(time_scale_state_t *s, int16_t out[], int16_t in[], int len)
{
    double lcpf;
    int pitch;
    int out_len;
    int in_len;
    int k;

    out_len = 0;
    in_len = 0;

    if (s->playout_rate == 1.0f)
    {
        vec_copyi16(out, in, len);
        return len;
    }
    /*endif*/

    /* Top up the buffer */
    if (s->fill + len < s->buf_len)
    {
        /* Cannot continue without more samples */
        /* Save the residual signal for next time. */
        vec_copyi16(&s->buf[s->fill], in, len);
        s->fill += len;
        return 0;
    }
    /*endif*/
    k = s->buf_len - s->fill;
    vec_copyi16(&s->buf[s->fill], in, k);
    in_len += k;
    s->fill = s->buf_len;
    while (s->fill == s->buf_len)
    {
        while (s->lcp >= s->buf_len)
        {
            vec_copyi16(&out[out_len], s->buf, s->buf_len);
            out_len += s->buf_len;
            if (len - in_len < s->buf_len)
            {
                /* Cannot continue without more samples */
                /* Save the residual signal for next time. */
                vec_copyi16(s->buf, &in[in_len], len - in_len);
                s->fill = len - in_len;
                s->lcp -= s->buf_len;
                return out_len;
            }
            /*endif*/
            vec_copyi16(s->buf, &in[in_len], s->buf_len);
            in_len += s->buf_len;
            s->lcp -= s->buf_len;
        }
        /*endwhile*/
        if (s->lcp > 0)
        {
            vec_copyi16(&out[out_len], s->buf, s->lcp);
            out_len += s->lcp;
            vec_movei16(s->buf, &s->buf[s->lcp], s->buf_len - s->lcp);
            if (len - in_len < s->lcp)
            {
                /* Cannot continue without more samples */
                /* Save the residual signal for next time. */
                vec_copyi16(&s->buf[s->buf_len - s->lcp], &in[in_len], len - in_len);
                s->fill = s->buf_len - s->lcp + len - in_len;
                s->lcp = 0;
                return out_len;
            }
            /*endif*/
            vec_copyi16(&s->buf[s->buf_len - s->lcp], &in[in_len], s->lcp);
            in_len += s->lcp;
            s->lcp = 0;
        }
        /*endif*/
        pitch = amdf_pitch(s->min_pitch, s->max_pitch, s->buf, s->min_pitch);
        lcpf = (double) pitch*s->rcomp;
        /* Nudge around to compensate for fractional samples */
        s->lcp = (int) lcpf;
        /* Note that s->lcp and lcpf are not the same, as lcpf has a fractional part, and s->lcp doesn't */
        s->rate_nudge += s->lcp - lcpf;
        if (s->rate_nudge >= 0.5f)
        {
            s->lcp--;
            s->rate_nudge -= 1.0f;
        }
        else if (s->rate_nudge <= -0.5f)
        {
            s->lcp++;
            s->rate_nudge += 1.0f;
        }
        /*endif*/
        if (s->playout_rate < 1.0f)
        {
            /* Speed up - drop a pitch period of signal */
            overlap_add(&s->buf[pitch], s->buf, pitch);
            vec_copyi16(&s->buf[pitch], &s->buf[2*pitch], s->buf_len - 2*pitch);
            if (len - in_len < pitch)
            {
                /* Cannot continue without more samples */
                /* Save the residual signal for next time. */
                vec_copyi16(&s->buf[s->buf_len - pitch], &in[in_len], len - in_len);
                s->fill += (len - in_len - pitch);
                return out_len;
            }
            /*endif*/
            vec_copyi16(&s->buf[s->buf_len - pitch], &in[in_len], pitch);
            in_len += pitch;
        }
        else
        {
            /* Slow down - insert a pitch period of signal */
            vec_copyi16(&out[out_len], s->buf, pitch);
            out_len += pitch;
            overlap_add(s->buf, &s->buf[pitch], pitch);
        }
        /*endif*/
    }
    /*endwhile*/
    return out_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) time_scale_flush(time_scale_state_t *s, int16_t out[])
{
    int len;
    int pad;

    if (s->playout_rate < 1.0f)
        return 0;
    /*endif*/
    vec_copyi16(out, s->buf, s->fill);
    len = s->fill;
    if (s->playout_rate > 1.0f)
    {
        pad = s->fill*(s->playout_rate - 1.0f);
        vec_zeroi16(&out[len], pad);
        len += pad;
    }
    /*endif*/
    s->fill = 0;
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) time_scale_max_output_len(time_scale_state_t *s, int input_len)
{
    return (int) (input_len*s->playout_rate + s->min_pitch + 1);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(time_scale_state_t *) time_scale_init(time_scale_state_t *s, int sample_rate, float playout_rate)
{
    bool alloced;

    if (sample_rate > TIME_SCALE_MAX_SAMPLE_RATE)
        return NULL;
    /*endif*/
    alloced = false;
    if (s == NULL)
    {
        if ((s = (time_scale_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
        /*endif*/
        alloced = true;
    }
    /*endif*/
    s->sample_rate = sample_rate;
    s->min_pitch = sample_rate/TIME_SCALE_MIN_PITCH;
    s->max_pitch = sample_rate/TIME_SCALE_MAX_PITCH;
    s->buf_len = 2*sample_rate/TIME_SCALE_MIN_PITCH;
    if (time_scale_rate(s, playout_rate))
    {
        if (alloced)
            span_free(s);
        /*endif*/
        return NULL;
    }
    /*endif*/
    s->rate_nudge = 0.0f;
    s->fill = 0;
    s->lcp = 0;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) time_scale_release(time_scale_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) time_scale_free(time_scale_state_t *s)
{
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
