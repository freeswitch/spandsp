/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v32bis_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* V.32bis SUPPORT IS A WORK IN PROGRESS - NOT YET FUNCTIONAL! */

/*! \page v32bis_tests_page V.32bis modem tests
\section v32bis_tests_page_sec_1 What does it do?
These tests connect two V.32bis modems back to back, through a telephone line
model. BER testing is then used to evaluate performance under various line
conditions.

If the appropriate GUI environment exists, the tests are built such that a visual
display of modem status is maintained.

\section v32bis_tests_page_sec_2 How is it used?
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if defined(HAVE_FL_FL_H)  &&  defined(HAVE_FL_FL_CARTESIAN_H)  &&  defined(HAVE_FL_FL_AUDIO_METER_H)
#define ENABLE_GUI
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sndfile.h>

#include "spandsp.h"
#include "spandsp-sim.h"

#if defined(ENABLE_GUI)
#include "modem_monitor.h"
#include "line_model_monitor.h"
#endif

#define BLOCK_LEN       160

#define IN_FILE_NAME    "v32bis_samp.wav"
#define OUT_FILE_NAME   "v32bis.wav"

char *decode_test_file = NULL;
bool use_gui = false;

int rx_bits = 0;
int tx_bits = 0;

bert_state_t caller_bert;
bert_state_t answerer_bert;
both_ways_line_model_state_t *model;

v32bis_state_t caller;
v32bis_state_t answerer;

struct qam_report_control_s
{
    v32bis_state_t *s;
#if defined(ENABLE_GUI)
    qam_monitor_t *qam_monitor;
#endif
    float smooth_power;
    int symbol_no;
};

struct qam_report_control_s qam_caller;
struct qam_report_control_s qam_answerer;

bert_results_t latest_results;

static void reporter(void *user_data, int reason, bert_results_t *results)
{
    switch (reason)
    {
    case BERT_REPORT_REGULAR:
        fprintf(stderr, "%p: BERT report regular - %d bits, %d bad bits, %d resyncs\n", user_data, results->total_bits, results->bad_bits, results->resyncs);
        memcpy(&latest_results, results, sizeof(latest_results));
        break;
    default:
        fprintf(stderr, "%p - BERT report %s\n", user_data, bert_event_to_str(reason));
        break;
    }
    /*endswitch*/
}
/*- End of function --------------------------------------------------------*/

static void v32bis_rx_status(void *user_data, int status)
{
    v32bis_state_t *s;
    int i;
    int len;
#if defined(SPANDSP_USE_FIXED_POINT)
    complexi16_t *coeffs;
#else
    complexf_t *coeffs;
#endif

    printf("%p: V.32bis rx status is %s (%d)\n", user_data, signal_status_to_str(status), status);
    s = (v32bis_state_t *) user_data;
    switch (status)
    {
    case SIG_STATUS_TRAINING_SUCCEEDED:
        len = v32bis_equalizer_state(s, &coeffs);
        printf("Equalizer:\n");
        for (i = 0;  i < len;  i++)
#if defined(SPANDSP_USE_FIXED_POINT)
            printf("%p: %3d (%15.5f, %15.5f)\n", user_data, i, coeffs[i].re/V32BIS_CONSTELLATION_SCALING_FACTOR, coeffs[i].im/V32BIS_CONSTELLATION_SCALING_FACTOR);
#else
            printf("%p: %3d (%15.5f, %15.5f) -> %15.5f\n", user_data, i, coeffs[i].re, coeffs[i].im, powerf(&coeffs[i]));
#endif
        /*endfor*/
        break;
    }
    /*endswitch*/
}
/*- End of function --------------------------------------------------------*/

