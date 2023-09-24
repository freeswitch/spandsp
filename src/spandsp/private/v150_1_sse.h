/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/v150_1_sse.h - An implementation of the SSE protocol defined in V.150.1
 *                        Annex C, less the packet exchange part
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

#if !defined(_SPANDSP_PRIVATE_V150_1_SSE_H_)
#define _SPANDSP_PRIVATE_V150_1_SSE_H_

enum v150_1_sse_signal_e
{
    /* Signal list 1 */

    /* SPE has detected 2100Hz tone for a duration less than 50ms */
    V150_1_SSE_SIGNAL_TONE_2100HZ = 1,
    /* SPE has detected 2225Hz tone for a duration less than 50ms */
    V150_1_SSE_SIGNAL_TONE_2225HZ,
    /* SPE has verified presence of V.25 ANS type answer tone */
    V150_1_SSE_SIGNAL_ANS,
    /* SPE has detected a 180-degree phase reversal in a verified ANS type answer tone */
    V150_1_SSE_SIGNAL_ANS_PR,
    /* SPE has verified presence of V.8 ANSam type answer tone */
    V150_1_SSE_SIGNAL_ANSAM,
    /* SPE has detected a 180-degree phase reversal in a verified ANSam type answer tone */
    V150_1_SSE_SIGNAL_ANSAM_PR,
    /* SPE has detected a V.8 CI signal */
    V150_1_SSE_SIGNAL_CI,
    /* SPE has detected a V.8 CM signal */
    V150_1_SSE_SIGNAL_CM,
    /* SPE has detected a V.8 JM signal */
    V150_1_SSE_SIGNAL_JM,
    /* SPE has detected a V.21 low channel signal */
    V150_1_SSE_SIGNAL_V21_LOW,
    /* SPE has detected a V.21 high channel signal */
    V150_1_SSE_SIGNAL_V21_HIGH,
    /* SPE has detected a V.23 low channel signal */
    V150_1_SSE_SIGNAL_V23_LOW,
    /* SPE has detected a V.23 high channel signal */
    V150_1_SSE_SIGNAL_V23_HIGH,
    /* SPE has detected a V.22 bis scrambled binary one's signal */
    V150_1_SSE_SIGNAL_SB1,
    /* SPE has detected a V.22 bis unscrambled binary one's signal */
    V150_1_SSE_SIGNAL_USB1,
    /* SPE has detected a V.22 bis S1 signal */
    V150_1_SSE_SIGNAL_S1,
    /* SPE has detected a V.32/V.32 bis AA signal */
    V150_1_SSE_SIGNAL_AA,
    /* SPE has detected a V.32/V.32 bis AC signal */
    V150_1_SSE_SIGNAL_AC,
    /* Call discrimination time-out */
    V150_1_SSE_SIGNAL_CALL_DISCRIMINATION_TIMEOUT,
    /* SPE has detected an unknown or unsupported signal */
    V150_1_SSE_SIGNAL_UNKNOWN,
    /* SPE has detected silence */
    V150_1_SSE_SIGNAL_SILENCE,
    /* SPE has initiated an abort request */
    V150_1_SSE_SIGNAL_ABORT,

    /* Signal list 2 */

    /* SPE requested to generate a V.25 ANS type answer tone signal */
    V150_1_SSE_SIGNAL_ANS_GEN,
    /* SPE requested to generate a V.25 ANS type answer tone signal with 180-degree phase reversals every 450 ms */
    V150_1_SSE_SIGNAL_ANS_PR_GEN,
    /* SPE requested to generate a V.8 ANSam type answer tone signal */
    V150_1_SSE_SIGNAL_ANSAM_GEN,
    /* SPE requested to generate a V.8 ANSam type answer tone signal with 180-degree phase reversals every 450 ms */
    V150_1_SSE_SIGNAL_ANSAM_PR_GEN,
    /* SPE requested to generate a 2225Hz tone */
    V150_1_SSE_SIGNAL_2225HZ_GEN,
    /* SPE requested to prevent any modem signal to be output to the telephony side of the gateway */
    V150_1_SSE_SIGNAL_CONCEAL_MODEM,
    /* SPE requested to block 2100Hz tone */
    V150_1_SSE_SIGNAL_BLOCK_2100HZ_TONE,
    /* SPE requested to enable automode function */
    V150_1_SSE_SIGNAL_AUTOMODE_ENABLE,

    /* Signal list 3 */

    /* Send audio state with reason code */
    V150_1_SSE_SIGNAL_A_GEN,
    /* Send facsimile relay state with reason code */
    V150_1_SSE_SIGNAL_F_GEN,
    /* Send indeterminate state with reason code */
    V150_1_SSE_SIGNAL_I_GEN,
    /* Send modem relay state with reason code */
    V150_1_SSE_SIGNAL_M_GEN,
    /* Send text relay state with reason code */
    V150_1_SSE_SIGNAL_T_GEN,
    /* Send VBD state with reason code */
    V150_1_SSE_SIGNAL_V_GEN,
    /* Send RFC2833 ANS event */
    V150_1_SSE_SIGNAL_RFC4733_ANS_GEN,
    /* Send RFC2833 ANS with phase reversals event */
    V150_1_SSE_SIGNAL_RFC4733_ANS_PR_GEN,
    /* Send RFC2833 ANSam event */
    V150_1_SSE_SIGNAL_RFC4733_ANSAM_GEN,
    /* Send RFC2833 ANSam with phase reversals event */
    V150_1_SSE_SIGNAL_RFC4733_ANSAM_PR_GEN,
    /* Send RFC2833 tone */
    V150_1_SSE_SIGNAL_RFC4733_TONE_GEN,

