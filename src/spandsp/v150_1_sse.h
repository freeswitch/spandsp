/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v150_1_sse.h - An implementation of the SSE protocol defined in V.150.1
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

#if !defined(_SPANDSP_V150_1_SSE_H_)
#define _SPANDSP_V150_1_SSE_H_

typedef int (*v150_1_sse_packet_handler_t) (void *user_data, const uint8_t pkt[], int len);

typedef int (*v150_1_sse_status_handler_t) (void *user_data, int status);

typedef span_timestamp_t (*v150_1_sse_timer_handler_t) (void *user_data, span_timestamp_t timeout);

/* V.150.1 C.5.3 */
enum v150_1_states_e
{
    V150_1_STATE_INITIAL_AUDIO                                  = 'a',
    V150_1_STATE_VOICE_BAND_DATA                                = 'v',
    V150_1_STATE_MODEM_RELAY                                    = 'm',
    V150_1_STATE_FAX_RELAY                                      = 'f',
    V150_1_STATE_TEXT_RELAY                                     = 't',
    V150_1_STATE_INDETERMINATE                                  = 'i'
};

/* Table C.1/V.150.1 plus amendments */
enum v150_1_sse_media_states_e
{
    V150_1_SSE_MEDIA_STATE_ITU_RESERVED_0                       = 0,        /* Reserved for future use by ITU-T */
    V150_1_SSE_MEDIA_STATE_INITIAL_AUDIO                        = 1,        /* Initial Audio */
    V150_1_SSE_MEDIA_STATE_VOICE_BAND_DATA                      = 2,        /* Voice Band Data (VBD) */
    V150_1_SSE_MEDIA_STATE_MODEM_RELAY                          = 3,        /* Modem Relay */
    V150_1_SSE_MEDIA_STATE_FAX_RELAY                            = 4,        /* Fax Relay */
    V150_1_SSE_MEDIA_STATE_TEXT_RELAY                           = 5,        /* Text Relay */
    V150_1_SSE_MEDIA_STATE_TEXT_PROBE                           = 6,        /* Text Probe (Amendment 2) */
    V150_1_SSE_MEDIA_STATE_ITU_RESERVED_MIN                     = 7,        /* Start of ITU reserved range */
    V150_1_SSE_MEDIA_STATE_ITU_RESERVED_MAX                     = 31,       /* End of ITU reserved range */
    V150_1_SSE_MEDIA_STATE_RESERVED_MIN                         = 32,       /* Start of vendor defined reserved range */
    V150_1_SSE_MEDIA_STATE_RESERVED_MAX                         = 63        /* End of vendor defined reserved range */
};

