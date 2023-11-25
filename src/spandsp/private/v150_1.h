/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/v150_1.h - An implementation of V.150.1.
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

#if !defined(_SPANDSP_PRIVATE_V150_1_H_)
#define _SPANDSP_PRIVATE_V150_1_H_

/*
               telephone network
                      ^
                      |
                      |
                      v
    +-----------------------------------+
    |                                   |
    |   Signal processing entity (SPE)  |
    |                                   |
    +-----------------------------------+
                |           ^
                |           |
  Signal list 1 |           | Signal list 2
                |           |
                v           |
    +-----------------------------------+      Signal list 5      +-----------------------------------+
    |                                   | ----------------------->|                                   |
    |   SSE protocol state machine (P)  |                         |    Gateway state machine (s,s')   |
    |                                   |<------------------------|                                   |
    +-----------------------------------+      Signal list 6      +-----------------------------------+
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

enum V150_1_SIGNAL_e
{
    /* Signal list 1 - SPE to SSE protocol state engine */

    /* SPE has detected 2100Hz tone for a duration less than 50ms */
    V150_1_SIGNAL_TONE_2100HZ = 1,
    /* SPE has detected 2225Hz tone for a duration less than 50ms */
    V150_1_SIGNAL_TONE_2225HZ,
    /* SPE has verified presence of V.25 ANS type answer tone */
    V150_1_SIGNAL_ANS,
    /* SPE has detected a 180-degree phase reversal in a verified ANS type answer tone */
    V150_1_SIGNAL_ANS_PR,
    /* SPE has verified presence of V.8 ANSam type answer tone */
    V150_1_SIGNAL_ANSAM,
    /* SPE has detected a 180-degree phase reversal in a verified ANSam type answer tone */
    V150_1_SIGNAL_ANSAM_PR,
    /* SPE has detected a V.8 CI signal */
    V150_1_SIGNAL_CI,
    /* SPE has detected a V.8 CM signal */
    V150_1_SIGNAL_CM,
    /* SPE has detected a V.8 JM signal */
    V150_1_SIGNAL_JM,
    /* SPE has detected a V.21 low channel signal */
    V150_1_SIGNAL_V21_LOW,
    /* SPE has detected a V.21 high channel signal */
    V150_1_SIGNAL_V21_HIGH,
    /* SPE has detected a V.23 low channel signal */
    V150_1_SIGNAL_V23_LOW,
    /* SPE has detected a V.23 high channel signal */
    V150_1_SIGNAL_V23_HIGH,
    /* SPE has detected a V.22bis scrambled binary one's signal */
    V150_1_SIGNAL_SB1,
    /* SPE has detected a V.22bis unscrambled binary one's signal */
    V150_1_SIGNAL_USB1,
    /* SPE has detected a V.22bis S1 signal */
    V150_1_SIGNAL_S1,
    /* SPE has detected a V.32/V.32bis AA signal */
    V150_1_SIGNAL_AA,
    /* SPE has detected a V.32/V.32bis AC signal */
    V150_1_SIGNAL_AC,
    /* Call discrimination time-out */
    V150_1_SIGNAL_CALL_DISCRIMINATION_TIMEOUT,
    /* SPE has detected an unknown or unsupported signal */
    V150_1_SIGNAL_UNKNOWN,
    /* SPE has detected silence */
    V150_1_SIGNAL_SILENCE,
    /* SPE has initiated an abort request */
    V150_1_SIGNAL_ABORT,

    /* Signal list 2 - SSE protocol state engine to SPE */

    /* SPE requested to generate a V.25 ANS type answer tone signal */
    V150_1_SIGNAL_ANS_GEN,
    /* SPE requested to generate a V.25 ANS type answer tone signal with 180-degree phase reversals every 450 ms */
    V150_1_SIGNAL_ANS_PR_GEN,
    /* SPE requested to generate a V.8 ANSam type answer tone signal */
    V150_1_SIGNAL_ANSAM_GEN,
    /* SPE requested to generate a V.8 ANSam type answer tone signal with 180-degree phase reversals every 450 ms */
    V150_1_SIGNAL_ANSAM_PR_GEN,
    /* SPE requested to generate a 2225Hz tone */
    V150_1_SIGNAL_2225HZ_GEN,
    /* SPE requested to prevent any modem signal to be output to the telephony side of the gateway */
    V150_1_SIGNAL_CONCEAL_MODEM,
    /* SPE requested to block 2100Hz tone */
    V150_1_SIGNAL_BLOCK_2100HZ_TONE,
    /* SPE requested to enable automode function */
    V150_1_SIGNAL_AUTOMODE_ENABLE,

    /* Signal list 3 - SSE protocol state engine to IP network */

    /* Send audio state with reason code */
    V150_1_SIGNAL_AUDIO_GEN,
    /* Send facsimile relay state with reason code */
    V150_1_SIGNAL_FAX_RELAY_GEN,
    /* Send indeterminate state with reason code */
    V150_1_SIGNAL_INDETERMINATE_GEN,
    /* Send modem relay state with reason code */
    V150_1_SIGNAL_MODEM_RELAY_GEN,
    /* Send text relay state with reason code */
    V150_1_SIGNAL_TEXT_RELAY_GEN,
    /* Send VBD state with reason code */
    V150_1_SIGNAL_VBD_GEN,
    /* Send RFC4733 ANS event */
    V150_1_SIGNAL_RFC4733_ANS_GEN,
    /* Send RFC4733 ANS with phase reversals event */
    V150_1_SIGNAL_RFC4733_ANS_PR_GEN,
    /* Send RFC4733 ANSam event */
    V150_1_SIGNAL_RFC4733_ANSAM_GEN,
    /* Send RFC4733 ANSam with phase reversals event */
    V150_1_SIGNAL_RFC4733_ANSAM_PR_GEN,
    /* Send RFC4733 tone */
    V150_1_SIGNAL_RFC4733_TONE_GEN,

    /* Signal list 4 - IP network to SSE protocol state engine */

    /* Audio state detected with reason code */
    V150_1_SIGNAL_AUDIO,
    /* Facsimile relay state detected with reason code */
    V150_1_SIGNAL_FAX_RELAY,
    /* Indeterminate state detected with reason code */
    V150_1_SIGNAL_INDETERMINATE,
    /* Modem relay state detected with reason code */
    V150_1_SIGNAL_MODEM_RELAY,
    /* Text relay state detected with reason code */
    V150_1_SIGNAL_TEXT_RELAY,
    /* VBD state detected with reason code */
    V150_1_SIGNAL_VBD,
    /* An RFC4733 ANS event detected with reason code */
    V150_1_SIGNAL_RFC4733_ANS,
    /* An RFC4733 ANS with phase reversals event detected */
    V150_1_SIGNAL_RFC4733_ANS_PR,
    /* An RFC4733 ANSam event detected */
    V150_1_SIGNAL_RFC4733_ANSAM,
    /* An RFC4733 ANSam with phase reversals event detected */
    V150_1_SIGNAL_RFC4733_ANSAM_PR,
    /* An RFC4733 tone detected */
    V150_1_SIGNAL_RFC4733_TONE,

    /* Lists 5 and 6 have the same contents */
    /* Signal list 5 - SSE protocol state engine to gateway */
    /* Signal list 6 - Gateway to SSE protocol state engine */

    /* Audio state */
    V150_1_SIGNAL_AUDIO_STATE,
    /* Facsimile relay state */
    V150_1_SIGNAL_FAX_RELAY_STATE,
    /* Indeterminate state */
    V150_1_SIGNAL_INDETERMINATE_STATE,
    /* Modem relay state */
    V150_1_SIGNAL_MODEM_RELAY_STATE,
    /* Text relay state */
    V150_1_SIGNAL_TEXT_RELAY_STATE,
    /* VBD state */
    V150_1_SIGNAL_VBD_STATE,

    /* Signal not listed in V.150.1 */
    V150_1_SIGNAL_CALL_DISCRIMINATION_TIMER_EXPIRED
};

