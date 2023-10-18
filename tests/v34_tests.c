/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v34_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2009 Steve Underwood
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

/*! \page v34_tests_page V.34 modem tests
\section v34_tests_page_sec_1 What does it do?
These tests connect two V.34 modems back to back, through a telephone line
model. BER testing is then used to evaluate performance under various line
conditions.

If the appropriate GUI environment exists, the tests are built such that a visual
display of modem status is maintained.

\section v34_tests_page_sec_2 How is it used?
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

#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
#include "spandsp.h"
#include "spandsp-sim.h"

#if defined(ENABLE_GUI)
#include "modem_monitor.h"
#endif

#define SAMPLES_PER_CHUNK       160

#define OUT_FILE_NAME   "v34_tests.wav"

int in_bit = 0;
int out_bit = 0;

int in_bit_no = 0;
int out_bit_no = 0;

uint8_t tx_buf[1000];
int rx_ptr = 0;
int tx_ptr = 0;

int rx_bits = 0;
int rx_bad_bits = 0;

char *decode_test_file = NULL;
int use_gui = false;

static const int valid_bit_rates[] =
{
     2400,
     2600,
     4800,
     5000,
     7200,
     7400,
     9600,
     9800,
    12000,
    12200,
    14400,
    14600,
    16800,
    17000,
    19200,
    19400,
    21600,
    21800,
    24000,
    24200,
    26400,
    26600,
    28800,
    29000,
    31200,
    31400,
    33600,
    33800,
    -1
};

both_ways_line_model_state_t *model;

v8_state_t v8_caller;
v8_state_t v8_answerer;
v34_state_t v34_caller;
v34_state_t v34_answerer;

int test_bps;
int test_baud_rate;

struct qam_report_control_s
{
    v34_state_t *s;
#if defined(ENABLE_GUI)
    qam_monitor_t *qam_monitor;
#endif
    float smooth_power;
    int symbol_no;
};

struct qam_report_control_s qam_caller;
struct qam_report_control_s qam_answerer;

static void v8_handler(void *user_data, v8_parms_t *result)
{
    v34_state_t *s;

    s = (v34_state_t *) user_data;

    switch (result->status)
    {
    case V8_STATUS_IN_PROGRESS:
        printf("V.8 negotiation in progress\n");
        return;
        break;
    case V8_STATUS_V8_OFFERED:
        printf("V.8 offered by the other party\n");
        break;
    case V8_STATUS_V8_CALL:
        printf("V.8 call negotiation successful\n");
        break;
    case V8_STATUS_NON_V8_CALL:
        printf("Non-V.8 call negotiation successful\n");
        printf("  Modem connect tone '%s' (%d)\n", modem_connect_tone_to_str(result->modem_connect_tone), result->modem_connect_tone);
        return;
    case V8_STATUS_FAILED:
        printf("V.8 call negotiation failed\n");
        return;
    default:
        printf("Unexpected V.8 status %d\n", result->status);
        break;
    }
    /*endswitch*/

    printf("  Modem connect tone '%s' (%d)\n", modem_connect_tone_to_str(result->modem_connect_tone), result->modem_connect_tone);
    printf("  Call function '%s' (%d)\n", v8_call_function_to_str(result->call_function), result->call_function);
    printf("  Far end modulations 0x%X\n", result->modulations);
    printf("  Protocol '%s' (%d)\n", v8_protocol_to_str(result->protocol), result->protocol);
    printf("  PSTN access '%s' (%d)\n", v8_pstn_access_to_str(result->pstn_access), result->pstn_access);
    printf("  PCM modem availability '%s' (%d)\n", v8_pcm_modem_availability_to_str(result->pcm_modem_availability), result->pcm_modem_availability);
    if (result->t66 >= 0)
        printf("  T.66 '%s' (%d)\n", v8_t66_to_str(result->t66), result->t66);
    /*endif*/
    if (result->nsf >= 0)
        printf("  NSF %d\n", result->nsf);
    /*endif*/

    switch (result->status)
    {
    case V8_STATUS_V8_OFFERED:
        /* Edit the result information appropriately */
        result->modulations &= (V8_MOD_V21
                              | V8_MOD_V27TER
                              | V8_MOD_V29
                              | V8_MOD_V17
                              | V8_MOD_V34HDX);
        break;
    case V8_STATUS_V8_CALL:
        switch (result->call_function)
        {
        case V8_CALL_T30_TX:
            v34_restart(s, test_baud_rate, test_bps, false);
            v34_half_duplex_change_mode(s, V34_HALF_DUPLEX_SOURCE);
            break;
        case V8_CALL_T30_RX:
            v34_restart(s, test_baud_rate, test_bps, false);
            v34_half_duplex_change_mode(s, V34_HALF_DUPLEX_RECIPIENT);
            break;
        case V8_CALL_V_SERIES:
            v34_restart(s, test_baud_rate, test_bps, true);
            if (result->protocol == V8_PROTOCOL_NONE)
            {
                //negotiations_ok++;
            }
            /*endif*/
            break;
        }
        /*endswitch*/
        break;
    }
    /*endswitch*/
}
/*- End of function --------------------------------------------------------*/