/* Table 12/V.150.1 plus amendments - SSE RIC codes for MoIP */
enum v150_1_sse_ric_code_e
{
    /* Additional info: Available modulation modes as indicated in the CM sequence (Format is defined in Table 13) */
    V150_1_SSE_RIC_V8_CM                                        = 1,
    /* Additional info: Available modulation modes as indicated in the JM sequence (Format is defined in Table 13) */
    V150_1_SSE_RIC_V8_JM                                        = 2,
    /* Additional info: None */
    V150_1_SSE_RIC_V32BIS_AA                                    = 3,
    /* Additional info: None */
    V150_1_SSE_RIC_V32BIS_AC                                    = 4,
    /* Additional info: None */
    V150_1_SSE_RIC_V22BIS_USB1                                  = 5,
    /* Additional info: None */
    V150_1_SSE_RIC_V22BIS_SB1                                   = 6,
    /* Additional info: None */
    V150_1_SSE_RIC_V22BIS_S1                                    = 7,
    /* Additional info: None */
    V150_1_SSE_RIC_V21_CH2                                      = 8,
    /* Additional info: None */
    V150_1_SSE_RIC_V21_CH1                                      = 9,
    /* Additional info: None */
    V150_1_SSE_RIC_V23_HIGH_CHANNEL                             = 10,
    /* Additional info: None */
    V150_1_SSE_RIC_V23_LOW_CHANNEL                              = 11,
    /* Additional info: None */
    V150_1_SSE_RIC_TONE_2225HZ                                  = 12,
    /* Additional info: None */
    V150_1_SSE_RIC_V21_CH2_HDLC_FLAGS                           = 13,
    /* Additional info: None */
    V150_1_SSE_RIC_INDETERMINATE_SIGNAL                         = 14,
    /* Additional info: None */
    V150_1_SSE_RIC_SILENCE                                      = 15,
    /* Additional info: None */
    V150_1_SSE_RIC_CNG                                          = 16,
    /* Additional info: None */
    V150_1_SSE_RIC_VOICE                                        = 17,
    /* Additional info: The timeout event (Format is defined in Table 14) */
    V150_1_SSE_RIC_TIMEOUT                                      = 18,
    /* Additional info: None */
    V150_1_SSE_RIC_P_STATE_TRANSITION                           = 19,
    /* Additional info: Reason for clear down (Format is defined in Table 15) */
    V150_1_SSE_RIC_CLEARDOWN                                    = 20,
    /* Additional info: None */
    V150_1_SSE_RIC_ANS_CED                                      = 21,
    /* Additional info: None */
    V150_1_SSE_RIC_ANSAM                                        = 22,
    /* Additional info: None */
    V150_1_SSE_RIC_ANS_PR                                       = 23,
    /* Additional info: None */
    V150_1_SSE_RIC_ANSAM_PR                                     = 24,
    /* Additional info: None */
    V150_1_SSE_RIC_V92_QC1A                                     = 25,
    /* Additional info: None */
    V150_1_SSE_RIC_V92_QC1D                                     = 26,
    /* Additional info: None */
    V150_1_SSE_RIC_V92_QC2A                                     = 27,
    /* Additional info: None */
    V150_1_SSE_RIC_V92_QC2D                                     = 28,
    /* Additional info: None */
    V150_1_SSE_RIC_V8BIS_CRE                                    = 29,
    /* Additional info: None */
    V150_1_SSE_RIC_V8BIS_CRD                                    = 30,
    /* Additional info: None */
    V150_1_SSE_RIC_TIA825A_45_45BPS                             = 31,
    /* Additional info: None */
    V150_1_SSE_RIC_TIA825A_50BPS                                = 32,
    /* Additional info: None */
    V150_1_SSE_RIC_EDT                                          = 33,
    /* Additional info: None */
    V150_1_SSE_RIC_BELL103                                      = 34,
    /* Additional info: None */
    V150_1_SSE_RIC_V21_TEXT_TELEPHONE                           = 35,
    /* Additional info: None */
    V150_1_SSE_RIC_V23_MINITEL                                  = 36,
    /* Additional info: None */
    V150_1_SSE_RIC_V18_TEXT_TELEPHONE                           = 37,
    /* Additional info: None */
    V150_1_SSE_RIC_V18_DTMF_TEXT_RELAY                          = 38,
    /* Additional info: None */
    V150_1_SSE_RIC_CTM                                          = 39,
    V150_1_SSE_RIC_VENDOR_MIN                                   = 128,
    V150_1_SSE_RIC_VENDOR_MAX                                   = 255
};

/* Table 13/V.150.1 - CM and JM additional information format in SSE payloads */
enum v150_1_sse_ric_info_v8_cm_code_e
{
    V150_1_SSE_RIC_INFO_V8_CM_PCM_MODE                          = 0x8000,
    V150_1_SSE_RIC_INFO_V8_CM_V34_DUPLEX                        = 0x4000,
    V150_1_SSE_RIC_INFO_V8_CM_V34_HALF_DUPLEX                   = 0x2000,
    V150_1_SSE_RIC_INFO_V8_CM_V32BIS                            = 0x1000,
    V150_1_SSE_RIC_INFO_V8_CM_V22BIS                            = 0x0800,
    V150_1_SSE_RIC_INFO_V8_CM_V17                               = 0x0400,
    V150_1_SSE_RIC_INFO_V8_CM_V29                               = 0x0200,
    V150_1_SSE_RIC_INFO_V8_CM_V27TER                            = 0x0100,
    V150_1_SSE_RIC_INFO_V8_CM_V26TER                            = 0x0080,
    V150_1_SSE_RIC_INFO_V8_CM_V26BIS                            = 0x0040,
    V150_1_SSE_RIC_INFO_V8_CM_V23_DUPLEX                        = 0x0020,
    V150_1_SSE_RIC_INFO_V8_CM_V23_HALF_DUPLEX                   = 0x0010,
    V150_1_SSE_RIC_INFO_V8_CM_V21                               = 0x0008,
    V150_1_SSE_RIC_INFO_V8_CM_V90_V92_ANALOGUE                  = 0x0004,
    V150_1_SSE_RIC_INFO_V8_CM_V90_V92_DIGITAL                   = 0x0002,
    V150_1_SSE_RIC_INFO_V8_CM_V91                               = 0x0001
};

/* Table 14/V.150.1 - SSE timeout reason code definitions in SSE payload */
enum v150_1_sse_ric_info_v8_timeout_reason_code_e
{
    V150_1_SSE_RIC_INFO_TIMEOUT_NULL                            = 0,
    V150_1_SSE_RIC_INFO_TIMEOUT_CALL_DISCRIMINATION_TIMEOUT     = 1,
    V150_1_SSE_RIC_INFO_TIMEOUT_IP_TLP                          = 2,
    V150_1_SSE_RIC_INFO_TIMEOUT_SSE_EXPLICIT_ACK_TIMEOUT        = 3
};