typedef struct
{
    v150_1_cdscselect_t cdscselect;
    v150_1_modem_relay_gateway_type_t modem_relay_gateway_type;

    bool v42_lapm_supported;
    /* Annex A was removed from the V.42 spec. in 2002, so it won't be supported. */
    bool v42_annex_a_supported;
    bool v42bis_supported;
    bool v44_supported;
    bool mnp5_supported;

    int ecp;
    bool necrxch_option;
    bool ecrxch_option;
    bool xid_profile_exchange_supported;
    bool asymmetric_data_types_supported;
    bool dlci_supported;
    bool i_raw_bit_supported;
    bool i_char_stat_supported;
    bool i_char_dyn_supported;
    bool i_frame_supported;
    bool i_octet_cs_supported;
    bool i_char_stat_cs_supported;
    bool i_char_dyn_cs_supported;

    bool i_raw_bit_available;
    bool i_frame_available;
    bool i_octet_with_dlci_available;
    bool i_octet_without_dlci_available;
    bool i_char_stat_available;
    bool i_char_dyn_available;
    bool i_octet_cs_available;
    bool i_char_stat_cs_available;
    bool i_char_dyn_cs_available;

    uint16_t compression_tx_dictionary_size;
    uint16_t compression_rx_dictionary_size;
    uint8_t compression_tx_string_length;
    uint8_t compression_rx_string_length;
    uint16_t compression_tx_history_size;
    uint16_t compression_rx_history_size;

    bool jm_category_id_seen[16];
    uint16_t jm_category_info[16];

    uint16_t v42bis_p0;     /* directions */
    uint16_t v42bis_p1;     /* codewords */
    uint16_t v42bis_p2;     /* string size */
    uint16_t v44_c0;        /* capability */
    uint16_t v44_p0;        /* directions */
    uint16_t v44_p1t;       /* tx_dictionary_size */
    uint16_t v44_p1r;       /* rx_dictionary_size */
    uint16_t v44_p2t;       /* tx_string_size */
    uint16_t v44_p2r;       /* rx_string_size */
    uint16_t v44_p3t;       /* tx_history_size */
    uint16_t v44_p3r;       /* rx_history_size */

    uint16_t selected_compression_direction;
    uint16_t selected_compression;
    uint16_t selected_error_correction;

    /* Data link connection identifier */
    uint16_t dlci;

    /* Sequence number for the information packets which contain a transmitted character sequence number */
    uint16_t octet_cs_next_seq_no;
    /* The data format for asynchronous data characters - data bits, parity and stop bits */
    uint8_t data_format_code;

    /* Selected modulation scheme */
    uint16_t selmod;
    /* Transmit symbol rate enable */
    bool txsen;
    /* Receive symbol rate enable */
    bool rxsen;
    /* Transmit data signalling rate */
    uint16_t tdsr;
    /* Receive data signalling rate */
    uint16_t rdsr;
    /* Physical layer transmitter symbol rate */
    uint16_t txsr;
    /* Physical layer receiver symbol rate */
    uint16_t rxsr;

    bool busy;

    int sprt_subsession_id;
    uint8_t sprt_payload_type;

    int connection_state;
    int cleardown_reason;
} v150_1_near_far_t;

