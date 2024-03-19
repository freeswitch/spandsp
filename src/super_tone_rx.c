/*
 * SpanDSP - a series of DSP components for telephony
 *
 * super_tone_rx.c - Flexible telephony supervisory tone detection.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003 Steve Underwood
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
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <inttypes.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include "floating_fudge.h"

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/fast_convert.h"
#include "spandsp/complex.h"
#include "spandsp/vector_float.h"
#include "spandsp/complex_vector_float.h"
#include "spandsp/tone_detect.h"
#include "spandsp/tone_generate.h"
#include "spandsp/super_tone_rx.h"

#include "spandsp/private/super_tone_rx.h"

#if defined(SPANDSP_USE_FIXED_POINT)
#if defined(SPANDSP_USE_INTRINSICS_IN_INITIALIZERS)
static const int detection_threshold        = energy_threshold_dbm0(SUPER_TONE_BINS, -42);
static const int tone_twist                 = 4;
static const int tone_to_total_energy       = SUPER_TONE_BINS*64;
#else
static const int detection_threshold        = 16439;            /* -42dBm0 */
static const int tone_twist                 = 4;                /* 6dB */
static const int tone_to_total_energy       = 64;               /* -3dB */
#endif
#else
#if defined(SPANDSP_USE_INTRINSICS_IN_INITIALIZERS)
static const float detection_threshold      = energy_threshold_dbm0(SUPER_TONE_BINS, -42);
static const float tone_twist               = db_to_power_ratio(6.0f);
static const float tone_to_total_energy     = SUPER_TONE_BINS*db_to_power_ratio(-3.0f);
#else
static const float detection_threshold      = 2104205.6f;       /* -42dBm0 */
static const float tone_twist               = 3.981f;           /* 6dB */
static const float tone_to_total_energy     = 1.995f;           /* 3dB */
#endif
#endif

static int add_super_tone_freq(super_tone_rx_descriptor_t *desc, int freq)
{
    int i;

    if (freq == 0)
        return -1;
    /*endif*/
    /* Look for an existing frequency */
    for (i = 0;  i < desc->used_frequencies;  i++)
    {
        if (desc->pitches[i][0] == freq)
            return desc->pitches[i][1];
        /*endif*/
    }
    /*endfor*/
    /* Look for an existing tone which is very close. We may need to merge
       the detectors. */
    for (i = 0;  i < desc->used_frequencies;  i++)
    {
        if ((desc->pitches[i][0] - 10) <= freq  &&  freq <= (desc->pitches[i][0] + 10))
        {
            /* Merge these two */
            desc->pitches[desc->used_frequencies][0] = freq;
            desc->pitches[desc->used_frequencies][1] = i;
            make_goertzel_descriptor(&desc->desc[desc->pitches[i][1]], (float) (freq + desc->pitches[i][0])/2, SUPER_TONE_BINS);
            desc->used_frequencies++;
            return desc->pitches[i][1];
        }
        /*endif*/
    }
    /*endfor*/
    desc->pitches[i][0] = freq;
    desc->pitches[i][1] = desc->monitored_frequencies;
    if (desc->monitored_frequencies%5 == 0)
    {
        desc->desc = (goertzel_descriptor_t *) span_realloc(desc->desc, (desc->monitored_frequencies + 5)*sizeof(goertzel_descriptor_t));
    }
    /*endif*/
    make_goertzel_descriptor(&desc->desc[desc->monitored_frequencies++], (float) freq, SUPER_TONE_BINS);
    desc->used_frequencies++;
    return desc->pitches[i][1];
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) super_tone_rx_add_tone(super_tone_rx_descriptor_t *desc)
{
    if (desc->tones%5 == 0)
    {
        desc->tone_list = (super_tone_rx_segment_t **) span_realloc(desc->tone_list, (desc->tones + 5)*sizeof(super_tone_rx_segment_t *));
        desc->tone_segs = (int *) span_realloc(desc->tone_segs, (desc->tones + 5)*sizeof(int));
    }
    /*endif*/
    desc->tone_list[desc->tones] = NULL;
    desc->tone_segs[desc->tones] = 0;
    desc->tones++;
    return desc->tones - 1;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) super_tone_rx_add_element(super_tone_rx_descriptor_t *desc,
                                            int tone,
                                            int f1,
                                            int f2,
                                            int min,
                                            int max)
{
    int step;

    step = desc->tone_segs[tone];
    if (step%5 == 0)
    {
        desc->tone_list[tone] = (super_tone_rx_segment_t *) span_realloc(desc->tone_list[tone], (step + 5)*sizeof(super_tone_rx_segment_t));
    }
    /*endif*/
    desc->tone_list[tone][step].f1 = add_super_tone_freq(desc, f1);
    desc->tone_list[tone][step].f2 = add_super_tone_freq(desc, f2);
    desc->tone_list[tone][step].min_duration = min*8;
    desc->tone_list[tone][step].max_duration = (max == 0)  ?  0x7FFFFFFF  :  max*8;
    desc->tone_segs[tone]++;
    return step;
}
/*- End of function --------------------------------------------------------*/

