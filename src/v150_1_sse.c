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

/*
If the explicit acknowledgement procedure is being used for a call, the endpoints shall execute the
following procedures.

When an endpoint's MoIP application goes to a new mode, it:
    sends an SSE message containing the current value of the variables lcl_mode and rmt_mode
        to the other endpoint, with the must respond flag set to FALSE
    sets counter n0 to the value n0count
    sets timer t0 to t0interval (even if it was non-zero)
    sets timer t1 to t1interval (even if it was non-zero)



    if timer t0 decrements to 0
       and
       counter n0 is not equal to 0
       and
       the value of lcl_mode is not equal to the value of rmt_ack
    then
        The endpoint sends an SSE message to the other endpoint exactly as above except
            o counter n0 is decremented rather than set to n0count
            o timer t1 is not set
            o the must respond flag is set to TRUE if the value of timer t1 is zero.

NOTE - If timer t0 decrements to 0 and counter n0 is equal to zero, no action is taken until timer t1
       decrements to 0.



    if  timer t1 decrements to 0
        and
        counter n0 is equal to 0
        and
        the value of lcl_mode is not equal to the value of rmt_ack.
    then
        The endpoint sends an SSE message to the other endpoint exactly as first given above except
            o counter n0 is not decremented, it is left equal to zero
            o timer t0 is not set (It too is left equal to 0.)
            o the must respond flag is set to TRUE



Upon receipt of an SSE message from the other endpoint
    if the message is a duplicate or out of sequence (determined using the RTP header sequence number)
    then
        the endpoint ignores the received message
    else
        set the values of rmt_mode and rmt_ack to the values in the message
        if the message contained a new value for the remote endpoint's mode
        then
            or the message's must respond flag is set to TRUE
        then
            the endpoint sends an SSE message to the other endpoint exactly as first given above,
            except counter n0 and timers t0 and t1 are not (re)set.
*/

/*
               telephone network
                      ^
                      |
                      |
                      v
    +-----------------------------------+
    |                                   |
    |     Signal processing entity      |
    |                                   |
    +-----------------------------------+
                |           ^
                |           |
  Signal list 1 |           | Signal list 2
                |           |
                v           |
    +-----------------------------------+   Signal list 5   +-----------------------------------+
    |                                   | ----------------->|                                   |
    |   SSE protocol state machine (P)  |                   |    Gateway state machine (s,s')   |
    |                                   |<------------------|                                   |
    +-----------------------------------+   Signal list 6   +-----------------------------------+
                |           ^
                |           |
  Signal list 3 |           | Signal list 4
                |           |
                v           |
    +-----------------------------------+
    |                                   |
    |       IP network processor        |
    |                                   |
    +-----------------------------------+
                      ^
                      |
                      |
                      v
                 IP network
*/

#if 0