static void v32bis_putbit(void *user_data, int bit)
{
    if (bit < 0)
    {
        v32bis_rx_status(user_data, bit);
        return;
    }
    /*endif*/

    if (decode_test_file)
    {
        printf("%p: Rx bit %d - %d\n", user_data, rx_bits++, bit);
    }
    else
    {
        if (user_data == (void *) &caller)
            bert_put_bit(&caller_bert, bit);
        else
            bert_put_bit(&answerer_bert, bit);
        /*endif*/
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static int v32bis_getbit(void *user_data)
{
    if (user_data == (void *) &caller)
        return bert_get_bit(&caller_bert);
    /*endif*/
    return bert_get_bit(&answerer_bert);
}
/*- End of function --------------------------------------------------------*/

#if defined(SPANDSP_USE_FIXED_POINT)
static void qam_report(void *user_data, const complexi16_t *constel, const complexi16_t *target, int symbol)
#else
static void qam_report(void *user_data, const complexf_t *constel, const complexf_t *target, int symbol)
#endif
{
    int i;
    int len;
#if defined(SPANDSP_USE_FIXED_POINT)
    complexi16_t *coeffs;
#else
    complexf_t *coeffs;
#endif
    complexf_t constel_point;
    complexf_t target_point;
    float fpower;
    struct qam_report_control_s *s;

    s = (struct qam_report_control_s *) user_data;
    if (constel)
    {
        constel_point.re = constel->re/V32BIS_CONSTELLATION_SCALING_FACTOR;
        constel_point.im = constel->im/V32BIS_CONSTELLATION_SCALING_FACTOR;
        target_point.re = target->re/V32BIS_CONSTELLATION_SCALING_FACTOR;
        target_point.im = target->im/V32BIS_CONSTELLATION_SCALING_FACTOR;
        fpower = (constel_point.re - target_point.re)*(constel_point.re - target_point.re)
               + (constel_point.im - target_point.im)*(constel_point.im - target_point.im);
        s->smooth_power = 0.95f*s->smooth_power + 0.05f*fpower;
#if defined(ENABLE_GUI)
        if (use_gui)
        {
            qam_monitor_update_constel(s->qam_monitor, &constel_point);
            qam_monitor_update_carrier_tracking(s->qam_monitor, v32bis_rx_carrier_frequency(s->s));
            qam_monitor_update_symbol_tracking(s->qam_monitor, v32bis_rx_symbol_timing_correction(s->s));
        }
        /*endif*/
#endif
        printf("%8d [%8.4f, %8.4f] [%8.4f, %8.4f] %2x %8.4f %8.4f %9.4f %7.3f %7.4f\n",
               s->symbol_no,
               constel_point.re,
               constel_point.im,
               target_point.re,
               target_point.im,
               symbol,
               fpower,
               s->smooth_power,
               v32bis_rx_carrier_frequency(s->s),
               v32bis_rx_signal_power(s->s),
               v32bis_rx_symbol_timing_correction(s->s));
        s->symbol_no++;
    }
    else
    {
        printf("Gardner step %d\n", symbol);
        if ((len = v32bis_equalizer_state(s->s, &coeffs)))
        {
            printf("Equalizer A:\n");
            for (i = 0;  i < len;  i++)
#if defined(SPANDSP_USE_FIXED_POINT)
                printf("%3d (%15.5f, %15.5f)\n", i, coeffs[i].re/V32BIS_CONSTELLATION_SCALING_FACTOR, coeffs[i].im/V32BIS_CONSTELLATION_SCALING_FACTOR);
#else
                printf("%3d (%15.5f, %15.5f) -> %15.5f\n", i, coeffs[i].re, coeffs[i].im, powerf(&coeffs[i]));
#endif
            /*endfor*/
#if defined(ENABLE_GUI)
            if (use_gui)
            {
#if defined(SPANDSP_USE_FIXED_POINT)
                qam_monitor_update_int_equalizer(s->qam_monitor, coeffs, len);
#else
                qam_monitor_update_equalizer(s->qam_monitor, coeffs, len);
#endif
            }
            /*endif*/
#endif
        }
    }
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int16_t caller_amp[BLOCK_LEN];
    int16_t answerer_amp[BLOCK_LEN];
    int16_t caller_model_amp[BLOCK_LEN];
    int16_t answerer_model_amp[BLOCK_LEN];
    int16_t out_amp[2*BLOCK_LEN];
    bert_results_t bert_results;
    SNDFILE *outhandle;
    int outframes;
    int samples;
    int i;
    int test_bps;
    int line_model_no;
    int bits_per_test;
    int noise_level;
    int signal_level;
    int channel_codec;
    int opt;
    bool log_audio;
    float echo_level;
    logging_state_t *logging;

    channel_codec = MUNGE_CODEC_NONE;
    test_bps = 14400;
    line_model_no = 0;
    noise_level = -70;
    signal_level = -13;
    echo_level = -99.0f;
    bits_per_test = 50000;
    log_audio = false;
    while ((opt = getopt(argc, argv, "b:B:c:e:glm:n:s:")) != -1)
    {
        switch (opt)
        {
        case 'b':
            test_bps = atoi(optarg);
            if (test_bps != 14400
                &&
                test_bps != 12000
                &&
                test_bps != 9600
                &&
                test_bps != 7200
                &&
                test_bps != 4800)
            {
                fprintf(stderr, "Invalid bit rate specified\n");
                exit(2);
            }
            /*endif*/
            break;
        case 'B':
            bits_per_test = atoi(optarg);
            break;
        case 'c':
            channel_codec = atoi(optarg);
            break;
        case 'e':
            echo_level = atoi(optarg);
            break;
        case 'g':
#if defined(ENABLE_GUI)
            use_gui = true;
#else
            fprintf(stderr, "Graphical monitoring not available\n");
            exit(2);
#endif
            break;
        case 'l':
            log_audio = true;
            break;
        case 'm':
            line_model_no = atoi(optarg);
            break;
        case 'n':
            noise_level = atoi(optarg);
            break;
        case 's':
            signal_level = atoi(optarg);
            break;
        default:
            //usage();
            exit(2);
            break;
        }
        /*endswitch*/
    }
    /*endwhile*/
    outhandle = NULL;

    if (log_audio)
    {
        if ((outhandle = sf_open_telephony_write(OUT_FILE_NAME, 2)) == NULL)
        {
            fprintf(stderr, "    Cannot create audio file '%s'\n", OUT_FILE_NAME);
            exit(2);
        }
        /*endif*/
    }
    /*endif*/

    v32bis_init(&caller, test_bps, true, v32bis_getbit, &caller, v32bis_putbit, &caller);
    v32bis_tx_power(&caller, signal_level);
    /* Move the carrier off a bit */
    caller.tx.carrier_phase_rate = dds_phase_ratef(1807.0f);
    v32bis_set_qam_report_handler(&caller, qam_report, (void *) &qam_caller);
    logging = v32bis_get_logging_state(&caller);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "caller");

    v32bis_init(&answerer, test_bps, false, v32bis_getbit, &answerer, v32bis_putbit, &answerer);
    v32bis_tx_power(&answerer, signal_level);
    /* Move the carrier off a bit */
    answerer.tx.carrier_phase_rate = dds_phase_ratef(1793.0f);
    v32bis_set_qam_report_handler(&answerer, qam_report, (void *) &qam_answerer);
    logging = v32bis_get_logging_state(&answerer);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "answerer");

    qam_caller.s = &caller;
    qam_caller.smooth_power = 0.0f;
    qam_caller.symbol_no = 0;

    qam_answerer.s = &answerer;
    qam_answerer.smooth_power = 0.0f;
    qam_answerer.symbol_no = 0;

#if defined(ENABLE_GUI)
    if (use_gui)
    {
        qam_caller.qam_monitor = qam_monitor_init(10.0f, V32BIS_CONSTELLATION_SCALING_FACTOR, "Calling modem");
        qam_answerer.qam_monitor = qam_monitor_init(10.0f, V32BIS_CONSTELLATION_SCALING_FACTOR, "Answering modem");
    }
    /*endif*/
#endif
    bert_init(&caller_bert, bits_per_test, BERT_PATTERN_ITU_O152_11, test_bps, 20);
    bert_set_report(&caller_bert, 10000, reporter, &caller);
    bert_init(&answerer_bert, bits_per_test, BERT_PATTERN_ITU_O152_11, test_bps, 20);
    bert_set_report(&answerer_bert, 10000, reporter, &answerer);
    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           echo_level,
                                           echo_level,
                                           line_model_no,
                                           (float) noise_level,
                                           echo_level,
                                           echo_level,
                                           channel_codec,
                                           0)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }
    /*endif*/
    for (;;)
    {
        samples = v32bis_tx(&caller, caller_amp, BLOCK_LEN);
#if defined(ENABLE_GUI)
        if (use_gui)
            qam_monitor_update_audio_level(qam_caller.qam_monitor, caller_amp, samples);
#endif
        if (samples == 0)
        {
            printf("Restarting on zero output\n");
            v32bis_restart(&caller, test_bps);
            bert_init(&caller_bert, bits_per_test, BERT_PATTERN_ITU_O152_11, test_bps, 20);
            bert_set_report(&caller_bert, 10000, reporter, &caller);
            bert_init(&answerer_bert, bits_per_test, BERT_PATTERN_ITU_O152_11, test_bps, 20);
            bert_set_report(&answerer_bert, 10000, reporter, &answerer);
        }
        /*endif*/

        samples = v32bis_tx(&answerer, answerer_amp, BLOCK_LEN);
#if defined(ENABLE_GUI)
        if (use_gui)
            qam_monitor_update_audio_level(qam_answerer.qam_monitor, answerer_amp, samples);
        /*endif*/
#endif
        if (samples == 0)
        {
            printf("Restarting on zero output\n");
            v32bis_restart(&answerer, test_bps);
            bert_init(&caller_bert, bits_per_test, BERT_PATTERN_ITU_O152_11, test_bps, 20);
            bert_set_report(&caller_bert, 10000, reporter, &caller);
            bert_init(&answerer_bert, bits_per_test, BERT_PATTERN_ITU_O152_11, test_bps, 20);
            bert_set_report(&answerer_bert, 10000, reporter, &answerer);
        }
        /*endif*/
        both_ways_line_model(model,
                             caller_model_amp,
                             caller_amp,
                             answerer_model_amp,
                             answerer_amp,
                             samples);
        v32bis_rx(&answerer, caller_model_amp, samples);
        for (i = 0;  i < samples;  i++)
            out_amp[2*i] = caller_model_amp[i];
        /*endfor*/
        for (  ;  i < BLOCK_LEN;  i++)
            out_amp[2*i] = 0;
        /*endfor*/

        v32bis_rx(&caller, answerer_model_amp, samples);
        for (i = 0;  i < samples;  i++)
            out_amp[2*i + 1] = answerer_model_amp[i];
        /*endfor*/
        for (  ;  i < BLOCK_LEN;  i++)
            out_amp[2*i + 1] = 0;
        /*endfor*/

        if (log_audio)
        {
            outframes = sf_writef_short(outhandle, out_amp, BLOCK_LEN);
            if (outframes != BLOCK_LEN)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
            /*endif*/
        }
        /*endif*/
    }
    /*endfor*/
    if (!decode_test_file)
    {
        bert_result(&answerer_bert, &bert_results);
        fprintf(stderr, "At completion:\n");
        fprintf(stderr, "Final result %ddBm0/%ddBm0, %d bits, %d bad bits, %d resyncs\n", signal_level, noise_level, bert_results.total_bits, bert_results.bad_bits, bert_results.resyncs);
        fprintf(stderr, "Last report  %ddBm0/%ddBm0, %d bits, %d bad bits, %d resyncs\n", signal_level, noise_level, latest_results.total_bits, latest_results.bad_bits, latest_results.resyncs);
        both_ways_line_model_free(model);
        bert_release(&answerer_bert);

        if (signal_level > -43)
        {
            printf("Tests failed.\n");
            exit(2);
        }
        /*endif*/

        printf("Tests passed.\n");
    }
    /*endif*/
    v32bis_free(&caller);
    v32bis_free(&answerer);
    if (log_audio)
    {
        if (sf_close_telephony(outhandle))
        {
            fprintf(stderr, "    Cannot close audio file '%s'\n", OUT_FILE_NAME);
            exit(2);
        }
        /*endif*/
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
