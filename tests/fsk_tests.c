/*
 * SpanDSP - a series of DSP components for telephony
 *
 * fsk_tests.c - Tests for the low speed FSK modem code (V.21, V.23, etc.).
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003, 2022 Steve Underwood
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

/*! \page fsk_tests_page FSK modem tests
\section fsk_tests_page_sec_1 What does it do?
These tests allow either:

 - An FSK transmit modem to feed an FSK receive modem, of the same type,
   through a telephone line model. BER testing is then used to evaluate
   performance under various line conditions. This is effective for testing
   the basic performance of the receive modem. It is also the only test mode
   provided for evaluating the transmit modem.

 - An FSK receive modem is used to decode FSK audio, stored in a file.
   This is good way to evaluate performance with audio recorded from other
   models of modem, and with real world problematic telephone lines.

\section fsk_tests_page_sec_2 How does it work?
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sndfile.h>

#include "spandsp.h"
#include "spandsp-sim.h"

#define BLOCK_LEN           160

#define OUTPUT_FILE_NAME    "fsk.wav"

typedef struct
{
    int out_ch;
    int in_ch;
} track_t;

track_t track[2];

char *decode_test_file = NULL;
both_ways_line_model_state_t *model;
int rx_bits = 0;
bool cutoff_test_carrier = false;

static void rx_status(void *user_data, int status)
{
    printf("FSK rx status is %s (%d)\n", signal_status_to_str(status), status);
}
/*- End of function --------------------------------------------------------*/

static void tx_status(void *user_data, int status)
{
    printf("FSK tx status is %s (%d)\n", signal_status_to_str(status), status);
}
/*- End of function --------------------------------------------------------*/

static int framed_get(void *user_data)
{
    track_t *s;
    int x;

    s = (track_t *) user_data;
    x = s->out_ch++;
    return x;
}
/*- End of function --------------------------------------------------------*/