static void hdlc_handler(void *user_data, const uint8_t *buf, int len, int ok)
{
    int i;

    printf("OK %d, len %d\n", ok, len);
    if (len > 0)
    {
        printf("OK >> ");
        for (i = 0;  i < len;  i++)
            printf("%02X ", buf[i]);
        /*endfor*/
        printf("\n");
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static void v34_decode_putbit(void *user_data, int bit)
{
    static hdlc_rx_state_t hdlc_rx;
    static int first = 1;

    if (bit < 0)
    {
        /* Special conditions */
        printf("V.34 rx status is %s (%d)\n", signal_status_to_str(bit), bit);
        return;
    }
    /*endif*/

    if (first)
    {
        first = 0;
        hdlc_rx_init(&hdlc_rx, false, true, 2, hdlc_handler, NULL);
    }
    /*endif*/
    hdlc_rx_put_bit(&hdlc_rx, bit);
}
/*- End of function --------------------------------------------------------*/

static int v34_get_aux_bit(void *user_data)
{
    int bit;

    //bit = rand() & 1;
    bit = 1;
    return bit;
}
/*- End of function --------------------------------------------------------*/

static void v34_put_aux_bit(void *user_data, int bit)
{
    printf("Rx aux bit %d\n", bit);
}
/*- End of function --------------------------------------------------------*/

static int v34_get_bit(void *user_data)
{
    int bit;
    //static int tx_bits = 0;
    static int xxx = 0;

#if 1
    //bit = rand() & 1;
    bit = 1;
#endif
    tx_buf[tx_ptr++] = bit;
    if (tx_ptr > 1000)
        tx_ptr = 0;
    /*endif*/
    //printf("Tx bit %d\n", bit);
#if 0
    if (++tx_bits > 100000)
    {
        tx_bits = 0;
        bit = 2;
    }
    /*endif*/
#endif
    return bit;
}
/*- End of function --------------------------------------------------------*/

static void v34_put_bit(void *user_data, int bit)
{
#if 0
    int i;
    int len;
    complexf_t *coeffs;
#endif

    if (bit < 0)
    {
        /* Special conditions */
        printf("V.34 rx status is %s (%d)\n", signal_status_to_str(bit), bit);
#if 0
        switch (bit)
        {
        case SIG_STATUS_TRAINING_SUCCEEDED:
            if ((en = v34_equalizer_state(s, &coeffs)))
            {
                printf("Equalizer:\n");
                for (i = 0;  i < len;  i++)
#if defined(SPANDSP_USE_FIXED_POINT)
                    printf("%3d (%15.5f, %15.5f)\n", i, coeffs[i].re/V34_CONSTELLATION_SCALING_FACTOR, coeffs[i].im/V34_CONSTELLATION_SCALING_FACTOR);
#else
                    printf("%3d (%15.5f, %15.5f) -> %15.5f\n", i, coeffs[i].re, coeffs[i].im, powerf(&coeffs[i]));
#endif
                /*endfor*/
            }
            /*endif*/
            break;
        }
        /*endswitch*/
#endif
        return;
    }
    /*endif*/

    if (bit != tx_buf[rx_ptr])
    {
        printf("Rx bit %d - %d %d\n", rx_bits, bit, tx_buf[rx_ptr]);
        rx_bad_bits++;
    }
    /*endif*/
    rx_ptr++;
    if (rx_ptr > 1000)
        rx_ptr = 0;
    /*endif*/
    rx_bits++;
    if ((rx_bits % 100000) == 0)
    {
        printf("%d bits received, %d bad bits\r", rx_bits, rx_bad_bits);
        fflush(stdout);
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

#if 0
static void qam_report(void *user_data, const complexf_t *constel, const complexf_t *target, int symbol)
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
        constel_point.re = constel->re/V29_CONSTELLATION_SCALING_FACTOR;
        constel_point.im = constel->im/V29_CONSTELLATION_SCALING_FACTOR;
        target_point.re = target->re/V29_CONSTELLATION_SCALING_FACTOR,
        target_point.im = target->im/V29_CONSTELLATION_SCALING_FACTOR,
#if defined(ENABLE_GUI)
        if (use_gui)
        {
            qam_monitor_update_constel(s->qam_monitor, constel);
            qam_monitor_update_carrier_tracking(s->qam_monitor, v34_rx_carrier_frequency(s->s));
            qam_monitor_update_symbol_tracking(s->qam_monitor, v34_rx_symbol_timing_correction(s->s));
        }
        /*endif*/
#endif
        fpower = (constel_point.re - target_point.re)*(constel_point.re - target_point.re)
               + (constel_point.im - target_point.im)*(constel_point.im - target_point.im);
        s->smooth_power = 0.95f*s->smooth_power + 0.05f*fpower;
        printf("%8d [%8.4f, %8.4f] [%8.4f, %8.4f] %2x %8.4f %8.4f %9.4f %7.3f %7.4f\n",
               s->symbol_no,
               constel_point.re,
               constel_point.im,
               target_point.re,
               target_point.im,
               symbol,
               fpower,
               s->smooth_power,
               v34_rx_carrier_frequency(s->s),
               v34_rx_signal_power(s->s),
               v34_rx_symbol_timing_correction(s->s));
        s->symbol_no++;
    }
    else
    {
        printf("Gardner step %d\n", symbol);
        if ((len = v34_equalizer_state(s->s, &coeffs)))
        {
            printf("Equalizer A:\n");
            for (i = 0;  i < len;  i++)
            {
#if defined(SPANDSP_USE_FIXED_POINT)
                printf("%3d (%15.5f, %15.5f)\n", i, coeffs[i].re/V34_CONSTELLATION_SCALING_FACTOR, coeffs[i].im/V34_CONSTELLATION_SCALING_FACTOR);
#else
                printf("%3d (%15.5f, %15.5f) -> %15.5f\n", i, coeffs[i].re, coeffs[i].im, powerf(&coeffs[i]));
#endif
            }
            /*endfor*/
#if defined(ENABLE_GUI)
            if (use_gui)
                qam_monitor_update_equalizer(s->qam_monitor, coeffs, len);
            /*endif*/
#endif
        }
        /*endif*/
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/
#endif

#if 1
SPAN_DECLARE(int) v34_get_mapping_frame(v34_tx_state_t *s, int16_t bits[16]);
SPAN_DECLARE(void) v34_put_mapping_frame(v34_rx_state_t *s, int16_t bits[16]);

static void v34_mapping_frame_tests(int test_baud_rate, int test_bps, bool duplex)
{
    logging_state_t *logging;
    int i;
    int j;
    int16_t bits[16];

    /* Test the bit stream -> 4D symbol -> bit stream cycle */
    if (v34_init(&v34_caller, test_baud_rate, test_bps, false, duplex, v34_get_bit, &v34_caller, v34_put_bit, &v34_caller) == NULL)
    {
        fprintf(stderr, "    Cannot init V.34\n");
        exit(2);
    }
    /*endif*/
    v34_set_get_aux_bit(&v34_caller, v34_get_aux_bit, &v34_caller);
    v34_set_put_aux_bit(&v34_caller, v34_put_aux_bit, &v34_caller);
    logging = v34_get_logging_state(&v34_caller);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "caller  ");

    if (v34_init(&v34_answerer, test_baud_rate, test_bps, true, duplex, v34_get_bit, &v34_answerer, v34_put_bit, &v34_answerer) == NULL)
    {
        fprintf(stderr, "    Cannot init V.34\n");
        exit(2);
    }
    /*endif*/
    v34_set_get_aux_bit(&v34_answerer, v34_get_aux_bit, &v34_caller);
    v34_set_put_aux_bit(&v34_answerer, v34_put_aux_bit, &v34_caller);
    logging = v34_get_logging_state(&v34_answerer);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "answerer");

    for (i = 0;  i < 1000;  i++)
    {
        v34_get_mapping_frame(&v34_answerer.tx, bits);
#if 0
        /* Copy the elements of the 4D block over */
        for (j = 0;  j < 8;  j++)
        {
            v34_caller.rx.mjk[j] = v34_answerer.tx.mjk[j];
            v34_caller.rx.qbits[j] = v34_answerer.tx.qbits[j];
        }
        /*endfor*/
        for (j = 0;  j < 4;  j++)
            v34_caller.rx.ibits[j] = v34_answerer.tx.ibits[j];
        /*endfor*/
#endif
#if 1
        for (j = 0;  j < 8;  j++)
        {
            printf("Bits %d %d\n", bits[2*j], bits[2*j + 1]);
        }
        /*endfor*/
#endif
        v34_put_mapping_frame(&v34_caller.rx, bits);
    }
    /*endfor*/
}
/*- End of function --------------------------------------------------------*/
#endif

int main(int argc, char *argv[])
{
    v8_parms_t v8_call_parms;
    int16_t caller_amp[SAMPLES_PER_CHUNK];
    int16_t answerer_amp[SAMPLES_PER_CHUNK];
    int16_t caller_model_amp[SAMPLES_PER_CHUNK];
    int16_t answerer_model_amp[SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    SNDFILE *inhandle;
    SNDFILE *outhandle;
    int outframes;
    int samples;
    int i;
    int line_model_no;
    int bits_per_test;
    int noise_level;
    int signal_level;
    int channel_codec;
    int opt;
    int caller_phase;
    int answerer_phase;
    int residue;
    bool calling_party;
    bool test_4d;
    bool duplex;
    bool log_audio;
    logging_state_t *logging;
    float echo_level;

    channel_codec = MUNGE_CODEC_NONE;
    test_baud_rate = 3429;
    test_bps = 33600;
    line_model_no = 0;
    noise_level = -70;
    signal_level = -13;
    echo_level = -99.0f;
    bits_per_test = 50000;
    decode_test_file = NULL;
    log_audio = false;
    calling_party = true;
    test_4d = false;
    duplex = true;
    while ((opt = getopt(argc, argv, "4a:b:B:c:d:D:e:ghlm:n:s:")) != -1)
    {
        switch (opt)
        {
        case '4':
            test_4d = true;
            break;
        case 'a':
            test_baud_rate = atoi(optarg);
            switch (test_baud_rate)
            {
            case 2400:
            case 2743:
            case 2800:
            case 3000:
            case 3200:
            case 3429:
                break;
            default:
                fprintf(stderr, "Invalid baud rate %d specified.\n", test_baud_rate);
                exit(2);
            }
            /*endswitch*/
            break;
        case 'b':
            test_bps = atoi(optarg);
            for (i = 0;  valid_bit_rates[i] > 0;  i++)
            {
                if (test_bps == valid_bit_rates[i])
                    break;
                /*endif*/
            }
            /*endfor*/
            if (valid_bit_rates[i] <= 0)
            {
                fprintf(stderr, "Invalid bit rate %d specified.\n", test_bps);
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
        case 'd':
            decode_test_file = optarg;
            calling_party = true;
            break;
        case 'D':
            decode_test_file = optarg;
            calling_party = false;
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
        case 'h':
            duplex = false;
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
    if (test_4d)
    {
#if 1
        v34_mapping_frame_tests(test_baud_rate, test_bps, duplex);
#endif
        printf("Wombat soup\n");
        fprintf(stderr, "Wombat soup\n");
        exit(0);
    }
    /*endif*/
    if (decode_test_file)
    {
        if ((inhandle = sf_open_telephony_read(decode_test_file, 1)) == NULL)
        {
            fprintf(stderr, "    Cannot open audio file '%s'\n", decode_test_file);
            exit(2);
        }
        /*endif*/
        outhandle = NULL;
        if (log_audio)
        {
            if ((outhandle = sf_open_telephony_write(OUT_FILE_NAME, 1)) == NULL)
            {
                fprintf(stderr, "    Cannot create audio file '%s'\n", OUT_FILE_NAME);
                exit(2);
            }
            /*endif*/
        }
        /*endif*/
        if (v34_init(&v34_caller, test_baud_rate, test_bps, calling_party, duplex, v34_get_bit, &v34_caller, v34_decode_putbit, &v34_caller) == NULL)
        {
            fprintf(stderr, "    Cannot init V.34\n");
            exit(2);
        }
        /*endif*/
        v34_set_get_aux_bit(&v34_caller, v34_get_aux_bit, &v34_caller);
        v34_set_put_aux_bit(&v34_caller, v34_put_aux_bit, &v34_caller);
        span_log_set_level(&v34_caller.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_FLOW);
        for (;;)
        {
            samples = sf_readf_short(inhandle, caller_amp, SAMPLES_PER_CHUNK);
            if (samples <= 0)
                break;
            /*endif*/
            v34_rx(&v34_caller, caller_amp, samples);
            v34_tx(&v34_caller, caller_amp, samples);
            if (outhandle)
            {
                outframes = sf_writef_short(outhandle, caller_amp, SAMPLES_PER_CHUNK);
                if (outframes != SAMPLES_PER_CHUNK)
                {
                    fprintf(stderr, "    Error writing audio file\n");
                    exit(2);
                }
                /*endif*/
            }
            /*endif*/
            span_log_bump_samples(&v34_caller.logging, samples);
        }
        /*endfor*/
        if (sf_close_telephony(inhandle))
        {
            fprintf(stderr, "    Cannot close audio file '%s'\n", decode_test_file);
            exit(2);
        }
        /*endif*/
        if (outhandle)
        {
            if (sf_close_telephony(outhandle))
            {
                fprintf(stderr, "    Cannot close audio file '%s'\n", OUT_FILE_NAME);
                exit(2);
            }
            /*endif*/
        }
        /*endif*/
        exit(0);
    }
    /*endif*/
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

    v8_call_parms.modem_connect_tone = MODEM_CONNECT_TONES_NONE;
    if (duplex)
    {
        v8_call_parms.call_function = V8_CALL_V_SERIES;
        v8_call_parms.modulations = V8_MOD_V32
                                  | V8_MOD_V34;
        v8_call_parms.protocol = V8_PROTOCOL_LAPM_V42;
    }
    else
    {
        //v8_call_parms.call_function = V8_CALL_T30_RX;
        v8_call_parms.call_function = V8_CALL_T30_TX;
        v8_call_parms.modulations = V8_MOD_V21
                                  | V8_MOD_V27TER
                                  | V8_MOD_V29
                                  | V8_MOD_V17
                                  | V8_MOD_V34HDX;
        v8_call_parms.protocol = V8_PROTOCOL_NONE;
    }
    /*endif*/
    v8_call_parms.pcm_modem_availability = 0;
    v8_call_parms.pstn_access = 0;
    v8_call_parms.nsf = -1;
    v8_call_parms.t66 = -1;
    v8_init(&v8_caller, true, &v8_call_parms, v8_handler, &v34_caller);
    logging = v8_get_logging_state(&v8_caller);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "caller  ");

    if (v34_init(&v34_caller, test_baud_rate, test_bps, true, true, v34_get_bit, &v34_caller, v34_put_bit, &v34_caller) == NULL)
    {
        fprintf(stderr, "    Cannot init V.34\n");
        exit(2);
    }
    /*endif*/
    v34_set_get_aux_bit(&v34_caller, v34_get_aux_bit, &v34_caller);
    v34_set_put_aux_bit(&v34_caller, v34_put_aux_bit, &v34_caller);
    v34_tx_power(&v34_caller, signal_level);
    //v34_set_qam_report_handler(&v34_caller, qam_report, (void *) &qam_caller);
    logging = v34_get_logging_state(&v34_caller);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "caller  ");

    v8_call_parms.modem_connect_tone = MODEM_CONNECT_TONES_ANSAM_PR;
    if (duplex)
    {
        v8_call_parms.call_function = V8_CALL_V_SERIES;
        v8_call_parms.modulations = V8_MOD_V32
                                  | V8_MOD_V34;
        v8_call_parms.protocol = V8_PROTOCOL_LAPM_V42;
    }
    else
    {
        v8_call_parms.call_function = V8_CALL_T30_RX;
        v8_call_parms.modulations = V8_MOD_V21
                                  | V8_MOD_V27TER
                                  | V8_MOD_V29
                                  | V8_MOD_V17
                                  | V8_MOD_V34HDX;
        v8_call_parms.protocol = V8_PROTOCOL_NONE;
        v8_call_parms.pcm_modem_availability = 0;
    }
    /*endif*/
    v8_call_parms.pstn_access = 0;
    v8_call_parms.nsf = -1;
    v8_call_parms.t66 = -1;
    v8_init(&v8_answerer, false, &v8_call_parms, v8_handler, &v34_answerer);
    logging = v8_get_logging_state(&v8_answerer);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "answerer");

    if (v34_init(&v34_answerer, test_baud_rate, test_bps, false, true, v34_get_bit, &v34_answerer, v34_put_bit, &v34_answerer) == NULL)
    {
        fprintf(stderr, "    Cannot init V.34\n");
        exit(2);
    }
    /*endif*/
    v34_set_get_aux_bit(&v34_answerer, v34_get_aux_bit, &v34_caller);
    v34_set_put_aux_bit(&v34_answerer, v34_put_aux_bit, &v34_caller);
    v34_tx_power(&v34_answerer, signal_level);
    //v34_set_qam_report_handler(&v34_answerer, qam_report, (void *) &qam_answerer);
    logging = v34_get_logging_state(&v34_answerer);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "answerer");

    qam_caller.s = &v34_caller;
    qam_caller.smooth_power = 0.0f;
    qam_caller.symbol_no = 0;

    qam_answerer.s = &v34_answerer;
    qam_answerer.smooth_power = 0.0f;
    qam_answerer.symbol_no = 0;

#if defined(ENABLE_GUI)
    if (use_gui)
    {
        qam_caller.qam_monitor = qam_monitor_init(45.0f, V34_CONSTELLATION_SCALING_FACTOR, "Calling modem");
        qam_answerer.qam_monitor = qam_monitor_init(45.0f, V34_CONSTELLATION_SCALING_FACTOR, "Answering modem");
    }
    /*endif*/
#endif

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
    caller_phase = 0;
    answerer_phase = 0;
    for (;;)
    {
        samples = 0;
        if (caller_phase == 0)
        {
            samples = v8_tx(&v8_caller, caller_amp, SAMPLES_PER_CHUNK);
            if (samples < SAMPLES_PER_CHUNK)
            {
                printf("Caller V.8 ends (%d)\n", samples);
                caller_phase = 1;
            }
            /*endif*/
        }
        /*endif*/
        if (samples < SAMPLES_PER_CHUNK)
            samples += v34_tx(&v34_caller, &caller_amp[samples], SAMPLES_PER_CHUNK - samples);
        /*endif*/
        if (samples < SAMPLES_PER_CHUNK)
        {
printf("Caller silence %d\n", SAMPLES_PER_CHUNK - samples);
            vec_zeroi16(&caller_amp[samples], SAMPLES_PER_CHUNK - samples);
            samples = SAMPLES_PER_CHUNK;
        }
        /*endif*/
#if defined(ENABLE_GUI)
        if (use_gui)
            qam_monitor_update_audio_level(qam_caller.qam_monitor, caller_amp, samples);
        /*endif*/
#endif

        samples = 0;
        if (answerer_phase == 0)
        {
            samples = v8_tx(&v8_answerer, answerer_amp, SAMPLES_PER_CHUNK);
            if (samples < SAMPLES_PER_CHUNK)
            {
                printf("Answerer V.8 ends (%d)\n", samples);
                answerer_phase = 1;
            }
            /*endif*/
        }
        /*endif*/
        if (samples < SAMPLES_PER_CHUNK)
            samples += v34_tx(&v34_answerer, &answerer_amp[samples], SAMPLES_PER_CHUNK - samples);
        /*endif*/
        if (samples < SAMPLES_PER_CHUNK)
        {
printf("Answerer silence %d\n", SAMPLES_PER_CHUNK - samples);
            vec_zeroi16(&answerer_amp[samples], SAMPLES_PER_CHUNK - samples);
            samples = SAMPLES_PER_CHUNK;
        }
        /*endif*/
#if defined(ENABLE_GUI)
        if (use_gui)
            qam_monitor_update_audio_level(qam_answerer.qam_monitor, answerer_amp, samples);
        /*endif*/
#endif

        if (samples == 0)
        {
            if (caller_phase == 0)
            {
                printf("Phase change\n");
                caller_phase = 1;
            }
            else
            {
                printf("Restarting on zero output\n");
                v34_restart(&v34_answerer, test_baud_rate, test_bps, duplex);
                rx_ptr = 0;
                tx_ptr = 0;
            }
            /*endif*/
        }
        /*endif*/

        both_ways_line_model(model,
                             caller_model_amp,
                             caller_amp,
                             answerer_model_amp,
                             answerer_amp,
                             samples);

        if (answerer_phase == 0)
        {
            if ((residue = v8_rx(&v8_answerer, caller_model_amp, samples)))
            {
                //printf("Phase change\n");
                //answerer_phase = 1;
            }
            /*endif*/
        }
        else
        {
            if ((residue = v34_rx(&v34_answerer, caller_amp, samples)))
            {
                printf("Restarting on zero output\n");
                v34_restart(&v34_answerer, test_baud_rate, test_bps, duplex);
                rx_ptr = 0;
                tx_ptr = 0;
            }
            /*endif*/
        }
        /*endif*/
        for (i = 0;  i < samples;  i++)
            out_amp[2*i] = caller_model_amp[i];
        /*endfor*/
        for (  ;  i < SAMPLES_PER_CHUNK;  i++)
            out_amp[2*i] = 0;
        /*endfor*/

        if (caller_phase == 0)
        {
            if ((residue = v8_rx(&v8_caller, answerer_model_amp, samples)))
            {
                //printf("Phase change\n");
                //caller_phase = 1;
            }
            /*endif*/
        }
        else
        {
            if ((residue = v34_rx(&v34_caller, answerer_amp, samples)))
            {
                printf("Restarting on zero output\n");
                v34_restart(&v34_caller, test_baud_rate, test_bps, duplex);
                rx_ptr = 0;
                tx_ptr = 0;
            }
            /*endif*/
        }
        /*endif*/
        for (i = 0;  i < samples;  i++)
            out_amp[2*i + 1] = answerer_model_amp[i];
        /*endfor*/
        for (  ;  i < SAMPLES_PER_CHUNK;  i++)
            out_amp[2*i + 1] = 0;
        /*endfor*/

        if (log_audio)
        {
            outframes = sf_writef_short(outhandle, out_amp, SAMPLES_PER_CHUNK);
            if (outframes != SAMPLES_PER_CHUNK)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
            /*endif*/
        }
        /*endif*/
    }
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
