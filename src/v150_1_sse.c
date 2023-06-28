/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v150_1_sse.c - An implementation of the SSE protocol defined in V.150.1
 *                Annex C, less the packet exchange part
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2022 Steve Underwood
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <inttypes.h>
#include <memory.h>
#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#else
#include <spandsp/stdbool.h>
#endif

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/unaligned.h"
#include "spandsp/logging.h"
#include "spandsp/async.h"
#include "spandsp/v150_1_sse.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/v150_1_sse.h"

static int v150_1_sse_tx_modem_relay_packet(v150_1_sse_state_t *s, int x, int ric, int ricinfo);
static int v150_1_sse_tx_fax_relay_packet(v150_1_sse_state_t *s, int x, int ric, int ricinfo);

SPAN_DECLARE(const char *) v150_1_sse_media_state_to_str(int state)
{
    const char *res;

    res = "unknown";
    switch (state)
    {
    case V150_1_SSE_MEDIA_STATE_INITIAL_AUDIO:
        res = "Initial audio";
        break;
    case V150_1_SSE_MEDIA_STATE_VOICE_BAND_DATA:
        res = "Voice band data";
        break;
    case V150_1_SSE_MEDIA_STATE_MODEM_RELAY:
        res = "Modem relay";
        break;
    case V150_1_SSE_MEDIA_STATE_FAX_RELAY:
        res = "Fax relay";
        break;
    case V150_1_SSE_MEDIA_STATE_TEXT_RELAY:
        res = "Text relay";
        break;
    case V150_1_SSE_MEDIA_STATE_TEXT_PROBE:
        res = "Text probe";
        break;
    }
    /*endswitch*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) v150_1_sse_ric_to_str(int ric)
{
    const char *res;

    res = "unknown";
    switch (ric)
    {
    case V150_1_SSE_RIC_V8_CM:
        res = "V.8 CM";
        break;
    case V150_1_SSE_RIC_V8_JM:
        res = "V.8 JM";
        break;
    case V150_1_SSE_RIC_V32BIS_AA:
        res = "V.32/V.32bis AA";
        break;
    case V150_1_SSE_RIC_V32BIS_AC:
        res = "V.32/V.32bis AC";
        break;
    case V150_1_SSE_RIC_V22BIS_USB1:
        res = "V.22bis USB1";
        break;
    case V150_1_SSE_RIC_V22BIS_SB1:
        res = "V.22bis SB1";
        break;
    case V150_1_SSE_RIC_V22BIS_S1:
        res = "V.22bis S1";
        break;
    case V150_1_SSE_RIC_V21_CH2:
        res = "V.21 Ch2";
        break;
    case V150_1_SSE_RIC_V21_CH1:
        res = "V.21 Ch1";
        break;
    case V150_1_SSE_RIC_V23_HIGH_CHANNEL:
        res = "V.23 high channel";
        break;
    case V150_1_SSE_RIC_V23_LOW_CHANNEL:
        res = "V.23 low channel";
        break;
    case V150_1_SSE_RIC_TONE_2225HZ:
        res = "2225Hz tone";
        break;
    case V150_1_SSE_RIC_V21_CH2_HDLC_FLAGS:
        res = "V.21 Ch2 HDLC flags";
        break;
    case V150_1_SSE_RIC_INDETERMINATE_SIGNAL:
        res = "Indeterminate signal";
        break;
    case V150_1_SSE_RIC_SILENCE:
        res = "Silence";
        break;
    case V150_1_SSE_RIC_CNG:
        res = "CNG";
        break;
    case V150_1_SSE_RIC_VOICE:
        res = "Voice";
        break;
    case V150_1_SSE_RIC_TIMEOUT:
        res = "Time-out";
        break;
    case V150_1_SSE_RIC_P_STATE_TRANSITION:
        res = "P' state transition";
        break;
    case V150_1_SSE_RIC_CLEARDOWN:
        res = "Cleardown";
        break;
    case V150_1_SSE_RIC_ANS_CED:
        res = "CED";
        break;
    case V150_1_SSE_RIC_ANSAM:
        res = "ANSam";
        break;
    case V150_1_SSE_RIC_ANS_PR:
        res = "/ANS";
        break;
    case V150_1_SSE_RIC_ANSAM_PR:
        res = "/ANSam";
        break;
    case V150_1_SSE_RIC_V92_QC1A:
        res = "V.92 QC1a";
        break;
    case V150_1_SSE_RIC_V92_QC1D:
        res = "V.92 QC1d";
        break;
    case V150_1_SSE_RIC_V92_QC2A:
        res = "V.92 QC2a";
        break;
    case V150_1_SSE_RIC_V92_QC2D:
        res = "V.92 QC2d";
        break;
    case V150_1_SSE_RIC_V8BIS_CRE:
        res = "V.8bis Cre";
        break;
    case V150_1_SSE_RIC_V8BIS_CRD:
        res = "V.8bis CRd";
        break;
    case V150_1_SSE_RIC_TIA825A_45_45BPS:
        res = "TIA825A 45.45BPS";
        break;
    case V150_1_SSE_RIC_TIA825A_50BPS:
        res = "TIA825A 50BPS";
        break;
    case V150_1_SSE_RIC_EDT:
        res = "EDT";
        break;
    case V150_1_SSE_RIC_BELL103:
        res = "Bell 103";
        break;
    case V150_1_SSE_RIC_V21_TEXT_TELEPHONE:
        res = "Text telephone";
        break;
    case V150_1_SSE_RIC_V23_MINITEL:
        res = "V.23 Minitel";
        break;
    case V150_1_SSE_RIC_V18_TEXT_TELEPHONE:
        res = "Text telephone";
        break;
    case V150_1_SSE_RIC_V18_DTMF_TEXT_RELAY:
        res = "Text relay";
        break;
    case V150_1_SSE_RIC_CTM:
        res = "CTM";
        break;
    }
    /*endswitch*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) v150_1_sse_timeout_reason_to_str(int ric)
{
    const char *res;

    res = "unknown";
    switch (ric)
    {
    case V150_1_SSE_RIC_INFO_TIMEOUT_NULL:
        res = "NULL";
        break;
    case V150_1_SSE_RIC_INFO_TIMEOUT_CALL_DISCRIMINATION_TIMEOUT:
        res = "Call discrimination timeout";
        break;
    case V150_1_SSE_RIC_INFO_TIMEOUT_IP_TLP:
        res = "IP-TLP";
        break;
    case V150_1_SSE_RIC_INFO_TIMEOUT_SSE_EXPLICIT_ACK_TIMEOUT:
        res = "TSSE explicit acknowledgement timeout";
        break;
    }
    /*endswitch*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) v150_1_sse_cleardown_reason_to_str(int ric)
{
    const char *res;

    res = "unknown";
    switch (ric)
    {
    case V150_1_SSE_RIC_INFO_CLEARDOWN_UNKNOWN:
        res = "Unknown/unspecified";
        break;
    case V150_1_SSE_RIC_INFO_CLEARDOWN_PHYSICAL_LAYER_RELEASE:
        res = "Physical Layer Release"; // (i.e. data pump release)";
        break;
    case V150_1_SSE_RIC_INFO_CLEARDOWN_LINK_LAYER_DISCONNECT:
        res = "Link Layer Disconnect"; // (i.e. receiving a V.42 DISC frame)";
        break;
    case V150_1_SSE_RIC_INFO_CLEARDOWN_COMPRESSION_DISCONNECT:
        res = "Data compression disconnect";
        break;
    case V150_1_SSE_RIC_INFO_CLEARDOWN_ABORT:
        res = "Abort"; // (i.e. termination due to Abort procedure as specified in SDL)";
        break;
    case V150_1_SSE_RIC_INFO_CLEARDOWN_ON_HOOK:
        res = "On-hook"; // (i.e. when gateway receives On-hook signal from an end-point device)";
        break;
    case V150_1_SSE_RIC_INFO_CLEARDOWN_NETWORK_LAYER_TERMINATION:
        res = "Network layer termination";
        break;
    case V150_1_SSE_RIC_INFO_CLEARDOWN_ADMINISTRATIVE:
        res = "Administrative"; // (i.e., operator action at gateway)";
        break;
    }
    /*endswitch*/
    return res;
}
/*- End of function --------------------------------------------------------*/