static void framed_put(void *user_data, int ch)
{
    track_t *s;
    int x;

    if (ch < 0)
    {
        rx_status(user_data, ch);
        return;
    }
    /*endif*/

    s = (track_t *) user_data;
    if (s->in_ch%1000 == 0)
        printf("Rx %d\n", s->in_ch);
    /*endif*/
    x = (s->in_ch++) & 0xFF;
    if (x != ch)
        printf("Rx char %d - 0x%x 0x%x\n", rx_bits++, x, ch);
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static void put_bit(void *user_data, int bit)
{
    if (bit < 0)
    {
        rx_status(user_data, bit);
        return;
    }
    /*endif*/

    printf("Rx bit %d - %d\n", rx_bits++, bit);
}
/*- End of function --------------------------------------------------------*/

static void cutoff_test_rx_status(void *user_data, int status)
{
    printf("FSK rx status is %s (%d)\n", signal_status_to_str(status), status);
    switch (status)
    {
    case SIG_STATUS_CARRIER_UP:
        cutoff_test_carrier = true;
        break;
    case SIG_STATUS_CARRIER_DOWN:
        cutoff_test_carrier = false;
        break;
    }
    /*endswitch*/
}
/*- End of function --------------------------------------------------------*/

static void cutoff_test_put_bit(void *user_data, int bit)
{
    if (bit < 0)
    {
        cutoff_test_rx_status(user_data, bit);
        return;
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static void reporter(void *user_data, int reason, bert_results_t *results)
{
    int channel;

    channel = (int) (intptr_t) user_data;
    switch (reason)
    {
    case BERT_REPORT_SYNCED:
        fprintf(stderr, "%d: BERT report synced\n", channel);
        break;
    case BERT_REPORT_UNSYNCED:
        fprintf(stderr, "%d: BERT report unsync'ed\n", channel);
        break;
    case BERT_REPORT_REGULAR:
        fprintf(stderr, "%d: BERT report regular - %d bits, %d bad bits, %d resyncs\n", channel, results->total_bits, results->bad_bits, results->resyncs);
        break;
    case BERT_REPORT_GT_10_2:
        fprintf(stderr, "%d: BERT report > 1 in 10^2\n", channel);
        break;
    case BERT_REPORT_LT_10_2:
        fprintf(stderr, "%d: BERT report < 1 in 10^2\n", channel);
        break;
    case BERT_REPORT_LT_10_3:
        fprintf(stderr, "%d: BERT report < 1 in 10^3\n", channel);
        break;
    case BERT_REPORT_LT_10_4:
        fprintf(stderr, "%d: BERT report < 1 in 10^4\n", channel);
        break;
    case BERT_REPORT_LT_10_5:
        fprintf(stderr, "%d: BERT report < 1 in 10^5\n", channel);
        break;
    case BERT_REPORT_LT_10_6:
        fprintf(stderr, "%d: BERT report < 1 in 10^6\n", channel);
        break;
    case BERT_REPORT_LT_10_7:
        fprintf(stderr, "%d: BERT report < 1 in 10^7\n", channel);
        break;
    default:
        fprintf(stderr, "%d: BERT report reason %d\n", channel, reason);
        break;
    }
    /*endswitch*/
}
/*- End of function --------------------------------------------------------*/

static void decode_file(const char *file, int modem)
{
    SNDFILE *inhandle;
    fsk_rx_state_t *s;
    power_meter_t power_meter;
    int i;
    int samples;
    int16_t amp[BLOCK_LEN];

    printf("Modem is '%s'\n", preset_fsk_specs[modem].name);
    if ((inhandle = sf_open_telephony_read(file, 1)) == NULL)
    {
        fprintf(stderr, "    Cannot open audio file '%s'\n", decode_test_file);
        exit(2);
    }
    /*endif*/
    power_meter_init(&power_meter, 7);
    s = fsk_rx_init(NULL, &preset_fsk_specs[modem], FSK_FRAME_MODE_SYNC, put_bit, NULL);
    fsk_rx_set_modem_status_handler(s, rx_status, (void *) s);

    for (;;)
    {
        samples = sf_readf_short(inhandle, amp, BLOCK_LEN);
        if (samples < BLOCK_LEN)
            break;
        /*endif*/
        for (i = 0;  i < samples;  i++)
            power_meter_update(&power_meter, amp[i]);
        /*endfor*/
        fsk_rx(s, amp, samples);
    }
    /*endfor*/

    if (sf_close_telephony(inhandle))
    {
        fprintf(stderr, "    Cannot close audio file '%s'\n", decode_test_file);
        exit(2);
    }
    /*endif*/
    fsk_rx_free(s);
}
/*- End of function --------------------------------------------------------*/

static void framing_mode_tests(int modem_under_test_1,
                               int modem_under_test_2,
                               int line_model_no,
                               int channel_codec,
                               int rbs_pattern,
                               int noise_sweep,
                               bool log_audio)
{
    fsk_tx_state_t *caller_tx;
    fsk_rx_state_t *caller_rx;
    fsk_tx_state_t *answerer_tx;
    fsk_rx_state_t *answerer_rx;
    async_tx_state_t *caller_tx_async;
    async_tx_state_t *answerer_tx_async;
    power_meter_t caller_meter;
    power_meter_t answerer_meter;
    int16_t caller_amp[BLOCK_LEN];
    int16_t answerer_amp[BLOCK_LEN];
    int16_t caller_model_amp[BLOCK_LEN];
    int16_t answerer_model_amp[BLOCK_LEN];
    int16_t out_amp[2*BLOCK_LEN];
    SNDFILE *outhandle;
    int outframes;
    int i;
    int samples;
    int noise_level;
    int data_bits;
    int parity_mode;
    int stop_bits;

    printf("Test with the framing options\n");

    if (modem_under_test_1 >= 0)
        printf("Modem channel 1 is '%s'\n", preset_fsk_specs[modem_under_test_1].name);
    /*endif*/
    if (modem_under_test_2 >= 0)
        printf("Modem channel 2 is '%s'\n", preset_fsk_specs[modem_under_test_2].name);
    /*endif*/

    outhandle = NULL;

    if (log_audio)
    {
        if ((outhandle = sf_open_telephony_write(OUTPUT_FILE_NAME, 2)) == NULL)
        {
            fprintf(stderr, "    Cannot create audio file '%s'\n", OUTPUT_FILE_NAME);
            exit(2);
        }
        /*endif*/
    }
    /*endif*/
    noise_level = -200;

    printf("Test the framing options\n");
    memset(caller_amp, 0, sizeof(*caller_amp));
    memset(answerer_amp, 0, sizeof(*answerer_amp));
    memset(caller_model_amp, 0, sizeof(*caller_model_amp));
    memset(answerer_model_amp, 0, sizeof(*answerer_model_amp));
    power_meter_init(&caller_meter, 7);
    power_meter_init(&answerer_meter, 7);

    samples = 0;
    data_bits = 8;
    parity_mode = ASYNC_PARITY_MARK;
    stop_bits = 1;
    for (;;)
    {
        if (samples < BLOCK_LEN)
        {
            if (modem_under_test_1 >= 0)
            {
                caller_tx_async = async_tx_init(NULL, data_bits, parity_mode, stop_bits, false, framed_get, &track[0]);
                caller_tx = fsk_tx_init(NULL, &preset_fsk_specs[modem_under_test_1], async_tx_get_bit, caller_tx_async);
                fsk_tx_set_modem_status_handler(caller_tx, tx_status, (void *) caller_tx);
                answerer_rx = fsk_rx_init(NULL, &preset_fsk_specs[modem_under_test_1], FSK_FRAME_MODE_FRAMED, framed_put, &track[1]);
                fsk_rx_set_frame_parameters(answerer_rx, data_bits, parity_mode, stop_bits);
                fsk_rx_set_modem_status_handler(answerer_rx, rx_status, (void *) answerer_rx);
            }
            /*endif*/
            if (modem_under_test_2 >= 0)
            {
                answerer_tx_async = async_tx_init(NULL, data_bits, parity_mode, stop_bits, false, framed_get, &track[1]);
                answerer_tx = fsk_tx_init(NULL, &preset_fsk_specs[modem_under_test_2], async_tx_get_bit, answerer_tx_async);
                fsk_tx_set_modem_status_handler(answerer_tx, tx_status, (void *) answerer_tx);
                caller_rx = fsk_rx_init(NULL, &preset_fsk_specs[modem_under_test_2], FSK_FRAME_MODE_FRAMED, framed_put, &track[0]);
                fsk_rx_set_frame_parameters(answerer_rx, data_bits, parity_mode, stop_bits);
                fsk_rx_set_modem_status_handler(caller_rx, rx_status, (void *) caller_rx);
            }
            /*endif*/
            if ((model = both_ways_line_model_init(line_model_no,
                                                   (float) noise_level,
                                                   -15.0f,
                                                   -15.0f,
                                                   line_model_no,
                                                   (float) noise_level,
                                                   -15.0f,
                                                   -15.0f,
                                                   channel_codec,
                                                   rbs_pattern)) == NULL)
            {
                fprintf(stderr, "    Failed to create line model\n");
                exit(2);
            }
            /*endif*/
        }
        /*endif*/
        samples = fsk_tx(caller_tx, caller_amp, BLOCK_LEN);
        for (i = 0;  i < samples;  i++)
            power_meter_update(&caller_meter, caller_amp[i]);
        /*endfor*/
        samples = fsk_tx(answerer_tx, answerer_amp, BLOCK_LEN);
        for (i = 0;  i < samples;  i++)
            power_meter_update(&answerer_meter, answerer_amp[i]);
        /*endfor*/
        both_ways_line_model(model,
                             caller_model_amp,
                             caller_amp,
                             answerer_model_amp,
                             answerer_amp,
                             samples);

        //printf("Powers %10.5fdBm0 %10.5fdBm0\n", power_meter_current_dbm0(&caller_meter), power_meter_current_dbm0(&answerer_meter));

        fsk_rx(answerer_rx, caller_model_amp, samples);
        for (i = 0;  i < samples;  i++)
            out_amp[2*i] = caller_model_amp[i];
        /*endfor*/
        for (  ;  i < BLOCK_LEN;  i++)
            out_amp[2*i] = 0;
        /*endfor*/

        fsk_rx(caller_rx, answerer_model_amp, samples);
        for (i = 0;  i < samples;  i++)
            out_amp[2*i + 1] = answerer_model_amp[i];
        /*endifor*/
        for (  ;  i < BLOCK_LEN;  i++)
            out_amp[2*i + 1] = 0;
        /*endfor*/

        //printf("Caller errors %d %d\n", fsk_rx_get_parity_errors(caller_rx, false), fsk_rx_get_framing_errors(caller_rx, false));
        //printf("Answerer errors %d %d\n", fsk_rx_get_parity_errors(answerer_rx, false), fsk_rx_get_framing_errors(caller_rx, false));

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

        if (samples < BLOCK_LEN)
        {
            fprintf(stderr, "%ddB AWGN\n", noise_level);

            /* Put a little silence between the chunks in the file. */
            memset(out_amp, 0, sizeof(out_amp));
            if (log_audio)
            {
                for (i = 0;  i < 200;  i++)
                    outframes = sf_writef_short(outhandle, out_amp, BLOCK_LEN);
                /*endfor*/
            }
            /*endif*/
            noise_level++;
            both_ways_line_model_free(model);
        }
        /*endif*/
    }
    /*endfor*/
    if (modem_under_test_1 >= 0)
    {
        fsk_tx_free(caller_tx);
        fsk_rx_free(answerer_rx);
    }
    /*endif*/
    if (modem_under_test_2 >= 0)
    {
        fsk_tx_free(answerer_tx);
        fsk_rx_free(caller_rx);
    }
    /*endif*/
    both_ways_line_model_free(model);
    printf("Tests passed.\n");
    if (log_audio)
    {
        if (sf_close_telephony(outhandle))
        {
            fprintf(stderr, "    Cannot close audio file '%s'\n", OUTPUT_FILE_NAME);
            exit(2);
        }
        /*endif*/
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static void cutoff_level_tests(int modem_under_test_1,
                               int modem_under_test_2)
{
    fsk_rx_state_t *rx;
    int16_t amp[BLOCK_LEN];
    int i;
    int j;
    int samples;
    int on_at;
    int off_at;
    tone_gen_descriptor_t tone_desc;
    tone_gen_state_t tone_tx;

    printf("Test cutoff level\n");

    if (modem_under_test_1 >= 0)
        printf("Modem channel 1 is '%s'\n", preset_fsk_specs[modem_under_test_1].name);
    /*endif*/
    if (modem_under_test_2 >= 0)
        printf("Modem channel 2 is '%s'\n", preset_fsk_specs[modem_under_test_2].name);
    /*endif*/

    memset(amp, 0, sizeof(*amp));

    rx = fsk_rx_init(NULL, &preset_fsk_specs[modem_under_test_1], FSK_FRAME_MODE_SYNC, cutoff_test_put_bit, NULL);
    fsk_rx_set_signal_cutoff(rx, -30.0f);
    fsk_rx_set_modem_status_handler(rx, cutoff_test_rx_status, (void *) rx);
    on_at = 0;
    for (i = -40;  i < -25;  i++)
    {
        tone_gen_descriptor_init(&tone_desc,
                                 1500,
                                 i,
                                 0,
                                 0,
                                 1,
                                 0,
                                 0,
                                 0,
                                 true);
        tone_gen_init(&tone_tx, &tone_desc);
        for (j = 0;  j < 100;  j++)
        {
            samples = tone_gen(&tone_tx, amp, 160);
            fsk_rx(rx, amp, samples);
        }
        /*endfor*/
        if (cutoff_test_carrier)
            break;
        /*endif*/
    }
    /*endfor*/
    on_at = i;
    off_at = 0;
    for (  ;  i > -40;  i--)
    {
        tone_gen_descriptor_init(&tone_desc,
                                 1500,
                                 i,
                                 0,
                                 0,
                                 1,
                                 0,
                                 0,
                                 0,
                                 true);
        tone_gen_init(&tone_tx, &tone_desc);
        for (j = 0;  j < 100;  j++)
        {
            samples = tone_gen(&tone_tx, amp, 160);
            fsk_rx(rx, amp, samples);
        }
        /*endfor*/
        if (!cutoff_test_carrier)
            break;
        /*endif*/
    }
    /*endfor*/
    off_at = i;
    printf("Carrier on at %d, off at %d\n", on_at, off_at);
    if (on_at < -29  ||  on_at > -26
        ||
        off_at < -35  ||  off_at > -31)
    {
        printf("Tests failed.\n");
        exit(2);
    }
    /*endif*/
    fsk_rx_free(rx);
    printf("Tests passed.\n");
}
/*- End of function --------------------------------------------------------*/

static void bert_tests(int modem_under_test_1,
                       int modem_under_test_2,
                       int line_model_no,
                       int channel_codec,
                       int rbs_pattern,
                       int noise_sweep,
                       bool log_audio)
{
    fsk_tx_state_t *caller_tx;
    fsk_rx_state_t *caller_rx;
    fsk_tx_state_t *answerer_tx;
    fsk_rx_state_t *answerer_rx;
    bert_state_t caller_bert;
    bert_state_t answerer_bert;
    bert_results_t bert_results;
    power_meter_t caller_meter;
    power_meter_t answerer_meter;
    int16_t caller_amp[BLOCK_LEN];
    int16_t answerer_amp[BLOCK_LEN];
    int16_t caller_model_amp[BLOCK_LEN];
    int16_t answerer_model_amp[BLOCK_LEN];
    int16_t out_amp[2*BLOCK_LEN];
    SNDFILE *outhandle;
    int outframes;
    int i;
    int samples;
    int test_bps;
    int bits_per_test;
    int noise_level;

    printf("Test with BERT\n");

    if (modem_under_test_1 >= 0)
        printf("Modem channel 1 is '%s'\n", preset_fsk_specs[modem_under_test_1].name);
    /*endif*/
    if (modem_under_test_2 >= 0)
        printf("Modem channel 2 is '%s'\n", preset_fsk_specs[modem_under_test_2].name);
    /*endif*/

    outhandle = NULL;

    if (log_audio)
    {
        if ((outhandle = sf_open_telephony_write(OUTPUT_FILE_NAME, 2)) == NULL)
        {
            fprintf(stderr, "    Cannot create audio file '%s'\n", OUTPUT_FILE_NAME);
            exit(2);
        }
        /*endif*/
    }
    /*endif*/

    memset(caller_amp, 0, sizeof(*caller_amp));
    memset(answerer_amp, 0, sizeof(*answerer_amp));
    memset(caller_model_amp, 0, sizeof(*caller_model_amp));
    memset(answerer_model_amp, 0, sizeof(*answerer_model_amp));
    power_meter_init(&caller_meter, 7);
    power_meter_init(&answerer_meter, 7);

    bits_per_test = 500000;
    noise_level = -24;

    samples = 0;
    for (;;)
    {
        if (samples < BLOCK_LEN)
        {
            if (modem_under_test_1 >= 0)
            {
                caller_tx = fsk_tx_init(NULL, &preset_fsk_specs[modem_under_test_1], (span_get_bit_func_t) bert_get_bit, &caller_bert);
                fsk_tx_set_modem_status_handler(caller_tx, tx_status, (void *) caller_tx);
                answerer_rx = fsk_rx_init(NULL, &preset_fsk_specs[modem_under_test_1], FSK_FRAME_MODE_SYNC, (span_put_bit_func_t) bert_put_bit, &answerer_bert);
                fsk_rx_set_modem_status_handler(answerer_rx, rx_status, (void *) answerer_rx);
            }
            /*endif*/
            if (modem_under_test_2 >= 0)
            {
                answerer_tx = fsk_tx_init(NULL, &preset_fsk_specs[modem_under_test_2], (span_get_bit_func_t) bert_get_bit, &answerer_bert);
                fsk_tx_set_modem_status_handler(answerer_tx, tx_status, (void *) answerer_tx);
                caller_rx = fsk_rx_init(NULL, &preset_fsk_specs[modem_under_test_2], FSK_FRAME_MODE_SYNC, (span_put_bit_func_t) bert_put_bit, &caller_bert);
                fsk_rx_set_modem_status_handler(caller_rx, rx_status, (void *) caller_rx);
            }
            /*endif*/
            test_bps = preset_fsk_specs[modem_under_test_1].baud_rate;
            bert_init(&caller_bert, bits_per_test, BERT_PATTERN_ITU_O152_11, test_bps, 20);
            bert_set_report(&caller_bert, 100000, reporter, (void *) (intptr_t) 1);
            bert_init(&answerer_bert, bits_per_test, BERT_PATTERN_ITU_O152_11, test_bps, 20);
            bert_set_report(&answerer_bert, 100000, reporter, (void *) (intptr_t) 2);

            if ((model = both_ways_line_model_init(line_model_no,
                                                   (float) noise_level,
                                                   -15.0f,
                                                   -15.0f,
                                                   line_model_no,
                                                   (float) noise_level,
                                                   -15.0f,
                                                   -15.0f,
                                                   channel_codec,
                                                   rbs_pattern)) == NULL)
            {
                fprintf(stderr, "    Failed to create line model\n");
                exit(2);
            }
            /*endif*/
        }
        /*endif*/
        samples = fsk_tx(caller_tx, caller_amp, BLOCK_LEN);
        for (i = 0;  i < samples;  i++)
            power_meter_update(&caller_meter, caller_amp[i]);
        /*endfor*/
        samples = fsk_tx(answerer_tx, answerer_amp, BLOCK_LEN);
        for (i = 0;  i < samples;  i++)
            power_meter_update(&answerer_meter, answerer_amp[i]);
        /*endfor*/
        both_ways_line_model(model,
                             caller_model_amp,
                             caller_amp,
                             answerer_model_amp,
                             answerer_amp,
                             samples);

        //printf("Powers %10.5fdBm0 %10.5fdBm0\n", power_meter_current_dbm0(&caller_meter), power_meter_current_dbm0(&answerer_meter));

        fsk_rx(answerer_rx, caller_model_amp, samples);
        for (i = 0;  i < samples;  i++)
            out_amp[2*i] = caller_model_amp[i];
        /*endfor*/
        for (  ;  i < BLOCK_LEN;  i++)
            out_amp[2*i] = 0;
        /*endfor*/

        fsk_rx(caller_rx, answerer_model_amp, samples);
        for (i = 0;  i < samples;  i++)
            out_amp[2*i + 1] = answerer_model_amp[i];
        /*endifor*/
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

        if (samples < BLOCK_LEN)
        {
            bert_result(&caller_bert, &bert_results);
            fprintf(stderr, "%ddB AWGN, %d bits, %d bad bits, %d resyncs\n", noise_level, bert_results.total_bits, bert_results.bad_bits, bert_results.resyncs);
            if (!noise_sweep)
            {
                if (bert_results.total_bits != bits_per_test - 43
                    ||
                    bert_results.bad_bits != 0
                    ||
                    bert_results.resyncs != 0)
                {
                    printf("Tests failed.\n");
                    exit(2);
                }
                /*endif*/
            }
            /*endif*/
            bert_result(&answerer_bert, &bert_results);
            fprintf(stderr, "%ddB AWGN, %d bits, %d bad bits, %d resyncs\n", noise_level, bert_results.total_bits, bert_results.bad_bits, bert_results.resyncs);
            if (!noise_sweep)
            {
                if (bert_results.total_bits != bits_per_test - 43
                    ||
                    bert_results.bad_bits != 0
                    ||
                    bert_results.resyncs != 0)
                {
                    printf("Tests failed.\n");
                    exit(2);
                }
                /*endif*/
                break;
            }
            /*endif*/

            /* Put a little silence between the chunks in the file. */
            memset(out_amp, 0, sizeof(out_amp));
            if (log_audio)
            {
                for (i = 0;  i < 200;  i++)
                    outframes = sf_writef_short(outhandle, out_amp, BLOCK_LEN);
                /*endfor*/
            }
            /*endif*/
            noise_level++;
            both_ways_line_model_free(model);
        }
        /*endif*/
    }
    /*endfor*/
    bert_release(&caller_bert);
    bert_release(&answerer_bert);
    if (modem_under_test_1 >= 0)
    {
        fsk_tx_free(caller_tx);
        fsk_rx_free(answerer_rx);
    }
    /*endif*/
    if (modem_under_test_2 >= 0)
    {
        fsk_tx_free(answerer_tx);
        fsk_rx_free(caller_rx);
    }
    /*endif*/
    both_ways_line_model_free(model);
    printf("Tests passed.\n");
    if (log_audio)
    {
        if (sf_close_telephony(outhandle))
        {
            fprintf(stderr, "    Cannot close audio file '%s'\n", OUTPUT_FILE_NAME);
            exit(2);
        }
        /*endif*/
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int noise_sweep;
    int line_model_no;
    int modem_under_test_1;
    int modem_under_test_2;
    int modems_set;
    int channel_codec;
    int rbs_pattern;
    int opt;
    bool framing_tests;
    bool log_audio;

    framing_tests = false;
    channel_codec = MUNGE_CODEC_NONE;
    rbs_pattern = 0;
    line_model_no = 0;
    decode_test_file = NULL;
    noise_sweep = false;
    modem_under_test_1 = FSK_V21CH1;
    modem_under_test_2 = FSK_V21CH2;
    log_audio = false;
    modems_set = 0;
    while ((opt = getopt(argc, argv, "c:d:flm:nr:s:")) != -1)
    {
        switch (opt)
        {
        case 'c':
            channel_codec = atoi(optarg);
            break;
        case 'd':
            decode_test_file = optarg;
            break;
        case 'f':
            framing_tests = true;
            break;
        case 'l':
            log_audio = true;
            break;
        case 'm':
            line_model_no = atoi(optarg);
            break;
        case 'n':
            noise_sweep = true;
            break;
        case 'r':
            rbs_pattern = atoi(optarg);
            break;
        case 's':
            switch (modems_set++)
            {
            case 0:
                modem_under_test_1 = atoi(optarg);
                break;
            case 1:
                modem_under_test_2 = atoi(optarg);
                break;
            }
            /*endswitch*/
            break;
        default:
            //usage();
            exit(2);
            break;
        }
        /*endswitch*/
    }
    /*endwhile*/

    if (decode_test_file)
    {
        decode_file(decode_test_file, modem_under_test_1);
    }
    else if (framing_tests)
    {
        framing_mode_tests(modem_under_test_1,
                           modem_under_test_2,
                           line_model_no,
                           channel_codec,
                           rbs_pattern,
                           noise_sweep,
                           log_audio);
    }
    else
    {
        cutoff_level_tests(modem_under_test_1,
                           modem_under_test_2);
        bert_tests(modem_under_test_1,
                   modem_under_test_2,
                   line_model_no,
                   channel_codec,
                   rbs_pattern,
                   noise_sweep,
                   log_audio);
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