    /* Signal list 4 */

    /* Audio state detected with reason code */
    V150_1_SSE_SIGNAL_A,
    /* Facsimile relay state detected with reason code */
    V150_1_SSE_SIGNAL_F,
    /* Indeterminate state detected with reason code */
    V150_1_SSE_SIGNAL_I,
    /* Modem relay state detected with reason code */
    V150_1_SSE_SIGNAL_M,
    /* Text relay state detected with reason code */
    V150_1_SSE_SIGNAL_T,
    /* VBD state detected with reason code */
    V150_1_SSE_SIGNAL_V,
    /* An RFC4733 ANS event detected with reason code */
    V150_1_SSE_SIGNAL_RFC4733_ANS,
    /* An RFC4733 ANS with phase reversals event detected */
    V150_1_SSE_SIGNAL_RFC4733_ANS_PR,
    /* An RFC4733 ANSam event detected */
    V150_1_SSE_SIGNAL_RFC4733_ANSAM,
    /* An RFC4733 ANSam with phase reversals event detected */
    V150_1_SSE_SIGNAL_RFC4733_ANSAM_PR,
    /* An RFC4733 tone detected */
    V150_1_SSE_SIGNAL_RFC4733_TONE,

    /* Signal lists 5 and 6 */

    /* Audio state */
    V150_1_SSE_SIGNAL_A_STATE,
    /* Facsimile relay state */
    V150_1_SSE_SIGNAL_F_STATE,
    /* Indeterminate state */
    V150_1_SSE_SIGNAL_I_STATE,
    /* Modem relay state */
    V150_1_SSE_SIGNAL_M_STATE,
    /* Text relay state */
    V150_1_SSE_SIGNAL_T_STATE,
    /* VBD state */
    V150_1_SSE_SIGNAL_V_STATE
};

struct v150_1_sse_state_s
{
    int reliability_method;

    int repetition_count;                                                   /* Default 3        See V.150.1 C.4.1 */
    int repetition_interval;                                                /* Defautl 20ms     See V.150.1 C.4.1 */

    int ack_n0count;                                                        /* Default 3        See V.150.1 C.4.3.1 */
    int ack_t0interval;                                                     /* Default 10ms     See V.150.1 C.4.3.1 */
    int ack_t1interval;                                                     /* Default 300ms    See V.150.1 C.4.3.1 */

    int recovery_n;                                                         /* Default 5        See V.150.1 C.5.4.1 */
    int recovery_t1;                                                        /* Default 1s       See V.150.1 C.5.4.1 */
    int recovery_t2;                                                        /* Default 1s       See V.150.1 C.5.4.1 */

    span_timestamp_t latest_timer;

    /* True is voice band bata (VBD) is allowed */
    bool vbd_available;
    /* True is voice band bata (VBD) is preferred */
    bool vbd_preferred;

    /* Explicit acknowledgement variables. */
    bool explicit_ack_enabled;
    /* The current media state of the local node (i.e., the value that will be sent to the remote
       node in the event field of an SSE message) */
    uint8_t lcl_mode;                                                       /* See V.150.1 C.4.3.1 */
    /* The last known media state of the remote node, as known by the local node (i.e. the value
       that will be sent to the remote node in the remote media state field of an SSE extension
       field with explicit acknowledgement) */
    uint8_t rmt_mode;                                                       /* See V.150.1 C.4.3.1 */
    /* The last known mode of the local node known by the remote node, as known by the local node
       (i.e., the value that was received from the remote node in the remote media state field of
       an SSE extension field with explicit acknowledgement) */
    uint8_t rmt_ack;                                                        /* See V.150.1 C.4.3.1 */

    span_timestamp_t recovery_timer_t1;                                     /* See V.150.1 C.5.4.1 */
    span_timestamp_t recovery_timer_t2;                                     /* See V.150.1 C.5.4.1 */
    int recovery_counter_n;                                                 /* See V.150.1 C.5.4.1 */

    /* Timer to control repetition transmission */
    span_timestamp_t repetition_timer;                                      /* See V.150.1 C.4.1 */
    /* Counter used to control repetition transmission */
    int repetition_counter;                                                 /* See V.150.1 C.4.1 */

    /* Timer to control sending mode change messages to the remote node */
    span_timestamp_t ack_timer_t0;                                          /* See V.150.1 C.4.3.1 */
    /* Timer to recover from lost acknowledgements sent by the remote node */
    span_timestamp_t ack_timer_t1;                                          /* See V.150.1 C.4.3.1 */
    /* Counter used to control sending mode change messages to the remote node */
    int ack_counter_n0;                                                     /* See V.150.1 C.4.3.1 */
    bool force_response;

    bool immediate_timer;

    uint8_t last_tx_pkt[256];
    int last_tx_len;

    /* The last timestamp received from the remote gateway or endpoint */
    uint32_t previous_rx_timestamp;

    v150_1_sse_packet_handler_t tx_packet_handler;
    void *tx_packet_user_data;
    v150_1_sse_status_handler_t status_handler;
    void *status_user_data;
    v150_1_sse_timer_handler_t timer_handler;
    void *timer_user_data;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

#endif
/*- End of file ------------------------------------------------------------*/
