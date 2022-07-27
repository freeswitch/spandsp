//#define LOG_FAX_AUDIO
/*
 * SpanDSP - a series of DSP components for telephony
 *
 * fax.c - Analogue line ITU T.30 FAX transfer processing
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003, 2005, 2006 Steve Underwood
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
#include <string.h>
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
#include <assert.h>
#include <fcntl.h>
#include <time.h>
#if defined(LOG_FAX_AUDIO)
#include <unistd.h>
#endif
#include <tiffio.h>

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/logging.h"
#include "spandsp/queue.h"
#include "spandsp/dc_restore.h"
#include "spandsp/vector_int.h"
#include "spandsp/power_meter.h"
#include "spandsp/complex.h"
#include "spandsp/tone_detect.h"
#include "spandsp/tone_generate.h"
#include "spandsp/async.h"
#include "spandsp/hdlc.h"
#include "spandsp/silence_gen.h"
#include "spandsp/super_tone_rx.h"
#include "spandsp/fsk.h"
#include "spandsp/modem_connect_tones.h"
#include "spandsp/v8.h"
#include "spandsp/v29tx.h"
#include "spandsp/v29rx.h"
#include "spandsp/v27ter_tx.h"
#include "spandsp/v27ter_rx.h"
#include "spandsp/v17tx.h"
#include "spandsp/v17rx.h"
#if defined(SPANDSP_SUPPORT_V34)
#include "spandsp/bitstream.h"
#include "spandsp/v34.h"
#endif
#include "spandsp/timezone.h"
#include "spandsp/t4_rx.h"
#include "spandsp/t4_tx.h"
#include "spandsp/image_translate.h"
#include "spandsp/t81_t82_arith_coding.h"
#include "spandsp/t85.h"
#include "spandsp/t42.h"
#include "spandsp/t43.h"
#include "spandsp/t4_t6_decode.h"
#include "spandsp/t4_t6_encode.h"

#include "spandsp/t30_fcf.h"
#include "spandsp/t35.h"
#include "spandsp/t30.h"
#include "spandsp/t30_api.h"
#include "spandsp/t30_logging.h"

#include "spandsp/fax_modems.h"
#include "spandsp/fax.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/silence_gen.h"
#include "spandsp/private/power_meter.h"
#include "spandsp/private/fsk.h"
#include "spandsp/private/modem_connect_tones.h"
#include "spandsp/private/v8.h"
#if defined(SPANDSP_SUPPORT_V34)
#include "spandsp/private/bitstream.h"
#include "spandsp/private/v34.h"
#endif
#include "spandsp/private/v17tx.h"
#include "spandsp/private/v17rx.h"
#include "spandsp/private/v27ter_tx.h"
#include "spandsp/private/v27ter_rx.h"
#include "spandsp/private/v29tx.h"
#include "spandsp/private/v29rx.h"
#include "spandsp/private/hdlc.h"
#include "spandsp/private/fax_modems.h"
#include "spandsp/private/timezone.h"
#include "spandsp/private/t81_t82_arith_coding.h"
#include "spandsp/private/t85.h"
#include "spandsp/private/t42.h"
#include "spandsp/private/t43.h"
#include "spandsp/private/t4_t6_decode.h"
#include "spandsp/private/t4_t6_encode.h"
#include "spandsp/private/image_translate.h"
#include "spandsp/private/t4_rx.h"
#include "spandsp/private/t4_tx.h"
#include "spandsp/private/t30.h"
#include "spandsp/private/fax.h"

#define HDLC_FRAMING_OK_THRESHOLD       8

static void tone_detected(void *user_data, int tone, int level, int delay)
{
    t30_state_t *s;

    s = (t30_state_t *) user_data;
    span_log(&s->logging, SPAN_LOG_FLOW, "%s detected (%ddBm0)\n", modem_connect_tone_to_str(tone), level);
}
/*- End of function --------------------------------------------------------*/

static void v8_handler(void *user_data, v8_parms_t *result)
{
    fax_state_t *s;

    s = (fax_state_t *) user_data;
    span_log(&s->logging, SPAN_LOG_FLOW, "V.8 report received\n");
}
/*- End of function --------------------------------------------------------*/