static int test_cadence(super_tone_rx_segment_t *pattern,
                        int steps,
                        super_tone_rx_segment_t *test,
                        int rotation)
{
    int i;
    int j;

    if (rotation >= 0)
    {
        /* Check only for the sustaining of a tone in progress. This means
           we only need to check each block if the latest step is compatible
           with the tone template. */
        j = 0;
        if (steps < 0)
        {
            /* A -ve value for steps indicates we just changed step, and need to
               check the last one ended within spec. If we don't do this
               extra test a low duration segment might be accepted as OK. */
            steps = -steps;
            j = (rotation + steps - 2)%steps;
            if (pattern[j].f1 != test[8].f1  ||  pattern[j].f2 != test[8].f2)
                return 0;
            /*endif*/
            if (pattern[j].min_duration > test[8].min_duration*SUPER_TONE_BINS
                ||
                pattern[j].max_duration < test[8].min_duration*SUPER_TONE_BINS)
            {
                return 0;
            }
            /*endif*/
        }
        /*endif*/
        if (steps)
            j = (rotation + steps - 1)%steps;
        /*endif*/
        if (pattern[j].f1 != test[9].f1  ||  pattern[j].f2 != test[9].f2)
            return 0;
        /*endif*/
        if (pattern[j].max_duration < test[9].min_duration*SUPER_TONE_BINS)
            return 0;
        /*endif*/
    }
    else
    {
        /* Look for a complete template match. */
        for (i = 0;  i < steps;  i++)
        {
            j = i + 10 - steps;
            if (pattern[i].f1 != test[j].f1  ||  pattern[i].f2 != test[j].f2)
                return 0;
            /*endif*/
            if (pattern[i].min_duration > test[j].min_duration*SUPER_TONE_BINS
                ||
                pattern[i].max_duration < test[j].min_duration*SUPER_TONE_BINS)
            {
                return 0;
            }
            /*endif*/
        }
        /*endfor*/
    }
    /*endif*/
    return 1;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(super_tone_rx_descriptor_t *) super_tone_rx_make_descriptor(super_tone_rx_descriptor_t *desc)
{
    if (desc == NULL)
    {
        if ((desc = (super_tone_rx_descriptor_t *) span_alloc(sizeof(*desc))) == NULL)
            return NULL;
        /*endif*/
    }
    /*endif*/
    desc->tone_list = NULL;
    desc->tone_segs = NULL;

    desc->used_frequencies = 0;
    desc->monitored_frequencies = 0;
    desc->desc = NULL;
    desc->tones = 0;
    return desc;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) super_tone_rx_free_descriptor(super_tone_rx_descriptor_t *desc)
{
    int i;

    if (desc)
    {
        for (i = 0; i < desc->tones; i++)
        {
            if (desc->tone_list[i])
                span_free(desc->tone_list[i]);
            /*endif*/
        }
        /*endfor*/
        if (desc->tone_list)
            span_free(desc->tone_list);
        /*endif*/
        if (desc->tone_segs)
            span_free(desc->tone_segs);
        /*endif*/
        if (desc->desc)
            span_free(desc->desc);
        /*endif*/
        span_free(desc);
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) super_tone_rx_tone_callback(super_tone_rx_state_t *s,
                                               span_tone_report_func_t callback,
                                               void *user_data)
{
    s->tone_callback = callback;
    s->callback_data = user_data;
}
/*- End of function --------------------------------------------------------*/

static void super_tone_chunk(super_tone_rx_state_t *s)
{
    int i;
    int j;
    int k1;
    int k2;
#if defined(SPANDSP_USE_FIXED_POINT)
    int32_t res[SUPER_TONE_BINS/2];
#else
    float res[SUPER_TONE_BINS/2];
#endif

    if (s->energy < detection_threshold)
    {
        /* The total energy is too low to be considered a tone detection. */
        k1 = -1;
        k2 = -1;
        for (i = 0;  i < s->desc->monitored_frequencies;  i++)
            goertzel_reset(&s->state[i]);
        /*endfor*/
    }
    else
    {
        if (s->desc->monitored_frequencies < 2)
        {
            k1 =
            k2 = 0;
        }
        else
        {
            /* Find our two best monitored frequencies, which also have adequate energy. */
            for (i = 0;  i < s->desc->monitored_frequencies;  i++)
                res[i] = goertzel_result(&s->state[i]);
            /*endfor*/
            if (res[0] > res[1])
            {
                k1 = 0;
                k2 = 1;
            }
            else
            {
                k1 = 1;
                k2 = 0;
            }
            /*endif*/
            for (j = 2;  j < s->desc->monitored_frequencies;  j++)
            {
                if (res[j] >= res[k1])
                {
                    k2 = k1;
                    k1 = j;
                }
                else if (res[j] >= res[k2])
                {
                    k2 = j;
                }
                /*endif*/
            }
            /*endfor*/
            if ((res[k1] + res[k2]) < tone_to_total_energy*s->energy)
            {
                k1 = -1;
                k2 = -1;
            }
            else if (res[k1] > tone_twist*res[k2])
            {
                k2 = -1;
            }
            else if (k2 < k1)
            {
                j = k1;
                k1 = k2;
                k2 = j;
            }
            /*endif*/
        }
        /*endif*/
    }
    /*endif*/
    /* See if this differs from last time. */
    if (k1 != s->segments[10].f1  ||  k2 != s->segments[10].f2)
    {
        /* It is different, but this might just be a transitional quirk, or
           a one shot hiccup (eg due to noise). Only if this same thing is
           seen a second time should we change state. */
        s->segments[10].f1 = k1;
        s->segments[10].f2 = k2;
        /* While things are hopping around, consider this a continuance of the
           previous state. */
        s->segments[9].min_duration++;
    }
    else
    {
        if (k1 != s->segments[9].f1  ||  k2 != s->segments[9].f2)
        {
            if (s->detected_tone >= 0)
            {
                /* Test for the continuance of the existing tone pattern, based on our new knowledge of an
                   entire segment length. */
                if (!test_cadence(s->desc->tone_list[s->detected_tone], -s->desc->tone_segs[s->detected_tone], s->segments, s->rotation++))
                {
                    s->detected_tone = -1;
                    s->tone_callback(s->callback_data, s->detected_tone, -10, 0);
                }
                /*endif*/
            }
            /*endif*/
            if (s->segment_callback)
            {
                s->segment_callback(s->callback_data,
                                    s->segments[9].f1,
                                    s->segments[9].f2,
                                    s->segments[9].min_duration*SUPER_TONE_BINS/8);
            }
            /*endif*/
            memmove(&s->segments[0], &s->segments[1], 9*sizeof(s->segments[0]));
            s->segments[9].f1 = k1;
            s->segments[9].f2 = k2;
            s->segments[9].min_duration = 1;
        }
        else
        {
            /* This is a continuance of the previous state */
            if (s->detected_tone >= 0)
            {
                /* Test for the continuance of the existing tone pattern. We must do this here, so we can sense the
                   discontinuance of the tone on an excessively long segment. */
                if (!test_cadence(s->desc->tone_list[s->detected_tone], s->desc->tone_segs[s->detected_tone], s->segments, s->rotation))
                {
                    s->detected_tone = -1;
                    s->tone_callback(s->callback_data, s->detected_tone, -10, 0);
                }
                /*endif*/
            }
            /*endif*/
            s->segments[9].min_duration++;
        }
        /*endif*/
    }
    /*endif*/
    if (s->detected_tone < 0)
    {
        /* Test for the start of any of the monitored tone patterns */
        for (j = 0;  j < s->desc->tones;  j++)
        {
            if (test_cadence(s->desc->tone_list[j], s->desc->tone_segs[j], s->segments, -1))
            {
                s->detected_tone = j;
                s->rotation = 0;
                s->tone_callback(s->callback_data, s->detected_tone, -10, 0);
                break;
            }
            /*endif*/
        }
        /*endfor*/
    }
    /*endif*/
#if defined(SPANDSP_USE_FIXED_POINT)
    s->energy = 0;
#else
    s->energy = 0.0f;
#endif
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) super_tone_rx(super_tone_rx_state_t *s, const int16_t amp[], int samples)
{
    int i;
    int x;
    int sample;
#if defined(SPANDSP_USE_FIXED_POINT)
    int16_t xamp;
#else
    float xamp;
#endif

    x = 0;
    for (sample = 0;  sample < samples;  sample += x)
    {
        for (i = 0;  i < s->desc->monitored_frequencies;  i++)
            x = goertzel_update(&s->state[i], &amp[sample], samples - sample);
        /*endfor*/
        for (i = 0;  i < x;  i++)
        {
            xamp = goertzel_preadjust_amp(amp[sample + i]);
#if defined(SPANDSP_USE_FIXED_POINT)
            s->energy += ((int32_t) xamp*xamp);
#else
            s->energy += xamp*xamp;
#endif
        }
        /*endfor*/
        if (s->state[0].current_sample >= SUPER_TONE_BINS)
        {
            /* We have finished a Goertzel block. */
            super_tone_chunk(s);
        }
        /*endif*/
    }
    /*endfor*/
    return samples;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) super_tone_rx_fillin(super_tone_rx_state_t *s, int samples)
{
    /* TODO: Roll the detector forward without a state change */
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) super_tone_rx_segment_callback(super_tone_rx_state_t *s,
                                                  tone_segment_func_t callback)
{
    s->segment_callback = callback;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(super_tone_rx_state_t *) super_tone_rx_init(super_tone_rx_state_t *s,
                                                         super_tone_rx_descriptor_t *desc,
                                                         span_tone_report_func_t callback,
                                                         void *user_data)
{
    int i;

    if (desc == NULL)
        return NULL;
    /*endif*/
    if (callback == NULL)
        return NULL;
    /*endif*/
    if (s == NULL)
    {
        if ((s = (super_tone_rx_state_t *) span_alloc(sizeof(*s) + desc->monitored_frequencies*sizeof(goertzel_state_t))) == NULL)
            return NULL;
        /*endif*/
    }
    /*endif*/

    for (i = 0;  i < 11;  i++)
    {
        s->segments[i].f1 = -1;
        s->segments[i].f2 = -1;
        s->segments[i].min_duration = 0;
    }
    /*endfor*/
    s->segment_callback = NULL;
    s->tone_callback = callback;
    s->callback_data = user_data;
    if (desc)
        s->desc = desc;
    /*endif*/
    s->detected_tone = -1;
#if defined(SPANDSP_USE_FIXED_POINT)
    s->energy = 0;
#else
    s->energy = 0.0f;
#endif
    for (i = 0;  i < desc->monitored_frequencies;  i++)
        goertzel_init(&s->state[i], &s->desc->desc[i]);
    /*endfor*/
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) super_tone_rx_release(super_tone_rx_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) super_tone_rx_free(super_tone_rx_state_t *s)
{
    if (s)
        span_free(s);
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