int generic_macro(int local, int remote, int cause)
{
}

    switch (s->lcl_mode)
    {
    case V150_1_SSE_SIGNAL_A_STATE:
        switch (s->rmt_mode)
        {
        case V150_1_SSE_SIGNAL_A_STATE:
            /* Figure 26/V.150.1 to 31/V.150.1 */
            switch (signal)
            {
            case V150_1_SSE_SIGNAL_2100HZ:
                if (s->vbd_available  &&  s->vbd_preferred)
                {
                    s->local_mode = V150_1_SSE_SIGNAL_V_STATE;
                    generic_macro(s->local_mode, s->rmt_mode, signal);
                    send ANS or ANSam
                }
                else
                {
                    block_tone();
                }
                /*endif*/
                break;
            case V150_1_SSE_SIGNAL_ANS:
                if (s->vbd_available  &&  s->vbd_preferred)
                {
                    s->local_mode = V150_1_SSE_SIGNAL_V_STATE;
                    generic_macro(s->local_mode, s->rmt_mode, signal);
                    send ANS
                }
                else
                {
                    send RFC4733_ANS
                    conceal_modem();
                }
                /*endif*/
                break;
            case V150_1_SSE_SIGNAL_ANSAM:
                if (s->vbd_available  &&  s->vbd_preferred)
                {
                    s->local_mode = V150_1_SSE_SIGNAL_V_STATE;
                    generic_macro(s->local_mode, s->rmt_mode, signal);
                    send ANS
                }
                else
                {
                    send RFC4733_ANSAM
                    conceal_modem();
                }
                /*endif*/
                break;
            case V150_1_SSE_SIGNAL_RFC4733_ANS:
            case V150_1_SSE_SIGNAL_RFC4733_ANSAM:
                send ANS or ANSAM
                conceal_modem();
                break;
            case V150_1_SSE_SIGNAL_RFC4733_ANS_PR:
            case V150_1_SSE_SIGNAL_RFC4733_ANSAM_PR:
                send /ANS or /ANSAM
                conceal_modem();
                break;
            case V150_1_SSE_SIGNAL_ANS:
            case V150_1_SSE_SIGNAL_ANSAM:
                break;
            case V150_1_SSE_SIGNAL_ANS_PR:
            case V150_1_SSE_SIGNAL_ANSAM_PR:
                break;
            case V150_1_SSE_SIGNAL_UNKNOWN:
            case V150_1_SSE_SIGNAL_CALL_DISCRIMINATION_TIMEOUT:
                if (s->vbd_preferred)
                {
                    s->local_mode = V150_1_SSE_SIGNAL_V_STATE;
                    generic_macro(s->local_mode, s->rmt_mode, signal);
                }
                /*endif*/
                break;
            case V150_1_SSE_SIGNAL_V:
                if (s->vbd_preferred)
                {
                    s->remote_mode = V150_1_SSE_SIGNAL_V_STATE;
                    s->local_mode = V150_1_SSE_SIGNAL_V_STATE;
                    generic_macro(s->local_mode, s->rmt_mode, signal);
                }
                else
                {
                    s->local_mode = V150_1_SSE_SIGNAL_V_STATE;
                    generic_macro(s->local_mode, s->rmt_mode, signal);
                }
                /*endif*/
                break;
            case V150_1_SSE_SIGNAL_M:
                break;
            }
            /*endswitch*/
            break;
        case V150_1_SSE_SIGNAL_F_STATE:
            break;
        case V150_1_SSE_SIGNAL_I_STATE:
            break;
        case V150_1_SSE_SIGNAL_M_STATE:
            /* Figure 32/V.150.1 */
            switch (signal)
            {
            case V150_1_SSE_SIGNAL_A:
                break;
            }
            /*endswitch*/
            break;
        case V150_1_SSE_SIGNAL_T_STATE:
            break;
        case V150_1_SSE_SIGNAL_V_STATE:
            /* Figure 33/V.150.1 */
            switch (signal)
            {
            case V150_1_SSE_SIGNAL_A:
                break;
            }
            /*endswitch*/
            break;
        }
        /*endswitch*/
        break;
    case V150_1_SSE_SIGNAL_F_STATE:
        switch (s->rmt_mode)
        {
        case V150_1_SSE_SIGNAL_A_STATE:
            break;
        case V150_1_SSE_SIGNAL_F_STATE:
            break;
        case V150_1_SSE_SIGNAL_I_STATE:
            break;
        case V150_1_SSE_SIGNAL_M_STATE:
            break;
        case V150_1_SSE_SIGNAL_T_STATE:
            break;
        case V150_1_SSE_SIGNAL_V_STATE:
            break;
        }
        /*endswitch*/
        break;
    case V150_1_SSE_SIGNAL_I_STATE:
        switch (s->rmt_mode)
        {
        case V150_1_SSE_SIGNAL_A_STATE:
            break;
        case V150_1_SSE_SIGNAL_F_STATE:
            break;
        case V150_1_SSE_SIGNAL_I_STATE:
            break;
        case V150_1_SSE_SIGNAL_M_STATE:
            break;
        case V150_1_SSE_SIGNAL_T_STATE:
            break;
        case V150_1_SSE_SIGNAL_V_STATE:
            break;
        }
        /*endswitch*/
        break;
    case V150_1_SSE_SIGNAL_M_STATE:
        switch (s->rmt_mode)
        {
        case V150_1_SSE_SIGNAL_A_STATE:
            /* Figure 34/V.150.1 */
            switch (signal)
            {
            case V150_1_SSE_SIGNAL_A:
                break;
            case V150_1_SSE_SIGNAL_M:
                break;
            case V150_1_SSE_SIGNAL_V:
                break;
            }
            /*endswitch*/
            break;
        case V150_1_SSE_SIGNAL_F_STATE:
            break;
        case V150_1_SSE_SIGNAL_I_STATE:
            break;
        case V150_1_SSE_SIGNAL_M_STATE:
            /* Figure 35/V.150.1 */
            switch (signal)
            {
            case V150_1_SSE_SIGNAL_JM:
                break;
            case V150_1_SSE_SIGNAL_V:
                break;
            }
            /*endswitch*/
            break;
        case V150_1_SSE_SIGNAL_T_STATE:
            break;
        case V150_1_SSE_SIGNAL_V_STATE:
            /* Figure 36/V.150.1 */
            switch (signal)
            {
            case V150_1_SSE_SIGNAL_A:
                break;
            case V150_1_SSE_SIGNAL_M:
                break;
            case V150_1_SSE_SIGNAL_V:
                break;
            }
            /*endswitch*/
            break;
        }
        /*endswitch*/
        break;
    case V150_1_SSE_SIGNAL_T_STATE:
        switch (s->rmt_mode)
        {
        case V150_1_SSE_SIGNAL_A_STATE:
            break;
        case V150_1_SSE_SIGNAL_F_STATE:
            break;
        case V150_1_SSE_SIGNAL_I_STATE:
            break;
        case V150_1_SSE_SIGNAL_M_STATE:
            break;
        case V150_1_SSE_SIGNAL_T_STATE:
            break;
        case V150_1_SSE_SIGNAL_V_STATE:
            break;
        }
        /*endswitch*/
        break;
    case V150_1_SSE_SIGNAL_V_STATE:
        switch (s->rmt_mode)
        {
        case V150_1_SSE_SIGNAL_A_STATE:
            /* Figure 37/V.150.1 */
            switch (signal)
            {
            case V150_1_SSE_SIGNAL_A:
                break;
            case V150_1_SSE_SIGNAL_M:
                break;
            case V150_1_SSE_SIGNAL_V:
                break;
            }
            /*endswitch*/
            break;
        case V150_1_SSE_SIGNAL_F_STATE:
            break;
        case V150_1_SSE_SIGNAL_I_STATE:
            break;
        case V150_1_SSE_SIGNAL_M_STATE:
            /* Figure 38/V.150.1 */
            switch (signal)
            {
            case V150_1_SSE_SIGNAL_A:
                break;
            case V150_1_SSE_SIGNAL_V:
                break;
            }
            /*endswitch*/
            break;
        case V150_1_SSE_SIGNAL_T_STATE:
            break;
        case V150_1_SSE_SIGNAL_V_STATE:
            /* Figure 39/V.150.1 */
            switch (signal)
            {
            case V150_1_SSE_SIGNAL_M:
                break;
            case V150_1_SSE_SIGNAL_CM:
                break;
            case V150_1_SSE_SIGNAL_RFC4733_ANS_PR:
            case V150_1_SSE_SIGNAL_RFC4733_ANSAM_PR:
                break;
            case V150_1_SSE_SIGNAL_ANS_PR:
            case V150_1_SSE_SIGNAL_ANSAM_PR:
                break;
            case V150_1_SSE_SIGNAL_RFC4733_ANS:
            case V150_1_SSE_SIGNAL_RFC4733_ANSAM:
                break;
            }
            /*endswitch*/
            break;
        }
        /*endswitch*/
        break;
    }
    /*endswitch*/