static void hdlc_underflow_handler(void *user_data)
{
    t30_state_t *s;

    s = (t30_state_t *) user_data;
    t30_front_end_status(s, T30_FRONT_END_SEND_STEP_COMPLETE);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) fax_rx(fax_state_t *s, int16_t *amp, int len)
{
    int i;

#if defined(LOG_FAX_AUDIO)
    if (s->modems.audio_rx_log >= 0)
        write(s->modems.audio_rx_log, amp, len*sizeof(int16_t));
#endif
    for (i = 0;  i < len;  i++)
        amp[i] = dc_restore(&s->modems.dc_restore, amp[i]);
    /*endfor*/
    if (s->modems.rx_handler)
        s->modems.rx_handler(s->modems.rx_user_data, amp, len);
    /*endif*/
    t30_timer_update(&s->t30, len);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) fax_rx_fillin(fax_state_t *s, int len)
{
    /* To mitigate the effect of lost packets on a packet network we should
       try to sustain the status quo. If there is no receive modem running, keep
       things that way. If there is a receive modem running, try to sustain its
       operation, without causing a phase hop, or letting its adaptive functions
       diverge. */
#if defined(LOG_FAX_AUDIO)
    if (s->modems.audio_rx_log >= 0)
    {
        int i;
#if defined(_MSC_VER)
        int16_t *amp = (int16_t *) _alloca(sizeof(int16_t)*len);
#else
        int16_t amp[len];
#endif

        vec_zeroi16(amp, len);
        write(s->modems.audio_rx_log, amp, len*sizeof(int16_t));
    }
    /*endif*/
#endif
    /* Call the fillin function of the current modem (if there is one). */
    s->modems.rx_fillin_handler(s->modems.rx_fillin_user_data, len);
    t30_timer_update(&s->t30, len);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) fax_tx(fax_state_t *s, int16_t *amp, int max_len)
{
    int len;
#if defined(LOG_FAX_AUDIO)
    int required_len;

    required_len = max_len;
#endif
    len = 0;
    while (s->modems.transmit  &&  (len += s->modems.tx_handler(s->modems.tx_user_data, &amp[len], max_len - len)) < max_len)
    {
        /* Allow for a change of tx handler within a block */
        if (fax_modems_set_next_tx_type(&s->modems)  &&  s->modems.current_tx_type != T30_MODEM_NONE  &&  s->modems.current_tx_type != T30_MODEM_DONE)
            t30_front_end_status(&s->t30, T30_FRONT_END_SEND_STEP_COMPLETE);
        /*endif*/
    }
    /*endwhile*/
    if (s->modems.transmit_on_idle)
    {
        /* Pad to the requested length with silence */
        memset(&amp[len], 0, (max_len - len)*sizeof(int16_t));
        len = max_len;
    }
    /*endif*/
#if defined(LOG_FAX_AUDIO)
    if (s->modems.audio_tx_log >= 0)
    {
        if (len < required_len)
            memset(&amp[len], 0, (required_len - len)*sizeof(int16_t));
        /*endif*/
        write(s->modems.audio_tx_log, amp, required_len*sizeof(int16_t));
    }
    /*endif*/
#endif
    return len;
}
/*- End of function --------------------------------------------------------*/

static int fax_get_phase(void *user_data)
{
    t30_state_t *s = user_data;
    return s->phase;
}