struct v150_1_state_s
{
    v150_1_rx_data_handler_t rx_data_handler;
    void *rx_data_handler_user_data;
    v150_1_rx_status_report_handler_t rx_status_report_handler;
    void *rx_status_report_user_data;
    v150_1_spe_signal_handler_t spe_signal_handler;
    void *spe_signal_handler_user_data;
    v150_1_timer_handler_t timer_handler;
    void *timer_user_data;

    v150_1_cdscselect_t cdscselect;
    /* True if RFC4733 is preferred */
    bool rfc4733_preferred;
    int call_discrimination_timeout;

    /* The current media state of the local node (i.e., the value that will be sent to the remote
       node in the event field of an SSE message) */
    uint8_t local_media_state;                                              /* See V.150.1 C.4.3.1 */
    /* The last known media state of the remote node, as known by the local node (i.e. the value
       that will be sent to the remote node in the remote media state field of an SSE extension
       field with explicit acknowledgement) */
    uint8_t remote_media_state;                                             /* See V.150.1 C.4.3.1 */
    /* The last known mode of the local node known by the remote node, as known by the local node
       (i.e., the value that was received from the remote node in the remote media state field of
       an SSE extension field with explicit acknowledgement) */
    uint8_t remote_ack;                                                     /* See V.150.1 C.4.3.1 */

    struct
    {
        v150_1_near_far_t parms;

        int8_t info_msg_preferences[10];

        /* The maximum packet lengths we may generate. These vary with the channel number when using SPRT
           as the transport. So, we hold a length for each SPRT protocol channel ID. */
        int max_payload_bytes[SPRT_CHANNELS];

        /* The channel to be used for info packets */
        uint16_t info_stream_channel;
        /* The message ID to be used for info packets */
        uint16_t info_stream_msg_id;
    } near;
    struct
    {
        v150_1_near_far_t parms;

        int break_source;
        int break_type;
        int break_duration;
    } far;
    int joint_connection_state;

    v150_1_sse_state_t sse;
    sprt_state_t sprt;

    span_timestamp_t latest_timer;
    span_timestamp_t call_discrimination_timer;
    span_timestamp_t sse_timer;
    span_timestamp_t sprt_timer;
    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

#endif
/*- End of file ------------------------------------------------------------*/
