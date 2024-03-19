/*
 * SpanDSP - a series of DSP components for telephony
 *
 * dtmf.c - DTMF generation and detection.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001-2003, 2005, 2006 Steve Underwood
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
#include <inttypes.h>
#include <memory.h>
#include <string.h>
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
#include "spandsp/stdbool.h"
#endif
#include "floating_fudge.h"

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/logging.h"
#include "spandsp/fast_convert.h"
#include "spandsp/queue.h"
#include "spandsp/complex.h"
#include "spandsp/dds.h"
#include "spandsp/tone_detect.h"
#include "spandsp/tone_generate.h"
#include "spandsp/super_tone_rx.h"
#include "spandsp/dtmf.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/queue.h"
#include "spandsp/private/tone_generate.h"
#include "spandsp/private/dtmf.h"

#define DEFAULT_DTMF_TX_LEVEL                   -10     /* In dBm0 */
#define DEFAULT_DTMF_TX_ON_TIME                 50      /* In ms */
#define DEFAULT_DTMF_TX_OFF_TIME                55      /* In ms */

#define DTMF_SAMPLES_PER_BLOCK                  102

#if defined(SPANDSP_USE_FIXED_POINT)
/* The fixed point version scales the 16 bit signal down by 7 bits, so the Goertzels will fit in a 32 bit word */
#define FP_SCALE(x)                             ((int16_t) (x/128.0 + ((x >= 0.0)  ?  0.5  :  -0.5)))
#if defined(SPANDSP_USE_INTRINSICS_IN_INITIALIZERS)
static const float dtmf_threshold               = goertzel_threshold_dbm0(DTMF_SAMPLES_PER_BLOCK, -42.0f);
static const float dtmf_normal_twist            = db_to_power_ratio(8.0f);
static const float dtmf_reverse_twist           = db_to_power_ratio(4.0f);
static const float dtmf_relative_peak_row       = db_to_power_ratio(8.0f);
static const float dtmf_relative_peak_col       = db_to_power_ratio(8.0f);
static const float dtmf_to_total_energy         = DTMF_SAMPLES_PER_BLOCK*db_to_power_ratio(-0.85f);
static const float dtmf_power_offset            = (power_ratio_to_db(256.0f*256.0f*DTMF_SAMPLES_PER_BLOCK) - DBM0_MAX_SINE_POWER);
#else
static const float dtmf_threshold               = 10438;          /* -42dBm0 */
static const float dtmf_normal_twist            = 6.309f;         /* 8dB */
static const float dtmf_reverse_twist           = 2.512f;         /* 4dB */
static const float dtmf_relative_peak_row       = 6.309f;         /* 8dB */
static const float dtmf_relative_peak_col       = 6.309f;         /* 8dB */
static const float dtmf_to_total_energy         = 83.868f;        /* -0.85dB */
static const float dtmf_power_offset            = 65.251f;
#endif
#else
#define FP_SCALE(x)                             (x)
#if defined(SPANDSP_USE_INTRINSICS_IN_INITIALIZERS)
static const float dtmf_threshold               = goertzel_threshold_dbm0(DTMF_SAMPLES_PER_BLOCK, -42.0f);
static const float dtmf_normal_twist            = db_to_power_ratio(8.0f);
static const float dtmf_reverse_twist           = db_to_power_ratio(4.0f);
static const float dtmf_relative_peak_row       = db_to_power_ratio(8.0f);
static const float dtmf_relative_peak_col       = db_to_power_ratio(8.0f);
static const float dtmf_to_total_energy         = DTMF_SAMPLES_PER_BLOCK*db_to_power_ratio(-0.85f);
static const float dtmf_power_offset            = (power_ratio_to_db(32768.0f*32768.0f*DTMF_SAMPLES_PER_BLOCK) - DBM0_MAX_SINE_POWER);
#else
static const float dtmf_threshold               = 171029200.0f;   /* -42dBm0 */
static const float dtmf_normal_twist            = 6.309f;         /* 8dB */
static const float dtmf_reverse_twist           = 2.512f;         /* 4dB */
static const float dtmf_relative_peak_row       = 6.309f;         /* 8dB */
static const float dtmf_relative_peak_col       = 6.309f;         /* 8dB */
static const float dtmf_to_total_energy         = 83.868f;        /* -0.85dB */
static const float dtmf_power_offset            = 107.255f;
#endif
#endif

static const float dtmf_row[] =
{
     697.0f,  770.0f,  852.0f,  941.0f
};
static const float dtmf_col[] =
{
    1209.0f, 1336.0f, 1477.0f, 1633.0f
};