static void fax_set_rx_type(void *user_data, int type, int bit_rate, int short_train, int use_hdlc)
{
    fax_state_t *s;
    fax_modems_state_t *t;

    s = (fax_state_t *) user_data;
    t = &s->modems;

    if (s->t30.sslfax.server && type != T30_MODEM_DONE)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Set fallback rx type %d%s\n", type, use_hdlc ? " (HDLC)" : "");
        fax_modems_set_rx_handler(t, (span_rx_handler_t) &sslfax_rx, &s->t30.sslfax, (span_rx_fillin_handler_t) NULL, &s->t30.sslfax);
        sslfax_setup(&s->t30.sslfax, &s->t30, t30_non_ecm_put, t30_non_ecm_get, t30_hdlc_accept, hdlc_underflow_handler, s->t30.sslfax.tx_use_hdlc, use_hdlc, fax_get_phase);
        t->rx_bit_rate = bit_rate;
        t->current_rx_type = type;
        if (use_hdlc)
        {
            t30_hdlc_accept(&s->t30, NULL, SIG_STATUS_CARRIER_UP, true);
            t30_hdlc_accept(&s->t30, NULL, SIG_STATUS_FRAMING_OK, true);
        }
        return;
    }

    span_log(&s->logging, SPAN_LOG_FLOW, "Set rx type %d\n", type);
    if (t->current_rx_type == type)
        return;
    /*endif*/
    t->current_rx_type = type;
    t->rx_bit_rate = bit_rate;
    hdlc_rx_init(&t->hdlc_rx, false, true, HDLC_FRAMING_OK_THRESHOLD, fax_modems_hdlc_accept, t);

    switch (type)
    {
    case T30_MODEM_V21:
        fax_modems_start_slow_modem(t, FAX_MODEM_V21_RX);
        break;
    case T30_MODEM_V17:
        fax_modems_start_fast_modem(t, FAX_MODEM_V17_RX, bit_rate, short_train, use_hdlc);
        break;
    case T30_MODEM_V27TER:
        fax_modems_start_fast_modem(t, FAX_MODEM_V27TER_RX, bit_rate, short_train, use_hdlc);
        break;
    case T30_MODEM_V29:
        fax_modems_start_fast_modem(t, FAX_MODEM_V29_RX, bit_rate, short_train, use_hdlc);
        break;
    case T30_MODEM_DONE:
        span_log(&s->logging, SPAN_LOG_FLOW, "FAX exchange complete\n");
        /* Fall through */
    default:
        fax_modems_set_rx_handler(t, (span_rx_handler_t) &span_dummy_rx, s, (span_rx_fillin_handler_t) &span_dummy_rx_fillin, s);
        break;
    }
    /*endswitch*/
}
/*- End of function --------------------------------------------------------*/