static int update_timer(v150_1_sse_state_t *s)
{
    span_timestamp_t shortest;
    uint8_t first;
    int i;
    int shortest_is;

    if (s->immediate_timer)
    {
        shortest = 1;
        shortest_is = 4;
    }
    else
    {
        /* Find the earliest expiring of the active timers, and set the timeout to that. */
        shortest = ~0;
        shortest_is = 0;

        if (s->timer_t0 < shortest)
        {
            shortest = s->timer_t0;
            shortest_is = 0;
        }
        /*endif*/
        if (s->timer_t0 < shortest)
        {
            shortest = s->timer_t1;
            shortest_is = 1;
        }
        /*endif*/
        /* If we haven't shrunk shortest from maximum, we have no timer to set, so we stop the timer,
           if its set. */
        if (shortest == ~0)
            shortest = 0;
        /*endif*/
    }
    /*endif*/
    span_log(&s->logging, SPAN_LOG_FLOW, "Update timer to %lu (%d)\n", shortest, shortest_is);
    if (s->timer_handler)
        s->timer_handler(s->timer_user_data, shortest);
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_sse_rx_initial_audio_packet(v150_1_sse_state_t *s, const uint8_t pkt[], int len)
{
    if (s->rmt_mode != V150_1_SSE_MEDIA_STATE_INITIAL_AUDIO)
    {
        /* Even if we don't support audio, C.5.3.2 says we need to make this our local state */
        s->lcl_mode = V150_1_SSE_MEDIA_STATE_INITIAL_AUDIO;
        s->rmt_mode = V150_1_SSE_MEDIA_STATE_INITIAL_AUDIO;
    }
    else
    {
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_sse_rx_voice_band_data_packet(v150_1_sse_state_t *s, const uint8_t pkt[], int len)
{
    if (s->rmt_mode != V150_1_SSE_MEDIA_STATE_VOICE_BAND_DATA)
    {
        /* Whether we change to VBD or plain audio is our choice. C.5.3.2. */
        //s->lcl_mode = V150_1_SSE_MEDIA_STATE_INITIAL_AUDIO;
        s->lcl_mode = V150_1_SSE_MEDIA_STATE_VOICE_BAND_DATA;
        s->rmt_mode = V150_1_SSE_MEDIA_STATE_VOICE_BAND_DATA;
    }
    else
    {
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_sse_rx_modem_relay_packet(v150_1_sse_state_t *s, const uint8_t pkt[], int len)
{
    int res;
    int ric;
    int ric_info;

    res = 0;
    ric = pkt[1];
    ric_info = get_net_unaligned_uint16(pkt + 2);
    span_log(&s->logging,
             SPAN_LOG_FLOW,
             "SSE force response %d, reason %s - %d\n",
             (pkt[0] >> 1) & 0x01,
             v150_1_sse_ric_to_str(ric),
             ric_info);
    if (s->rmt_mode != V150_1_SSE_MEDIA_STATE_MODEM_RELAY)
    {
        /* Whether we change to modem relay, VBD or plain audio is our choice. C.5.3.2. */
        //s->lcl_mode = V150_1_SSE_MEDIA_STATE_INITIAL_AUDIO;
        //s->lcl_mode = V150_1_SSE_MEDIA_STATE_VOICE_BAND_DATA;
        s->lcl_mode = V150_1_SSE_MEDIA_STATE_MODEM_RELAY;
        s->rmt_mode = V150_1_SSE_MEDIA_STATE_MODEM_RELAY;
    }
    else
    {
    }
    /*endif*/
    switch (ric)
    {
    case V150_1_SSE_RIC_V8_CM:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on V.8 detection\n");
        /* We need to respond with a P' */
        span_log(&s->logging, SPAN_LOG_FLOW, "Sending an SSE %s\n", v150_1_sse_ric_to_str(ric));
        v150_1_sse_tx_modem_relay_packet(s, 0, V150_1_SSE_RIC_P_STATE_TRANSITION, 0);
        if (s->status_handler)
            res = s->status_handler(s->status_user_data, 42);
        /*endif*/
        break;
    case V150_1_SSE_RIC_V8_JM:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on V.8 detection\n");
        break;
    case V150_1_SSE_RIC_V32BIS_AA:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on V.32bis detection\n");
        /* We need to respond with a P' */
        span_log(&s->logging, SPAN_LOG_FLOW, "Sending an SSE P'\n");
        v150_1_sse_tx_modem_relay_packet(s, 0, V150_1_SSE_RIC_P_STATE_TRANSITION, 0);
        if (s->status_handler)
            res = s->status_handler(s->status_user_data, 42);
        /*endif*/
        break;
    case V150_1_SSE_RIC_V32BIS_AC:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on V.32bis detection\n");
        break;
    case V150_1_SSE_RIC_V22BIS_USB1:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on V.22bis detection\n");
        break;
    case V150_1_SSE_RIC_V22BIS_SB1:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on V.22bis detection\n");
        break;
    case V150_1_SSE_RIC_V22BIS_S1:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on V.22bis detection\n");
        break;
    case V150_1_SSE_RIC_V21_CH2:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on V.21 detection\n");
        break;
    case V150_1_SSE_RIC_V21_CH1:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on V.21 detection\n");
        break;
    case V150_1_SSE_RIC_V23_HIGH_CHANNEL:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on V.23 detection\n");
        break;
    case V150_1_SSE_RIC_V23_LOW_CHANNEL:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on V.23 detection\n");
        break;
    case V150_1_SSE_RIC_TONE_2225HZ:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on 2225Hz tone detection\n");
        break;
    case V150_1_SSE_RIC_V21_CH2_HDLC_FLAGS:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on V.21 flags detection\n");
        break;
    case V150_1_SSE_RIC_INDETERMINATE_SIGNAL:
        break;
    case V150_1_SSE_RIC_SILENCE:
        break;
    case V150_1_SSE_RIC_CNG:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on CNG detection\n");
        break;
    case V150_1_SSE_RIC_VOICE:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on voice detection\n");
        break;
    case V150_1_SSE_RIC_TIMEOUT:
        span_log(&s->logging, SPAN_LOG_FLOW, "Timeout %d - %s\n", (ric_info >> 8), v150_1_sse_timeout_reason_to_str(ric_info >> 8));
        break;
    case V150_1_SSE_RIC_P_STATE_TRANSITION:
        span_log(&s->logging, SPAN_LOG_FLOW, "P' received\n");
        break;
    case V150_1_SSE_RIC_CLEARDOWN:
        span_log(&s->logging, SPAN_LOG_FLOW, "Cleardown %d - %s\n", (ric_info >> 8), v150_1_sse_cleardown_reason_to_str(ric_info >> 8));
        break;
    case V150_1_SSE_RIC_ANS_CED:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on ANS/CED detection\n");
        break;
    case V150_1_SSE_RIC_ANSAM:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on ANSam detection\n");
        break;
    case V150_1_SSE_RIC_ANS_PR:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on /ANS detection\n");
        break;
    case V150_1_SSE_RIC_ANSAM_PR:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on /ANSam detection\n");
        break;
    case V150_1_SSE_RIC_V92_QC1A:
        break;
    case V150_1_SSE_RIC_V92_QC1D:
        break;
    case V150_1_SSE_RIC_V92_QC2A:
        break;
    case V150_1_SSE_RIC_V92_QC2D:
        break;
    case V150_1_SSE_RIC_V8BIS_CRE:
        break;
    case V150_1_SSE_RIC_V8BIS_CRD:
        break;
    case V150_1_SSE_RIC_TIA825A_45_45BPS:
        break;
    case V150_1_SSE_RIC_TIA825A_50BPS:
        break;
    case V150_1_SSE_RIC_EDT:
        break;
    case V150_1_SSE_RIC_BELL103:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on Bell103 detection\n");
        break;
    case V150_1_SSE_RIC_V21_TEXT_TELEPHONE:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on V.21 text telephone detection\n");
        break;
    case V150_1_SSE_RIC_V23_MINITEL:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on V.21 minitel detection\n");
        break;
    case V150_1_SSE_RIC_V18_TEXT_TELEPHONE:
        break;
    case V150_1_SSE_RIC_V18_DTMF_TEXT_RELAY:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on DTMF text relay detection\n");
        break;
    case V150_1_SSE_RIC_CTM:
        break;
    }
    /*endswitch*/
    return res;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_sse_rx_fax_relay_packet(v150_1_sse_state_t *s, const uint8_t pkt[], int len)
{
    int res;
    int ric;
    int ric_info;

    res = 0;
    ric = pkt[1];
    ric_info = get_net_unaligned_uint16(pkt + 2);
    span_log(&s->logging,
             SPAN_LOG_FLOW,
             "SSE force %d, reason %s - %d\n",
             (pkt[0] >> 1) & 0x01,
             v150_1_sse_ric_to_str(ric),
             ric_info);
    if (s->rmt_mode != V150_1_SSE_MEDIA_STATE_FAX_RELAY)
    {
        /* Whether we change to FAX relay, VBD or plain audio is our choice. C.5.3.2. */
        //s->lcl_mode = V150_1_SSE_MEDIA_STATE_INITIAL_AUDIO;
        //s->lcl_mode = V150_1_SSE_MEDIA_STATE_VOICE_BAND_DATA;
        s->lcl_mode = V150_1_SSE_MEDIA_STATE_FAX_RELAY;
        s->rmt_mode = V150_1_SSE_MEDIA_STATE_FAX_RELAY;
    }
    else
    {
    }
    /*endif*/
    switch (ric)
    {
    case V150_1_SSE_RIC_V8_CM:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on V.8 detection\n");
        /* We need to respond with a P' */
        v150_1_sse_tx_fax_relay_packet(s, 0, V150_1_SSE_RIC_P_STATE_TRANSITION, 0);
        if (s->status_handler)
            res = s->status_handler(s->status_user_data, 42);
        /*endif*/
        break;
    case V150_1_SSE_RIC_V8_JM:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on V.8 detection\n");
        break;
    case V150_1_SSE_RIC_V32BIS_AA:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on V.32bis detection\n");
        /* We need to respond with a P' */
        v150_1_sse_tx_fax_relay_packet(s, 0, V150_1_SSE_RIC_P_STATE_TRANSITION, 0);
        if (s->status_handler)
            res = s->status_handler(s->status_user_data, 42);
        /*endif*/
        break;
    case V150_1_SSE_RIC_V32BIS_AC:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on V.32bis detection\n");
        break;
    case V150_1_SSE_RIC_V22BIS_USB1:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on V.22bis detection\n");
        break;
    case V150_1_SSE_RIC_V22BIS_SB1:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on V.22bis detection\n");
        break;
    case V150_1_SSE_RIC_V22BIS_S1:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on V.22bis detection\n");
        break;
    case V150_1_SSE_RIC_V21_CH2:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on V.21 detection\n");
        break;
    case V150_1_SSE_RIC_V21_CH1:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on V.21 detection\n");
        break;
    case V150_1_SSE_RIC_V23_HIGH_CHANNEL:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on V.23 detection\n");
        break;
    case V150_1_SSE_RIC_V23_LOW_CHANNEL:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on V.23 detection\n");
        break;
    case V150_1_SSE_RIC_TONE_2225HZ:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on 2225Hz tone detection\n");
        break;
    case V150_1_SSE_RIC_V21_CH2_HDLC_FLAGS:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on V.21 flags detection\n");
        break;
    case V150_1_SSE_RIC_INDETERMINATE_SIGNAL:
        break;
    case V150_1_SSE_RIC_SILENCE:
        break;
    case V150_1_SSE_RIC_CNG:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on CNG detection\n");
        break;
    case V150_1_SSE_RIC_VOICE:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on voice detection\n");
        break;
    case V150_1_SSE_RIC_TIMEOUT:
        span_log(&s->logging, SPAN_LOG_FLOW, "Timeout %d - %s\n", (ric_info >> 8), v150_1_sse_timeout_reason_to_str(ric_info >> 8));
        break;
    case V150_1_SSE_RIC_P_STATE_TRANSITION:
        span_log(&s->logging, SPAN_LOG_FLOW, "P' received\n");
        break;
    case V150_1_SSE_RIC_CLEARDOWN:
        span_log(&s->logging, SPAN_LOG_FLOW, "Cleardown %d - %s\n", (ric_info >> 8), v150_1_sse_cleardown_reason_to_str(ric_info >> 8));
        break;
    case V150_1_SSE_RIC_ANS_CED:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on ANS/CED detection\n");
        break;
    case V150_1_SSE_RIC_ANSAM:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on ANSam detection\n");
        break;
    case V150_1_SSE_RIC_ANS_PR:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on /ANS detection\n");
        break;
    case V150_1_SSE_RIC_ANSAM_PR:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on /ANSam detection\n");
        break;
    case V150_1_SSE_RIC_V92_QC1A:
        break;
    case V150_1_SSE_RIC_V92_QC1D:
        break;
    case V150_1_SSE_RIC_V92_QC2A:
        break;
    case V150_1_SSE_RIC_V92_QC2D:
        break;
    case V150_1_SSE_RIC_V8BIS_CRE:
        break;
    case V150_1_SSE_RIC_V8BIS_CRD:
        break;
    case V150_1_SSE_RIC_TIA825A_45_45BPS:
        break;
    case V150_1_SSE_RIC_TIA825A_50BPS:
        break;
    case V150_1_SSE_RIC_EDT:
        break;
    case V150_1_SSE_RIC_BELL103:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on Bell103 detection\n");
        break;
    case V150_1_SSE_RIC_V21_TEXT_TELEPHONE:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on V.21 text telephone detection\n");
        break;
    case V150_1_SSE_RIC_V23_MINITEL:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on V.23 minitel detection\n");
        break;
    case V150_1_SSE_RIC_V18_TEXT_TELEPHONE:
        break;
    case V150_1_SSE_RIC_V18_DTMF_TEXT_RELAY:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on DTMF text relay detection\n");
        break;
    case V150_1_SSE_RIC_CTM:
        break;
    }
    /*endswitch*/
    return res;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_sse_rx_text_relay_packet(v150_1_sse_state_t *s, const uint8_t pkt[], int len)
{
    if (s->rmt_mode != V150_1_SSE_MEDIA_STATE_TEXT_RELAY)
    {
        /* Whether we change to text relay, VBD or plain audio is our choice. C.5.3.2. */
        //s->lcl_mode = V150_1_SSE_MEDIA_STATE_INITIAL_AUDIO;
        //s->lcl_mode = V150_1_SSE_MEDIA_STATE_VOICE_BAND_DATA;
        s->lcl_mode = V150_1_SSE_MEDIA_STATE_TEXT_RELAY;
        s->rmt_mode = V150_1_SSE_MEDIA_STATE_TEXT_RELAY;
    }
    else
    {
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_sse_rx_text_probe_packet(v150_1_sse_state_t *s, const uint8_t pkt[], int len)
{
    if (s->rmt_mode != V150_1_SSE_MEDIA_STATE_TEXT_RELAY)
    {
        s->lcl_mode = V150_1_SSE_MEDIA_STATE_TEXT_RELAY;
        s->rmt_mode = V150_1_SSE_MEDIA_STATE_TEXT_RELAY;
    }
    else
    {
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_sse_rx_packet(v150_1_sse_state_t *s,
                                       uint16_t seq_no,
                                       uint32_t timestamp,
                                       const uint8_t pkt[],
                                       int len)
{
    int event;
    int res;
    int f;
    int x;
    int ext_len;

    span_log(&s->logging, SPAN_LOG_FLOW, "SSE rx message - %d bytes\n", len);
    if (len < 4)
        return -1;
    /*endif*/

    if (s->previous_rx_timestamp != timestamp)
    {
        /* V.150.1 C.4.1 says act on the first received copy of an SSE message. Expect
           the sequence number to increase, but the timestamp should remain the same for
           redundant repeats. */
        s->previous_rx_timestamp = timestamp;

        event = (pkt[0] >> 2) & 0x3F;
        f = (pkt[0] >> 1) & 0x01;
        x = pkt[0] & 0x01;
        span_log(&s->logging, SPAN_LOG_FLOW, "SSE event %s\n", v150_1_sse_media_state_to_str(event));
        if (x)
        {
            if (len >= 6)
            {
                /* Deal with the extension */
                ext_len = get_net_unaligned_uint16(&pkt[4]) & 0x7FF;
                if (ext_len >= 1)
                {
                    s->rmt_ack = pkt[6] & 0x3F;
                }
                /*endif*/
            }
            /*endif*/
        }
        else
        {
            if (len != 4)
                span_log(&s->logging, SPAN_LOG_FLOW, "Non-extended message of length %d\n", len);
            /*endif*/
        }
        /*endif*/
        switch (event)
        {
        case V150_1_SSE_MEDIA_STATE_INITIAL_AUDIO:
            res = v150_1_sse_rx_initial_audio_packet(s, pkt, len);
            break;
        case V150_1_SSE_MEDIA_STATE_VOICE_BAND_DATA:
            res = v150_1_sse_rx_voice_band_data_packet(s, pkt, len);
            break;
        case V150_1_SSE_MEDIA_STATE_MODEM_RELAY:
            res = v150_1_sse_rx_modem_relay_packet(s, pkt, len);
            break;
        case V150_1_SSE_MEDIA_STATE_FAX_RELAY:
            res = v150_1_sse_rx_fax_relay_packet(s, pkt, len);
            break;
        case V150_1_SSE_MEDIA_STATE_TEXT_RELAY:
            res = v150_1_sse_rx_text_relay_packet(s, pkt, len);
            break;
        case V150_1_SSE_MEDIA_STATE_TEXT_PROBE:
            res = v150_1_sse_rx_text_probe_packet(s, pkt, len);
            break;
        default:
            span_log(&s->logging, SPAN_LOG_FLOW, "Unexpected SSE event %d\n", event);
            res = -1;
            break;
        }
        /*endswitch*/
        s->rmt_mode = event;
    }
    /*endif*/
    return res;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_sse_tx_initial_audio_packet(v150_1_sse_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_sse_tx_voice_band_data_packet(v150_1_sse_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_sse_tx_modem_relay_packet(v150_1_sse_state_t *s, int x, int ric, int ricinfo)
{
    int res;
    uint8_t pkt[256];
    int len;
    uint8_t f;
    span_timestamp_t now;

    /* If we are using explicit acknowledgements, both the F and X bits need to be set */
    if (s->explicit_acknowledgements)
    {
        if (s->force_response)
            f = 0x03;
        else
            f = 0x01;
        /*endif*/
    }
    else
    {
        f = 0x00;
    }
    /*endif*/
    span_log(&s->logging, SPAN_LOG_FLOW, "Sending an SSE %s\n", v150_1_sse_ric_to_str(ric));
    pkt[0] = f | (V150_1_SSE_MEDIA_STATE_MODEM_RELAY << 2);
    pkt[1] = ric;
    put_net_unaligned_uint16(&pkt[2], ricinfo);
    len = 4;
    switch (ric)
    {
    case V150_1_SSE_RIC_CLEARDOWN:
        /* We may need to add more information as an extension. Note that V.150.1 originally made
           the SSE message lengths variable in a way that can't really work. The only message this
           affected was cleardown. Corrigendum 2 changed the extra bytes to an extension field, so
           all messages are 4 bytes long until the extension bit it used to stretch them. */
        break;
    }
    /*endswitch*/
    if (s->explicit_acknowledgements)
    {
        /* The length of the extension field */
        put_net_unaligned_uint16(&pkt[len], 1);
        len += 2;
        /* The actual content of the field */
        pkt[len++] = s->rmt_mode;
    }
    /*endif*/
    if (s->packet_handler)
        res = s->packet_handler(s->packet_user_data, pkt, len);
    /*endif*/
    if (s->explicit_acknowledgements)
    {
        if (s->timer_handler)
        {
            now = s->timer_handler(s->timer_user_data, ~0);
            s->timer_t0 = now + s->t0interval;
            s->timer_t1 = now + s->t1interval;
            s->counter_n0 = s->n0count;
            s->force_response = false;
            update_timer(s);
        }
        /*endif*/
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_sse_tx_fax_relay_packet(v150_1_sse_state_t *s, int x, int ric, int ricinfo)
{
    int res;
    uint8_t pkt[256];
    int len;
    uint8_t f;

    /* If we are using explicit acknowledgements, both the F and X bits need to be set */
    if (s->explicit_acknowledgements)
    {
        if (s->force_response)
            f = 0x03;
        else
            f = 0x01;
        /*endif*/
    }
    /*endif*/
    pkt[0] = f | (V150_1_SSE_MEDIA_STATE_FAX_RELAY << 2);
    pkt[1] = ric;
    put_net_unaligned_uint16(&pkt[2], ricinfo);
    len = 4;
    if (s->explicit_acknowledgements)
    {
        put_net_unaligned_uint16(&pkt[len], 1);
        len += 2;
        pkt[len++] = s->rmt_mode;
    }
    /*endif*/
    if (s->packet_handler)
        res = s->packet_handler(s->packet_user_data, pkt, len);
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_sse_tx_text_relay_packet(v150_1_sse_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_sse_tx_text_probe_packet(v150_1_sse_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_sse_tx_packet(v150_1_sse_state_t *s, int event, int ric, int ricinfo)
{
    int res;
    int x;

    x = 0;
    span_log(&s->logging, SPAN_LOG_FLOW, "SSE event %s\n", v150_1_sse_media_state_to_str(event));
    switch (event)
    {
    case V150_1_SSE_MEDIA_STATE_INITIAL_AUDIO:
        res = v150_1_sse_tx_initial_audio_packet(s);
        break;
    case V150_1_SSE_MEDIA_STATE_VOICE_BAND_DATA:
        res = v150_1_sse_tx_voice_band_data_packet(s);
        break;
    case V150_1_SSE_MEDIA_STATE_MODEM_RELAY:
        res = v150_1_sse_tx_modem_relay_packet(s, x, ric, ricinfo);
        break;
    case V150_1_SSE_MEDIA_STATE_FAX_RELAY:
        res = v150_1_sse_tx_fax_relay_packet(s, x, ric, ricinfo);
        break;
    case V150_1_SSE_MEDIA_STATE_TEXT_RELAY:
        res = v150_1_sse_tx_text_relay_packet(s);
        break;
    case V150_1_SSE_MEDIA_STATE_TEXT_PROBE:
        res = v150_1_sse_tx_text_probe_packet(s);
        break;
    default:
        span_log(&s->logging, SPAN_LOG_FLOW, "Unexpected SSE event %d\n", event);
        res = -1;
        break;
    }
    /*endswitch*/
    s->lcl_mode = event;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_sse_timer_expired(v150_1_sse_state_t *s, span_timestamp_t now)
{
    int i;

    span_log(&s->logging, SPAN_LOG_FLOW, "Timer expired at %lu\n", now);

    if (s->immediate_timer)
    {
        s->immediate_timer = false;
        /* TODO: */
    }
    /*endif*/
    if (s->timer_t0 != 0  &&  s->timer_t0 <= now)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "T0 expired\n");
        if (--s->counter_n0 > 0)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Resend (%d)\n", s->counter_n0);
            s->timer_t0 = now + s->t0interval;
            update_timer(s);
        }
        else
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Count exceeded\n");
        }
        /*endif*/
    }
    /*endif*/
    if (s->timer_t1 != 0  &&  s->timer_t1 <= now)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "T1 expired\n");
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_sse_explicit_acknowledgements(v150_1_sse_state_t *s,
                                                       bool explicit_acknowledgements)
{
    s->explicit_acknowledgements = explicit_acknowledgements;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) v150_1_sse_get_logging_state(v150_1_sse_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(v150_1_sse_state_t *) v150_1_sse_init(v150_1_sse_state_t *s,
                                                   v150_1_sse_packet_handler_t packet_handler,
                                                   void *packet_user_data,
                                                   v150_1_sse_status_handler_t status_handler,
                                                   void *status_user_data,
                                                   v150_1_sse_timer_handler_t timer_handler,
                                                   void *timer_user_data)
{
    if (packet_handler == NULL)
        return NULL;
    /*endif*/
    if (s == NULL)
    {
        if ((s = (v150_1_sse_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
        /*endif*/
    }
    /*endif*/
    memset(s, 0, sizeof(*s));

    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "V.150.1 SSE");

    s->reliability_method = V150_1_SSE_RELIABILITY_NONE;
    s->explicit_acknowledgements = false;
    s->repetitions = 3;
    s->repetition_interval = 20000;

    /* Set default values for the explicit acknowledgement parameters */
    /* V.150.1 C.4.3.1 */
    s->n0count = 3;
    s->t0interval = 10000;
    s->t1interval = 300000;

    /* V.150.1 C.4.3.1 */
    /* Let   p be the probability that a packet sent by one MoIP node through the packet
                  network will be successfully received by the other node.
       Let   t be the latency that can be tolerated in the delivery of mode updates
       Let   q be the reliability required in the delivery of mode updates within the given
                  latency
       Let rtd be the round trip delay through the packet network between the two nodes
       Let owd be the one way delay through the packet network from one node to the other
                  (i.e. rtd/2) */
    //s->n0count = floor(log(1 - q)/log(1 - p));
    //s->t0interval = max(0, ((rtd/2) - t)/(n0count - 1));
    //s->t1interval = 1.5*rtd;

    /* V.150.1 C.5.3 */
    s->lcl_mode = V150_1_SSE_MEDIA_STATE_INITIAL_AUDIO;
    s->rmt_mode = V150_1_SSE_MEDIA_STATE_INITIAL_AUDIO;

    s->previous_rx_timestamp = 0xFFFFFFFF;

    s->packet_handler = packet_handler;
    s->packet_user_data = packet_user_data;
    s->status_handler = status_handler;
    s->status_user_data = status_user_data;
    s->timer_handler = timer_handler;
    s->timer_user_data = timer_user_data;

    return s;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