static const char dtmf_positions[] = "123A" "456B" "789C" "*0#D";

static bool dtmf_rx_inited = false;
static goertzel_descriptor_t dtmf_detect_row[4];
static goertzel_descriptor_t dtmf_detect_col[4];

static bool dtmf_tx_inited = false;
static tone_gen_descriptor_t dtmf_digit_tones[16];

SPAN_DECLARE(int) dtmf_rx(dtmf_rx_state_t *s, const int16_t amp[], int samples)
{
#if defined(SPANDSP_USE_FIXED_POINT)
    int32_t row_energy[4];
    int32_t col_energy[4];
    int16_t xamp;
    float famp;
#else
    float row_energy[4];
    float col_energy[4];
    float xamp;
    float famp;
#endif
    float v1;
    int i;
    int j;
    int sample;
    int best_row;
    int best_col;
    int limit;
    uint8_t hit;

    for (sample = 0;  sample < samples;  sample = limit)
    {
        /* The block length is optimised to meet the DTMF specs. */
        if ((samples - sample) >= (DTMF_SAMPLES_PER_BLOCK - s->current_sample))
            limit = sample + (DTMF_SAMPLES_PER_BLOCK - s->current_sample);
        else
            limit = samples;
        /*endif*/
        /* The following unrolled loop takes only 35% (rough estimate) of the
           time of a rolled loop on the machine on which it was developed */
        for (j = sample;  j < limit;  j++)
        {
            xamp = amp[j];
            if (s->filter_dialtone)
            {
                famp = xamp;
                /* Sharp notches applied at 350Hz and 440Hz - the two common dialtone frequencies.
                   These are rather high Q, to achieve the required narrowness, without using lots of
                   sections. */
                v1 = 0.98356f*famp + 1.8954426f*s->z350[0] - 0.9691396f*s->z350[1];
                famp = v1 - 1.9251480f*s->z350[0] + s->z350[1];
                s->z350[1] = s->z350[0];
                s->z350[0] = v1;

                v1 = 0.98456f*famp + 1.8529543f*s->z440[0] - 0.9691396f*s->z440[1];
                famp = v1 - 1.8819938f*s->z440[0] + s->z440[1];
                s->z440[1] = s->z440[0];
                s->z440[0] = v1;
                xamp = famp;
            }
            /*endif*/
            xamp = goertzel_preadjust_amp(xamp);
#if defined(SPANDSP_USE_FIXED_POINT)
            s->energy += ((int32_t) xamp*xamp);
#else
            s->energy += xamp*xamp;
#endif
            goertzel_samplex(&s->row_out[0], xamp);
            goertzel_samplex(&s->col_out[0], xamp);
            goertzel_samplex(&s->row_out[1], xamp);
            goertzel_samplex(&s->col_out[1], xamp);
            goertzel_samplex(&s->row_out[2], xamp);
            goertzel_samplex(&s->col_out[2], xamp);
            goertzel_samplex(&s->row_out[3], xamp);
            goertzel_samplex(&s->col_out[3], xamp);
        }
        /*endfor*/
        if (s->duration < INT_MAX - (limit - sample))
            s->duration += (limit - sample);
        /*endif*/
        s->current_sample += (limit - sample);
        if (s->current_sample < DTMF_SAMPLES_PER_BLOCK)
            continue;
        /*endif*/

        /* We are at the end of a DTMF detection block */
        /* Find the peak row and the peak column */
        row_energy[0] = goertzel_result(&s->row_out[0]);
        best_row = 0;
        col_energy[0] = goertzel_result(&s->col_out[0]);
        best_col = 0;
        for (i = 1;  i < 4;  i++)
        {
            row_energy[i] = goertzel_result(&s->row_out[i]);
            if (row_energy[i] > row_energy[best_row])
                best_row = i;
            /*endif*/
            col_energy[i] = goertzel_result(&s->col_out[i]);
            if (col_energy[i] > col_energy[best_col])
                best_col = i;
            /*endif*/
        }
        /*endfor*/
        hit = 0;
        /* Basic signal level test and the twist test */
        if (row_energy[best_row] >= s->threshold
            &&
            col_energy[best_col] >= s->threshold)
        {
            if (col_energy[best_col] < row_energy[best_row]*s->reverse_twist
                &&
                col_energy[best_col]*s->normal_twist > row_energy[best_row])
            {
                /* Relative peak test ... */
                for (i = 0;  i < 4;  i++)
                {
                    if ((i != best_col  &&  col_energy[i]*dtmf_relative_peak_col > col_energy[best_col])
                        ||
                        (i != best_row  &&  row_energy[i]*dtmf_relative_peak_row > row_energy[best_row]))
                    {
                        break;
                    }
                    /*endif*/
                }
                /*endfor*/
                /* ... and fraction of total energy test */
                if (i >= 4
                    &&
                    (row_energy[best_row] + col_energy[best_col]) > dtmf_to_total_energy*s->energy)
                {
                    /* Got a hit */
                    hit = dtmf_positions[(best_row << 2) + best_col];
                }
                /*endif*/
            }
            /*endif*/
            if (span_log_test(&s->logging, SPAN_LOG_DEBUG))
            {
                /* Log information about the quality of the signal, to aid analysis of detection problems */
                /* Logging at this point filters the total no-hoper frames out of the log, and leaves
                   anything which might feasibly be a DTMF digit. The log will then contain a list of the
                   total, row and coloumn power levels for detailed analysis of detection problems. */
                span_log(&s->logging,
                         SPAN_LOG_DEBUG,
                         "Potentially '%c' - total %.2fdB, row %.2fdB, col %.2fdB, duration %d - %s\n",
                         dtmf_positions[(best_row << 2) + best_col],
                         power_ratio_to_db(s->energy) - dtmf_power_offset,
                         power_ratio_to_db(row_energy[best_row]/dtmf_to_total_energy) - dtmf_power_offset,
                         power_ratio_to_db(col_energy[best_col]/dtmf_to_total_energy) - dtmf_power_offset,
                         s->duration,
                         (hit)  ?  "hit"  :  "miss");
            }
            /*endif*/
        }
        /*endif*/
        /* The logic in the next test should ensure the following for different successive hit patterns:
                -----ABB = start of digit B.
                ----B-BB = start of digit B
                ----A-BB = start of digit B
                BBBBBABB = still in digit B.
                BBBBBB-- = end of digit B
                BBBBBBC- = end of digit B
                BBBBACBB = B ends, then B starts again.
                BBBBBBCC = B ends, then C starts.
                BBBBBCDD = B ends, then D starts.
           This can work with:
                - Back to back differing digits. Back-to-back digits should
                  not happen. The spec. says there should be a gap between digits.
                  However, many real phones do not impose a gap, and rolling across
                  the keypad can produce little or no gap.
                - It tolerates nasty phones that give a very wobbly start to a digit.
                - VoIP can give sample slips. The phase jumps that produces will cause
                  the block it is in to give no detection. This logic will ride over a
                  single missed block, and not falsely declare a second digit. If the
                  hiccup happens in the wrong place on a minimum length digit, however
                  we would still fail to detect that digit. Could anything be done to
                  deal with that? Packet loss is clearly a no-go zone.
                  Note this is only relevant to VoIP using A-law, u-law or similar.
                  Low bit rate codecs scramble DTMF too much for it to be recognised,
                  and often slip in units larger than a sample. */
        if (hit != s->in_digit  &&  s->last_hit != s->in_digit)
        {
            /* We have two successive indications that something has changed. */
            /* To declare digit on, the hits must agree. Otherwise we declare tone off. */
            hit = (hit  &&  hit == s->last_hit)  ?  hit   :  0;
            if (s->realtime_callback)
            {
                /* Avoid reporting multiple no digit conditions on flaky hits */
                if (s->in_digit  ||  hit)
                {
                    i = (s->in_digit  &&  !hit)  ?  -99  :  lfastrintf(power_ratio_to_db(s->energy) - dtmf_power_offset);
                    s->realtime_callback(s->realtime_callback_data, hit, i, s->duration);
                    s->duration = 0;
                }
                /*endif*/
            }
            else
            {
                if (hit)
                {
                    if (s->current_digits < MAX_DTMF_DIGITS)
                    {
                        s->digits[s->current_digits++] = (char) hit;
                        s->digits[s->current_digits] = '\0';
                        if (s->digits_callback)
                        {
                            s->digits_callback(s->digits_callback_data, s->digits, s->current_digits);
                            s->current_digits = 0;
                        }
                        /*endif*/
                    }
                    else
                    {
                        s->lost_digits++;
                    }
                    /*endif*/
                }
                /*endif*/
            }
            /*endif*/
            s->in_digit = hit;
        }
        /*endif*/
        s->last_hit = hit;
        s->energy = FP_SCALE(0.0f);
        s->current_sample = 0;
    }
    /*endfor*/
    if (s->current_digits  &&  s->digits_callback)
    {
        s->digits_callback(s->digits_callback_data, s->digits, s->current_digits);
        s->digits[0] = '\0';
        s->current_digits = 0;
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) dtmf_rx_fillin(dtmf_rx_state_t *s, int samples)
{
    int i;

    /* Restart any Goertzel and energy gathering operation we might be in the middle of. */
    for (i = 0;  i < 4;  i++)
    {
        goertzel_reset(&s->row_out[i]);
        goertzel_reset(&s->col_out[i]);
    }
    /*endfor*/
    s->energy = FP_SCALE(0.0f);
    s->current_sample = 0;
    /* Don't update the hit detection. Pretend it never happened. */
    /* TODO: Surely we can be cleverer than this. */
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) dtmf_rx_status(dtmf_rx_state_t *s)
{
    if (s->in_digit)
        return s->in_digit;
    /*endif*/
    if (s->last_hit)
        return 'x';
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) dtmf_rx_get(dtmf_rx_state_t *s, char *buf, int max)
{
    if (max > s->current_digits)
        max = s->current_digits;
    /*endif*/
    if (max > 0)
    {
        memcpy(buf, s->digits, max);
        memmove(s->digits, s->digits + max, s->current_digits - max);
        s->current_digits -= max;
    }
    /*endif*/
    buf[max] = '\0';
    return max;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) dtmf_rx_set_realtime_callback(dtmf_rx_state_t *s,
                                                 span_tone_report_func_t callback,
                                                 void *user_data)
{
    s->realtime_callback = callback;
    s->realtime_callback_data = user_data;
    s->duration = 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) dtmf_rx_parms(dtmf_rx_state_t *s,
                                 int filter_dialtone,
                                 float twist,
                                 float reverse_twist,
                                 float threshold)
{
    if (filter_dialtone >= 0)
    {
        s->z350[0] = 0.0f;
        s->z350[1] = 0.0f;
        s->z440[0] = 0.0f;
        s->z440[1] = 0.0f;
        s->filter_dialtone = filter_dialtone;
    }
    /*endif*/
    if (twist >= 0.0f)
        s->normal_twist = db_to_power_ratio(twist);
    /*endif*/
    if (reverse_twist >= 0.0f)
        s->reverse_twist = db_to_power_ratio(reverse_twist);
    /*endif*/
    if (threshold > -99.0f)
        s->threshold = goertzel_threshold_dbm0(DTMF_SAMPLES_PER_BLOCK, threshold);
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) dtmf_rx_get_logging_state(dtmf_rx_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(dtmf_rx_state_t *) dtmf_rx_init(dtmf_rx_state_t *s,
                                             digits_rx_callback_t callback,
                                             void *user_data)
{
    int i;

    if (s == NULL)
    {
        if ((s = (dtmf_rx_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
        /*endif*/
    }
    /*endif*/
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "DTMF");
    s->digits_callback = callback;
    s->digits_callback_data = user_data;
    s->realtime_callback = NULL;
    s->realtime_callback_data = NULL;
    s->filter_dialtone = false;
    s->normal_twist = dtmf_normal_twist;
    s->reverse_twist = dtmf_reverse_twist;
    s->threshold = dtmf_threshold;

    s->in_digit = 0;
    s->last_hit = 0;

    if (!dtmf_rx_inited)
    {
        for (i = 0;  i < 4;  i++)
        {
            make_goertzel_descriptor(&dtmf_detect_row[i], dtmf_row[i], DTMF_SAMPLES_PER_BLOCK);
            make_goertzel_descriptor(&dtmf_detect_col[i], dtmf_col[i], DTMF_SAMPLES_PER_BLOCK);
        }
        /*endfor*/
        dtmf_rx_inited = true;
    }
    /*endif*/
    for (i = 0;  i < 4;  i++)
    {
        goertzel_init(&s->row_out[i], &dtmf_detect_row[i]);
        goertzel_init(&s->col_out[i], &dtmf_detect_col[i]);
    }
    /*endfor*/
    s->energy = FP_SCALE(0.0f);
    s->current_sample = 0;
    s->lost_digits = 0;
    s->current_digits = 0;
    s->digits[0] = '\0';
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) dtmf_rx_release(dtmf_rx_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) dtmf_rx_free(dtmf_rx_state_t *s)
{
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void dtmf_tx_initialise(void)
{
    int row;
    int col;

    if (dtmf_tx_inited)
        return;
    /*endif*/
    for (row = 0;  row < 4;  row++)
    {
        for (col = 0;  col < 4;  col++)
        {
            tone_gen_descriptor_init(&dtmf_digit_tones[row*4 + col],
                                     (int) dtmf_row[row],
                                     DEFAULT_DTMF_TX_LEVEL,
                                     (int) dtmf_col[col],
                                     DEFAULT_DTMF_TX_LEVEL,
                                     DEFAULT_DTMF_TX_ON_TIME,
                                     DEFAULT_DTMF_TX_OFF_TIME,
                                     0,
                                     0,
                                     false);
        }
        /*endfor*/
    }
    /*endfor*/
    dtmf_tx_inited = true;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) dtmf_tx(dtmf_tx_state_t *s, int16_t amp[], int max_samples)
{
    int len;
    const char *cp;
    int digit;

    len = 0;
    if (s->tones.current_section >= 0)
    {
        /* Deal with the fragment left over from last time */
        len = tone_gen(&s->tones, amp, max_samples);
    }
    /*endif*/

    while (len < max_samples)
    {
        /* Step to the next digit */
        if ((digit = queue_read_byte(&s->queue.queue)) < 0)
        {
            /* See if we can get some more digits */
            if (s->callback == NULL)
                break;
            /*endif*/
            s->callback(s->callback_data);
            if ((digit = queue_read_byte(&s->queue.queue)) < 0)
                break;
            /*endif*/
        }
        /*endif*/
        if (digit == 0)
            continue;
        /*endif*/
        if ((cp = strchr(dtmf_positions, digit)) == NULL)
            continue;
        /*endif*/
        tone_gen_init(&s->tones, &dtmf_digit_tones[cp - dtmf_positions]);
        s->tones.tone[0].gain = s->low_level;
        s->tones.tone[1].gain = s->high_level;
        s->tones.duration[0] = s->on_time;
        s->tones.duration[1] = s->off_time;
        len += tone_gen(&s->tones, &amp[len], max_samples - len);
    }
    /*endwhile*/
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) dtmf_tx_put(dtmf_tx_state_t *s, const char *digits, int len)
{
    size_t space;

    /* This returns the number of characters that would not fit in the buffer.
       The buffer will only be loaded if the whole string of digits will fit,
       in which case zero is returned. */
    if (len < 0)
    {
        if ((len = strlen(digits)) == 0)
            return 0;
        /*endif*/
    }
    /*endif*/
    if ((space = queue_free_space(&s->queue.queue)) < (size_t) len)
        return len - (int) space;
    /*endif*/
    if (queue_write(&s->queue.queue, (const uint8_t *) digits, len) >= 0)
        return 0;
    /*endif*/
    return -1;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) dtmf_tx_set_level(dtmf_tx_state_t *s, int level, int twist)
{
    s->low_level = dds_scaling_dbm0f((float) level);
    s->high_level = dds_scaling_dbm0f((float) (level + twist));
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) dtmf_tx_set_timing(dtmf_tx_state_t *s, int on_time, int off_time)
{
    s->on_time = ((on_time >= 0)  ?  on_time  :  DEFAULT_DTMF_TX_ON_TIME)*SAMPLE_RATE/1000;
    s->off_time = ((off_time >= 0)  ?  off_time  :  DEFAULT_DTMF_TX_OFF_TIME)*SAMPLE_RATE/1000;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(dtmf_tx_state_t *) dtmf_tx_init(dtmf_tx_state_t *s,
                                             digits_tx_callback_t callback,
                                             void *user_data)
{
    if (s == NULL)
    {
        if ((s = (dtmf_tx_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
        /*endif*/
    }
    /*endif*/
    memset(s, 0, sizeof(*s));
    if (!dtmf_tx_inited)
        dtmf_tx_initialise();
    /*endif*/
    s->callback = callback;
    s->callback_data = user_data;
    tone_gen_init(&s->tones, &dtmf_digit_tones[0]);
    dtmf_tx_set_level(s, DEFAULT_DTMF_TX_LEVEL, 0);
    dtmf_tx_set_timing(s, -1, -1);
    queue_init(&s->queue.queue, MAX_DTMF_DIGITS, QUEUE_READ_ATOMIC | QUEUE_WRITE_ATOMIC);
    s->tones.current_section = -1;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) dtmf_tx_release(dtmf_tx_state_t *s)
{
    queue_release(&s->queue.queue);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) dtmf_tx_free(dtmf_tx_state_t *s)
{
    dtmf_tx_release(s);
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