#endif

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

        if (s->ack_timer_t0  &&  s->ack_timer_t0 < shortest)
        {
            shortest = s->ack_timer_t0;
            shortest_is = 0;
        }
        /*endif*/
        if (s->ack_timer_t1  &&  s->ack_timer_t1 < shortest)
        {
            shortest = s->ack_timer_t1;
            shortest_is = 1;
        }
        /*endif*/
        if (s->repetition_timer  &&  s->repetition_timer < shortest)
        {
            shortest = s->repetition_timer;
            shortest_is = 2;
        }
        /*endif*/
        if (s->recovery_timer_t1  &&  s->recovery_timer_t1 < shortest)
        {
            shortest = s->recovery_timer_t1;
            shortest_is = 3;
        }
        /*endif*/
        if (s->recovery_timer_t2  &&  s->recovery_timer_t2 < shortest)
        {
            shortest = s->recovery_timer_t2;
            shortest_is = 4;
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
    s->latest_timer = shortest;
    if (s->timer_handler)
        s->timer_handler(s->timer_user_data, s->latest_timer);
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void log_v8_ric_info(v150_1_sse_state_t *s, int ric_info)
{
    if ((ric_info & V150_1_SSE_RIC_INFO_V8_CM_PCM_MODE))
        span_log(&s->logging, SPAN_LOG_FLOW, "    PCM mode\n");
    /*endif*/
    if ((ric_info & V150_1_SSE_RIC_INFO_V8_CM_V34_DUPLEX))
        span_log(&s->logging, SPAN_LOG_FLOW, "    V.34 duplex\n");
    /*endif*/
    if ((ric_info & V150_1_SSE_RIC_INFO_V8_CM_V34_HALF_DUPLEX))
        span_log(&s->logging, SPAN_LOG_FLOW, "    V.34 half duplex\n");
    /*endif*/
    if ((ric_info & V150_1_SSE_RIC_INFO_V8_CM_V32BIS))
        span_log(&s->logging, SPAN_LOG_FLOW, "    V.32/V32.bis\n");
    /*endif*/
    if ((ric_info & V150_1_SSE_RIC_INFO_V8_CM_V22BIS))
        span_log(&s->logging, SPAN_LOG_FLOW, "    V.22/V22.bis\n");
    /*endif*/
    if ((ric_info & V150_1_SSE_RIC_INFO_V8_CM_V17))
        span_log(&s->logging, SPAN_LOG_FLOW, "    V.17\n");
    /*endif*/
    if ((ric_info & V150_1_SSE_RIC_INFO_V8_CM_V29))
        span_log(&s->logging, SPAN_LOG_FLOW, "    V.29 half-duplex\n");
    /*endif*/
    if ((ric_info & V150_1_SSE_RIC_INFO_V8_CM_V27TER))
        span_log(&s->logging, SPAN_LOG_FLOW, "    V.27ter\n");
    /*endif*/
    if ((ric_info & V150_1_SSE_RIC_INFO_V8_CM_V26TER))
        span_log(&s->logging, SPAN_LOG_FLOW, "    V.26ter\n");
    /*endif*/
    if ((ric_info & V150_1_SSE_RIC_INFO_V8_CM_V26BIS))
        span_log(&s->logging, SPAN_LOG_FLOW, "    V.26bis\n");
    /*endif*/
    if ((ric_info & V150_1_SSE_RIC_INFO_V8_CM_V23_DUPLEX))
        span_log(&s->logging, SPAN_LOG_FLOW, "    V.23 duplex\n");
    /*endif*/
    if ((ric_info & V150_1_SSE_RIC_INFO_V8_CM_V23_HALF_DUPLEX))
        span_log(&s->logging, SPAN_LOG_FLOW, "    V.23 half-duplex\n");
    /*endif*/
    if ((ric_info & V150_1_SSE_RIC_INFO_V8_CM_V21))
        span_log(&s->logging, SPAN_LOG_FLOW, "    V.21\n");
    /*endif*/
    if ((ric_info & V150_1_SSE_RIC_INFO_V8_CM_V90_V92_ANALOGUE))
        span_log(&s->logging, SPAN_LOG_FLOW, "    V.90/V.92 analogue\n");
    /*endif*/
    if ((ric_info & V150_1_SSE_RIC_INFO_V8_CM_V90_V92_DIGITAL))
        span_log(&s->logging, SPAN_LOG_FLOW, "    V.90/V.92 digital\n");
    /*endif*/
    if ((ric_info & V150_1_SSE_RIC_INFO_V8_CM_V91))
        span_log(&s->logging, SPAN_LOG_FLOW, "    V.91\n");
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static int rx_initial_audio_packet(v150_1_sse_state_t *s, const uint8_t pkt[], int len)
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

static int rx_voice_band_data_packet(v150_1_sse_state_t *s, const uint8_t pkt[], int len)
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

static int rx_modem_relay_packet(v150_1_sse_state_t *s, const uint8_t pkt[], int len)
{
    int res;
    int ric;
    int ric_info;

    res = 0;
    ric = pkt[1];
    ric_info = get_net_unaligned_uint16(pkt + 2);
    span_log(&s->logging,
             SPAN_LOG_FLOW,
             "%sReason %s - 0x%x\n",
             ((pkt[0] >> 1) & 0x01)  ?  "Force response. "  :  "",
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
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on V.8 (CM) detection\n");
        log_v8_ric_info(s, ric_info);
        /* We need to respond with a P' */
        v150_1_sse_tx_modem_relay_packet(s, 0, V150_1_SSE_RIC_P_STATE_TRANSITION, 0);
        if (s->status_handler)
            res = s->status_handler(s->status_user_data, V150_1_SSE_STATUS_V8_CM_RECEIVED);
        /*endif*/
        break;
    case V150_1_SSE_RIC_V8_JM:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on V.8 (JM) detection\n");
        log_v8_ric_info(s, ric_info);
        if (s->status_handler)
            res = s->status_handler(s->status_user_data, V150_1_SSE_STATUS_V8_JM_RECEIVED);
        /*endif*/
        break;
    case V150_1_SSE_RIC_V32BIS_AA:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switch on V.32bis detection\n");
        /* We need to respond with a P' */
        v150_1_sse_tx_modem_relay_packet(s, 0, V150_1_SSE_RIC_P_STATE_TRANSITION, 0);
        if (s->status_handler)
            res = s->status_handler(s->status_user_data, V150_1_SSE_STATUS_AA_RECEIVED);
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
        span_log(&s->logging,
                 SPAN_LOG_FLOW,
                 "Timeout %d - %s - 0x%x\n",
                 (ric_info >> 8),
                 v150_1_sse_timeout_reason_to_str(ric_info >> 8),
                 ric_info & 0xFF);
        break;
    case V150_1_SSE_RIC_P_STATE_TRANSITION:
        span_log(&s->logging, SPAN_LOG_FLOW, "P' received\n");
        break;
    case V150_1_SSE_RIC_CLEARDOWN:
        span_log(&s->logging,
                 SPAN_LOG_FLOW,
                 "Cleardown %d - %s\n",
                 (ric_info >> 8),
                 v150_1_sse_cleardown_reason_to_str(ric_info >> 8));
        if (s->status_handler)
            res = s->status_handler(s->status_user_data, V150_1_SSE_STATUS_CLEARDOWN);
        /*endif*/
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

static int rx_fax_relay_packet(v150_1_sse_state_t *s, const uint8_t pkt[], int len)
{
    int res;
    int ric;
    int ric_info;

    res = 0;
    ric = pkt[1];
    ric_info = get_net_unaligned_uint16(pkt + 2);
    span_log(&s->logging,
             SPAN_LOG_FLOW,
             "%sReason %s - 0x%x\n",
             ((pkt[0] >> 1) & 0x01)  ?  "Force response. "  :  "",
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
            res = s->status_handler(s->status_user_data, V150_1_SSE_STATUS_V8_CM_RECEIVED_FAX);
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
            res = s->status_handler(s->status_user_data, V150_1_SSE_STATUS_AA_RECEIVED_FAX);
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
        span_log(&s->logging,
                 SPAN_LOG_FLOW,
                 "Timeout %d - %s - 0x%x\n",
                 (ric_info >> 8),
                 v150_1_sse_timeout_reason_to_str(ric_info >> 8),
                 ric_info & 0xFF);
        break;
    case V150_1_SSE_RIC_P_STATE_TRANSITION:
        span_log(&s->logging, SPAN_LOG_FLOW, "P' received\n");
        break;
    case V150_1_SSE_RIC_CLEARDOWN:
        span_log(&s->logging,
                 SPAN_LOG_FLOW,
                 "Cleardown %d - %s\n",
                 (ric_info >> 8),
                 v150_1_sse_cleardown_reason_to_str(ric_info >> 8));
        if (s->status_handler)
            res = s->status_handler(s->status_user_data, V150_1_SSE_STATUS_CLEARDOWN);
        /*endif*/
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

static int rx_text_relay_packet(v150_1_sse_state_t *s, const uint8_t pkt[], int len)
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

static int rx_text_probe_packet(v150_1_sse_state_t *s, const uint8_t pkt[], int len)
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

    span_log(&s->logging, SPAN_LOG_FLOW, "Rx message - %d bytes\n", len);
    if (len < 4)
        return -1;
    /*endif*/

    /*
        Upon receipt of an SSE message from the other endpoint
        
        if the message is a duplicate or out of sequence (determined using the RTP header sequence number)
        then
            the endpoint ignores the received message
        else
            set the values of rmt_mode and rmt_ack to the values in the message
            if the message contained a new value for the remote endpoint's mode
               or
               the message's must respond flag is set to TRUE
            then
                The endpoint sends an SSE message to the other endpoint exactly as first given above except
                counter n0 and timers t0 and t1 are not (re)set.
    */

    res = 0;
    if (s->previous_rx_timestamp != timestamp)
    {
        /* V.150.1 C.4.1 says act on the first received copy of an SSE message. Expect
           the sequence number to increase, but the timestamp should remain the same for
           redundant repeats. */
        s->previous_rx_timestamp = timestamp;

        event = (pkt[0] >> 2) & 0x3F;
        f = (pkt[0] >> 1) & 0x01;
        x = pkt[0] & 0x01;
        span_log(&s->logging, SPAN_LOG_FLOW, "Rx event %s\n", v150_1_sse_media_state_to_str(event));
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
            res = rx_initial_audio_packet(s, pkt, len);
            break;
        case V150_1_SSE_MEDIA_STATE_VOICE_BAND_DATA:
            res = rx_voice_band_data_packet(s, pkt, len);
            break;
        case V150_1_SSE_MEDIA_STATE_MODEM_RELAY:
            res = rx_modem_relay_packet(s, pkt, len);
            break;
        case V150_1_SSE_MEDIA_STATE_FAX_RELAY:
            res = rx_fax_relay_packet(s, pkt, len);
            break;
        case V150_1_SSE_MEDIA_STATE_TEXT_RELAY:
            res = rx_text_relay_packet(s, pkt, len);
            break;
        case V150_1_SSE_MEDIA_STATE_TEXT_PROBE:
            res = rx_text_probe_packet(s, pkt, len);
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

static int send_packet(v150_1_sse_state_t *s, uint8_t *pkt, int len)
{
    span_timestamp_t now;

    if (s->tx_packet_handler)
        s->tx_packet_handler(s->tx_packet_user_data, false, pkt, len);
    /*endif*/
    switch (s->reliability_method)
    {
    case V150_1_SSE_RELIABILITY_BY_REPETITION:
        if (s->timer_handler)
        {
            memcpy(s->last_tx_pkt, pkt, len);
            s->last_tx_len = len;
            now = s->timer_handler(s->timer_user_data, ~0);
            s->repetition_timer = now + s->repetition_interval;
            s->repetition_counter = s->repetition_count;
            update_timer(s);
        }
        /*endif*/
        break;
    case V150_1_SSE_RELIABILITY_BY_EXPLICIT_ACK:
        if (s->timer_handler)
        {
            /* V.150.1/C.4.3.2 */
            /* Save a copy of the message for retransmission */
            /* TODO: add lcl_mode and rmt_mode to the message */
            memcpy(s->last_tx_pkt, pkt, len);
            s->last_tx_len = len;
            now = s->timer_handler(s->timer_user_data, ~0);
            s->ack_counter_n0 = s->ack_n0count;
            s->ack_timer_t0 = now + s->ack_t0interval;
            s->ack_timer_t1 = now + s->ack_t1interval;
            s->force_response = false;
            update_timer(s);
        }
        /*endif*/
        break;
    }
    /*endswitch*/
    return 0;
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
    uint8_t pkt[256];
    int len;
    uint8_t f;

    f = 0;
    /* If we are using explicit acknowledgements, both the F and X bits need to be set */
    if (s->reliability_method == V150_1_SSE_RELIABILITY_BY_EXPLICIT_ACK)
    {
        f |= 0x01;
        if (s->force_response)
            f |= 0x02;
        /*endif*/
    }
    /*endif*/
    span_log(&s->logging, SPAN_LOG_FLOW, "Sending %s\n", v150_1_sse_ric_to_str(ric));
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
    if (s->reliability_method == V150_1_SSE_RELIABILITY_BY_EXPLICIT_ACK)
    {
        /* The length of the extension field */
        put_net_unaligned_uint16(&pkt[len], 1);
        len += 2;
        /* The actual content of the field */
        pkt[len++] = s->rmt_mode;
    }
    /*endif*/
    send_packet(s, pkt, len);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_sse_tx_fax_relay_packet(v150_1_sse_state_t *s, int x, int ric, int ricinfo)
{
    int res;
    uint8_t pkt[256];
    int len;
    uint8_t f;

    f = 0;
    /* If we are using explicit acknowledgements, both the F and X bits need to be set */
    if (s->reliability_method == V150_1_SSE_RELIABILITY_BY_EXPLICIT_ACK)
    {
        f |= 0x01;
        if (s->force_response)
            f |= 0x02;
        /*endif*/
    }
    /*endif*/
    pkt[0] = (V150_1_SSE_MEDIA_STATE_FAX_RELAY << 2) | f;
    pkt[1] = ric;
    put_net_unaligned_uint16(&pkt[2], ricinfo);
    len = 4;
    if (s->reliability_method == V150_1_SSE_RELIABILITY_BY_EXPLICIT_ACK)
    {
        put_net_unaligned_uint16(&pkt[len], 1);
        len += 2;
        pkt[len++] = s->rmt_mode;
    }
    /*endif*/
    send_packet(s, pkt, len);
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
    span_log(&s->logging, SPAN_LOG_FLOW, "Tx event %s\n", v150_1_sse_media_state_to_str(event));
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
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_sse_timer_expired(v150_1_sse_state_t *s, span_timestamp_t now)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Timer expired at %lu\n", now);

    if (now < s->latest_timer)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Timer returned %luus early\n", s->latest_timer - now);
        /* Request the same timeout point again. */
        if (s->timer_handler)
            s->timer_handler(s->timer_user_data, s->latest_timer);
        /*endif*/
        return 0;
    }
    /*endif*/

    if (s->immediate_timer)
    {
        s->immediate_timer = false;
        /* TODO: */
    }
    /*endif*/
    if (s->ack_timer_t0 != 0  &&  s->ack_timer_t0 <= now)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "T0 expired\n");

        /* V.150.1/C.4.3.2 */
        if (s->ack_counter_n0 > 0  &&  s->lcl_mode != s->rmt_ack)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Resend (%d)\n", s->ack_counter_n0);
            /* TODO: The must respond flag is set to TRUE if the value of timer t1 is zero. */
            if (s->tx_packet_handler)
                s->tx_packet_handler(s->tx_packet_user_data, true, s->last_tx_pkt, s->last_tx_len);
            /*endif*/
            s->ack_counter_n0--;
            s->ack_timer_t0 = now + s->ack_t0interval;
            /* T1 is not touched at this time */
            update_timer(s);
        }
        /*endif*/
    }
    /*endif*/
    if (s->ack_timer_t1 != 0  &&  s->ack_timer_t1 <= now)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "T1 expired\n");

        /* V.150.1/C.4.3.2 */
        if (s->ack_counter_n0 == 0  &&  s->lcl_mode != s->rmt_ack)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Resend (%d)\n", s->ack_counter_n0);
            /* TODO: The must respond flag is set to TRUE */
            if (s->tx_packet_handler)
                s->tx_packet_handler(s->tx_packet_user_data, true, s->last_tx_pkt, s->last_tx_len);
            /*endif*/
            /* counter N0 is not touched at this time */
            /* T0 is not touched at this time */
            s->ack_timer_t1 = now + s->ack_t1interval;
            update_timer(s);
        }
        /*endif*/
    }
    /*endif*/
    if (s->repetition_timer != 0  &&  s->repetition_timer <= now)
    {
        /* Handle reliability by simple repetition timer */
        span_log(&s->logging, SPAN_LOG_FLOW, "Repetition timer expired\n");
        if (s->repetition_counter > 1)
        {
            s->repetition_timer += s->repetition_interval;
            update_timer(s);
        }
        else
        {
            s->repetition_timer  = 0;
        }
        /*endif*/
        --s->repetition_counter;
        if (s->tx_packet_handler)
            s->tx_packet_handler(s->tx_packet_user_data, true, s->last_tx_pkt, s->last_tx_len);
        /*endif*/
    }
    /*endif*/
    if (s->recovery_timer_t1 != 0  &&  s->recovery_timer_t1 <= now)
    {
    }
    /*endif*/
    if (s->recovery_timer_t2 != 0  &&  s->recovery_timer_t2 <= now)
    {
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_sse_set_reliability_method(v150_1_sse_state_t *s,
                                                    enum v150_1_sse_reliability_option_e method,
                                                    int parm1,
                                                    int parm2,
                                                    int parm3)
{
    /* Select one of the reliability methods from V.150.1 C.4 */
    switch (method)
    {
    case V150_1_SSE_RELIABILITY_NONE:
        break;
    case V150_1_SSE_RELIABILITY_BY_REPETITION:
        if (parm1 < 2  || parm1 > 10)
            return -1;
        /*endif*/
        if (parm2 < 10000  ||  parm2 > 1000000)
            return -1;
        /*endif*/
        /* The actual number of repeats is one less than the total number of
           transmissions */
        s->repetition_count = parm1 - 1;
        s->repetition_interval = parm2;
        break;
    case V150_1_SSE_RELIABILITY_BY_RFC2198:
        break;
    case V150_1_SSE_RELIABILITY_BY_EXPLICIT_ACK:
        if (parm1 < 2  || parm1 > 10)
            return -1;
        /*endif*/
        if (parm2 < 10000  ||  parm2 > 1000000)
            return -1;
        /*endif*/
        if (parm3 < 10000  ||  parm3 > 1000000)
            return -1;
        /*endif*/
        s->ack_n0count = parm1;
        s->ack_t0interval = parm2;
        s->ack_t1interval = parm3;
        break;
    default:
        return -1;
    }
    /*endswitch*/
    s->reliability_method = method;
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
    /* Set default values for the reliability by redundancy parameters */
    /* V.150.1 C.4.1 */
    /* The actual number of repeats is one less than the total number of
       transmissions */
    s->repetition_count = V150_1_SSE_DEFAULT_REPETITIONS - 1;
    s->repetition_interval = V150_1_SSE_DEFAULT_REPETITION_INTERVAL;

    /* Set default values for the explicit acknowledgement parameters */
    /* V.150.1 C.4.3.1 */
    s->ack_n0count = V150_1_SSE_DEFAULT_ACK_N0;
    s->ack_t0interval = V150_1_SSE_DEFAULT_ACK_T0;
    s->ack_t1interval = V150_1_SSE_DEFAULT_ACK_T1;

    s->recovery_n = V150_1_SSE_DEFAULT_RECOVERY_N;
    s->recovery_t1 = V150_1_SSE_DEFAULT_RECOVERY_T1;
    s->recovery_t2 = V150_1_SSE_DEFAULT_RECOVERY_T2;

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

    s->explicit_ack_enabled = false;

    /* V.150.1 C.5.3 */
    s->lcl_mode = V150_1_SSE_MEDIA_STATE_INITIAL_AUDIO;
    s->rmt_mode = V150_1_SSE_MEDIA_STATE_INITIAL_AUDIO;

    s->previous_rx_timestamp = 0xFFFFFFFF;

    s->tx_packet_handler = packet_handler;
    s->tx_packet_user_data = packet_user_data;
    s->status_handler = status_handler;
    s->status_user_data = status_user_data;
    s->timer_handler = timer_handler;
    s->timer_user_data = timer_user_data;

    return s;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