/* Table 28/V.150.1 - SSE cleardown reason code definitions in SSE payload */
enum v150_1_sse_ric_info_v8_cleardown_reason_code_e
{
    V150_1_SSE_RIC_INFO_CLEARDOWN_UNKNOWN                       = 0,
    V150_1_SSE_RIC_INFO_CLEARDOWN_PHYSICAL_LAYER_RELEASE        = 1,
    V150_1_SSE_RIC_INFO_CLEARDOWN_LINK_LAYER_DISCONNECT         = 2,
    V150_1_SSE_RIC_INFO_CLEARDOWN_COMPRESSION_DISCONNECT        = 3,
    V150_1_SSE_RIC_INFO_CLEARDOWN_ABORT                         = 4,
    V150_1_SSE_RIC_INFO_CLEARDOWN_ON_HOOK                       = 5,
    V150_1_SSE_RIC_INFO_CLEARDOWN_NETWORK_LAYER_TERMINATION     = 6,
    V150_1_SSE_RIC_INFO_CLEARDOWN_ADMINISTRATIVE                = 7
};

enum v150_1_sse_reliability_option_e
{
    /* There are no reliability measures in use. */
    V150_1_SSE_RELIABILITY_NONE                                 = 0,
    /* Simple SSE repetition as defined in C.4.1. This option is not declared at call
       establishment time. As the default option, it is used if one of the remaining two
       options is not declared. Note that it is permissible to set the number of
       transmissions to one (no redundancy). */
    V150_1_SSE_RELIABILITY_BY_REPETITION                        = 1,
    /* Use of RFC 2198-based redundancy for SSEs (see C.4.2). This must be explicitly
       declared at call establishment. */
    V150_1_SSE_RELIABILITY_BY_RFC2198                           = 2,
    /* Explicit acknowledgement of SSEs (see C.4.3). This scheme is based on the
       inclusion, in an SSE message, of the value of the endpoint's or gateway's
       rmt_mode variable, which indicates its view of the remote media state.
       Additionally, a gateway or endpoint may force the other end to respond
       with an SSE by setting the Forced Response (F) bit. To be used, this option
       must be explicitly declared by both ends at call establishment time. */
    V150_1_SSE_RELIABILITY_BY_EXPLICIT_ACK                      = 3
};

enum v150_1_sse_status_e
{
    V150_1_SSE_STATUS_V8_CM_RECEIVED                            = 10,
    V150_1_SSE_STATUS_V8_JM_RECEIVED                            = 11,
    V150_1_SSE_STATUS_AA_RECEIVED                               = 12,
    V150_1_SSE_STATUS_V8_CM_RECEIVED_FAX                        = 13,
    V150_1_SSE_STATUS_V8_JM_RECEIVED_FAX                        = 14,
    V150_1_SSE_STATUS_AA_RECEIVED_FAX                           = 15
};

typedef struct v150_1_sse_state_s v150_1_sse_state_t;

#if defined(__cplusplus)
extern "C" {
#endif

SPAN_DECLARE(const char *) v150_1_sse_media_state_to_str(int state);

SPAN_DECLARE(const char *) v150_1_sse_ric_to_str(int ric);

SPAN_DECLARE(const char *) v150_1_sse_timeout_reason_to_str(int ric);

SPAN_DECLARE(const char *) v150_1_sse_cleardown_reason_to_str(int ric);

SPAN_DECLARE(int) v150_1_sse_rx_packet(v150_1_sse_state_t *s,
                                       uint16_t seq_no,
                                       uint32_t timestamp,
                                       const uint8_t pkt[],
                                       int len);

SPAN_DECLARE(int) v150_1_sse_tx_packet(v150_1_sse_state_t *s, int event, int ric, int ricinfo);

SPAN_DECLARE(int) v150_1_sse_timer_expired(v150_1_sse_state_t *s, span_timestamp_t now);

SPAN_DECLARE(int) v150_1_sse_explicit_acknowledgements(v150_1_sse_state_t *s,
                                                       bool explicit_acknowledgements);

SPAN_DECLARE(logging_state_t *) v150_1_sse_get_logging_state(v150_1_sse_state_t *s);

SPAN_DECLARE(v150_1_sse_state_t *) v150_1_sse_init(v150_1_sse_state_t *s,
                                                   v150_1_sse_packet_handler_t packet_handler,
                                                   void *packet_user_data,
                                                   v150_1_sse_status_handler_t status_handler,
                                                   void *status_user_data,
                                                   v150_1_sse_timer_handler_t timer_handler,
                                                   void *timer_user_data);

#if defined(__cplusplus)
}
#endif
#endif
/*- End of file ------------------------------------------------------------*/