static void fax_set_tx_type(void *user_data, int type, int bit_rate, int short_train, int use_hdlc)
{
    fax_state_t *s;
    fax_modems_state_t *t;
    int tone;

    s = (fax_state_t *) user_data;
    t = &s->modems;

    if (s->t30.sslfax.server && type != T30_MODEM_DONE)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Set fallback tx type %d%s\n", type, use_hdlc ? " (HDLC)" : "");
        fax_modems_set_tx_handler(t, (span_tx_handler_t) &sslfax_tx, &s->t30.sslfax);
        fax_modems_set_next_tx_handler(t, (span_tx_handler_t) &sslfax_tx, &s->t30.sslfax);
        sslfax_setup(&s->t30.sslfax, &s->t30, t30_non_ecm_put, t30_non_ecm_get, t30_hdlc_accept, hdlc_underflow_handler, use_hdlc, s->t30.sslfax.rx_use_hdlc, fax_get_phase);
        t->transmit = true;
        t->tx_bit_rate = bit_rate;
        t->current_tx_type = type;
        return;
    }

    span_log(&s->logging, SPAN_LOG_FLOW, "Set tx type %d\n", type);
    if (t->current_tx_type == type)
        return;
    /*endif*/
    switch (type)
    {
    case T30_MODEM_PAUSE:
        silence_gen_alter(&t->silence_gen, ms_to_samples(short_train));
        fax_modems_set_tx_handler(t, (span_tx_handler_t) &silence_gen, &t->silence_gen);
        fax_modems_set_next_tx_handler(t, (span_tx_handler_t) NULL, NULL);
        t->transmit = true;
        break;
    case T30_MODEM_CED:
    case T30_MODEM_CNG:
        tone = (type == T30_MODEM_CED)  ?  FAX_MODEM_CED_TONE_TX  :  FAX_MODEM_CNG_TONE_TX;
        fax_modems_start_slow_modem(t, tone);
        fax_modems_set_next_tx_handler(t, (span_tx_handler_t) NULL, NULL);
        t->transmit = true;
        break;
    case T30_MODEM_V21:
        fax_modems_start_slow_modem(t, FAX_MODEM_V21_TX);
        /* The spec says 1s +-15% of preamble. So, the minimum is 32 octets. */
        fax_modems_hdlc_tx_flags(t, 32);
        /* Pause before switching from phase C, as per T.30 5.3.2.2. If we omit this, the receiver
           might not see the carrier fall between the high speed and low speed sections. In practice,
           a 75ms gap before any V.21 transmission is harmless, adds little to the overall length of
           a call, and ensures the receiving end is ready. */
        silence_gen_alter(&t->silence_gen, ms_to_samples(75));
        fax_modems_set_tx_handler(t, (span_tx_handler_t) &silence_gen, &t->silence_gen);
        fax_modems_set_next_tx_handler(t, (span_tx_handler_t) &fsk_tx, &t->v21_tx);
        t->transmit = true;
        break;
    case T30_MODEM_V17:
        silence_gen_alter(&t->silence_gen, ms_to_samples(75));
        /* For any fast modem, set 200ms of preamble flags */
        fax_modems_hdlc_tx_flags(t, bit_rate/(8*5));
        fax_modems_start_fast_modem(t, FAX_MODEM_V17_TX, bit_rate, short_train, use_hdlc);
        fax_modems_set_tx_handler(t, (span_tx_handler_t) &silence_gen, &t->silence_gen);
        fax_modems_set_next_tx_handler(t, (span_tx_handler_t) &v17_tx, &t->fast_modems.v17_tx);
        t->transmit = true;
        break;
    case T30_MODEM_V27TER:
        silence_gen_alter(&t->silence_gen, ms_to_samples(75));
        /* For any fast modem, set 200ms of preamble flags */
        fax_modems_hdlc_tx_flags(t, bit_rate/(8*5));
        fax_modems_start_fast_modem(t, FAX_MODEM_V27TER_TX, bit_rate, short_train, use_hdlc);
        fax_modems_set_tx_handler(t, (span_tx_handler_t) &silence_gen, &t->silence_gen);
        fax_modems_set_next_tx_handler(t, (span_tx_handler_t) &v27ter_tx, &t->fast_modems.v27ter_tx);
        t->transmit = true;
        break;
    case T30_MODEM_V29:
        silence_gen_alter(&t->silence_gen, ms_to_samples(75));
        /* For any fast modem, set 200ms of preamble flags */
        fax_modems_hdlc_tx_flags(t, bit_rate/(8*5));
        fax_modems_start_fast_modem(t, FAX_MODEM_V29_TX, bit_rate, short_train, use_hdlc);
        fax_modems_set_tx_handler(t, (span_tx_handler_t) &silence_gen, &t->silence_gen);
        fax_modems_set_next_tx_handler(t, (span_tx_handler_t) &v29_tx, &t->fast_modems.v29_tx);
        t->transmit = true;
        break;
    case T30_MODEM_DONE:
        span_log(&s->logging, SPAN_LOG_FLOW, "FAX exchange complete\n");
        /* Fall through */
    default:
        silence_gen_alter(&t->silence_gen, 0);
        fax_modems_set_tx_handler(t, (span_tx_handler_t) &silence_gen, &t->silence_gen);
        fax_modems_set_next_tx_handler(t, (span_tx_handler_t) NULL, NULL);
        t->transmit = false;
        break;
    }
    /*endswitch*/
    t->tx_bit_rate = bit_rate;
    t->current_tx_type = type;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) fax_set_transmit_on_idle(fax_state_t *s, int transmit_on_idle)
{
    s->modems.transmit_on_idle = transmit_on_idle;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) fax_set_tep_mode(fax_state_t *s, int use_tep)
{
    fax_modems_set_tep_mode(&s->modems, use_tep);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(t30_state_t *) fax_get_t30_state(fax_state_t *s)
{
    return &s->t30;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) fax_get_logging_state(fax_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) fax_restart(fax_state_t *s, bool calling_party)
{
    v8_parms_t v8_parms;

    fax_modems_restart(&s->modems);
    v8_parms.modem_connect_tone = MODEM_CONNECT_TONES_ANSAM_PR;
    v8_parms.call_function = V8_CALL_T30_RX;
    v8_parms.modulations = V8_MOD_V21;
    if (s->t30.supported_modems & T30_SUPPORT_V27TER)
        v8_parms.modulations |= V8_MOD_V27TER;
    /*endif*/
    if (s->t30.supported_modems & T30_SUPPORT_V29)
        v8_parms.modulations |= V8_MOD_V29;
    /*endif*/
    if (s->t30.supported_modems & T30_SUPPORT_V17)
        v8_parms.modulations |= V8_MOD_V17;
    /*endif*/
    if (s->t30.supported_modems & T30_SUPPORT_V34HDX)
        v8_parms.modulations |= V8_MOD_V34HDX;
    /*endif*/
    v8_parms.protocol = V8_PROTOCOL_NONE;
    v8_parms.pcm_modem_availability = 0;
    v8_parms.pstn_access = 0;
    v8_parms.nsf = -1;
    v8_parms.t66 = -1;
    v8_restart(&s->v8, calling_party, &v8_parms);
    t30_restart(&s->t30, calling_party);
#if defined(LOG_FAX_AUDIO)
    {
        char buf[100 + 1];
        struct tm *tm;
        time_t now;

        time(&now);
        tm = localtime(&now);
        sprintf(buf,
                "/tmp/fax-rx-audio-%p-%02d%02d%02d%02d%02d%02d",
                s,
                tm->tm_year%100,
                tm->tm_mon + 1,
                tm->tm_mday,
                tm->tm_hour,
                tm->tm_min,
                tm->tm_sec);
        s->modems.audio_rx_log = open(buf, O_CREAT | O_TRUNC | O_WRONLY, 0666);
        sprintf(buf,
                "/tmp/fax-tx-audio-%p-%02d%02d%02d%02d%02d%02d",
                s,
                tm->tm_year%100,
                tm->tm_mon + 1,
                tm->tm_mday,
                tm->tm_hour,
                tm->tm_min,
                tm->tm_sec);
        s->modems.audio_tx_log = open(buf, O_CREAT | O_TRUNC | O_WRONLY, 0666);
    }
#endif
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(fax_state_t *) fax_init(fax_state_t *s, bool calling_party)
{
    v8_parms_t v8_parms;

    if (s == NULL)
    {
        if ((s = (fax_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
        /*endif*/
    }
    /*endif*/
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "FAX");
    fax_modems_init(&s->modems,
                    false,
                    t30_hdlc_accept,
                    hdlc_underflow_handler,
                    t30_non_ecm_put_bit,
                    t30_non_ecm_get_bit,
                    tone_detected,
                    &s->t30);
    t30_init(&s->t30,
             calling_party,
             fax_set_rx_type,
             (void *) s,
             fax_set_tx_type,
             (void *) s,
             fax_modems_hdlc_tx_frame,
             (void *) &s->modems);
    t30_set_supported_modems(&s->t30, T30_SUPPORT_V27TER | T30_SUPPORT_V29 | T30_SUPPORT_V17);
    v8_parms.modem_connect_tone = MODEM_CONNECT_TONES_ANSAM_PR;
    v8_parms.call_function = V8_CALL_T30_RX;
    v8_parms.modulations = V8_MOD_V21;
    if (s->t30.supported_modems & T30_SUPPORT_V27TER)
        v8_parms.modulations |= V8_MOD_V27TER;
    /*endif*/
    if (s->t30.supported_modems & T30_SUPPORT_V29)
        v8_parms.modulations |= V8_MOD_V29;
    /*endif*/
    if (s->t30.supported_modems & T30_SUPPORT_V17)
        v8_parms.modulations |= V8_MOD_V17;
    /*endif*/
    if (s->t30.supported_modems & T30_SUPPORT_V34HDX)
        v8_parms.modulations |= V8_MOD_V34HDX;
    /*endif*/
    v8_parms.protocol = V8_PROTOCOL_NONE;
    v8_parms.pcm_modem_availability = 0;
    v8_parms.pstn_access = 0;
    v8_parms.nsf = -1;
    v8_parms.t66 = -1;
    v8_init(&s->v8, calling_party, &v8_parms, v8_handler, s);
    fax_restart(s, calling_party);
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) fax_release(fax_state_t *s)
{
    t30_release(&s->t30);
    v8_release(&s->v8);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) fax_free(fax_state_t *s)
{
    fax_release(s);
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
