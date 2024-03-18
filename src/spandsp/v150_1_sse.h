/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v150_1_sse.h - An implementation of the state signaling events (SSE),
 *                protocol defined in V.150.1 Annex C, less the packet
 *                exchange part
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

//typedef int (*v150_1_sse_tx_packet_handler_t) (void *user_data, bool repeat, const uint8_t pkt[], int len);

//typedef int (*v150_1_sse_status_handler_t) (void *user_data, int status);

/* V.150.1 C.4.1 */
#define V150_1_SSE_DEFAULT_REPETITIONS              3
#define V150_1_SSE_DEFAULT_REPETITION_INTERVAL      20000

/* V.150.1 C.4.3.1 */
#define V150_1_SSE_DEFAULT_ACK_N0                   3
#define V150_1_SSE_DEFAULT_ACK_T0                   10000
#define V150_1_SSE_DEFAULT_ACK_T1                   300000

/* V.150.1 C.5.4.1 */
#define V150_1_SSE_DEFAULT_RECOVERY_N               5
#define V150_1_SSE_DEFAULT_RECOVERY_T1              1000000
#define V150_1_SSE_DEFAULT_RECOVERY_T2              1000000

/* Table 12/V.150.1 plus amendments - SSE RIC codes for MoIP and ToIP (as per 15.2.1/V.151) */
enum v150_1_sse_moip_ric_code_e
{
    /* Additional info: Available modulation modes as indicated in the CM sequence (Format is defined in Table 13) */
    V150_1_SSE_MOIP_RIC_V8_CM                                       = 1,
    /* Additional info: Available modulation modes as indicated in the JM sequence (Format is defined in Table 13) */
    V150_1_SSE_MOIP_RIC_V8_JM                                       = 2,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_V32BIS_AA                                   = 3,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_V32BIS_AC                                   = 4,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_V22BIS_USB1                                 = 5,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_V22BIS_SB1                                  = 6,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_V22BIS_S1                                   = 7,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_V21_CH2                                     = 8,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_V21_CH1                                     = 9,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_V23_HIGH_CHANNEL                            = 10,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_V23_LOW_CHANNEL                             = 11,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_TONE_2225HZ                                 = 12,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_V21_CH2_HDLC_FLAGS                          = 13,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_INDETERMINATE_SIGNAL                        = 14,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_SILENCE                                     = 15,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_CNG                                         = 16,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_VOICE                                       = 17,
    /* Additional info: The timeout event (Format is defined in Table 14) */
    V150_1_SSE_MOIP_RIC_TIMEOUT                                     = 18,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_P_STATE_TRANSITION                          = 19,
    /* Additional info: Reason for clear down (Format is defined in Table 15) */
    V150_1_SSE_MOIP_RIC_CLEARDOWN                                   = 20,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_ANS_CED                                     = 21,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_ANSAM                                       = 22,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_ANS_PR                                      = 23,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_ANSAM_PR                                    = 24,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_V92_QC1A                                    = 25,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_V92_QC1D                                    = 26,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_V92_QC2A                                    = 27,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_V92_QC2D                                    = 28,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_V8BIS_CRE                                   = 29,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_V8BIS_CRD                                   = 30,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_TIA825A_45_45BPS                            = 31,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_TIA825A_50BPS                               = 32,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_EDT                                         = 33,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_BELL103                                     = 34,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_V21_TEXT_TELEPHONE                          = 35,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_V23_MINITEL                                 = 36,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_V18_TEXT_TELEPHONE                          = 37,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_V18_DTMF_TEXT_RELAY                         = 38,
    /* Additional info: None */
    V150_1_SSE_MOIP_RIC_CTM                                         = 39,
    V150_1_SSE_MOIP_RIC_VENDOR_MIN                                  = 128,
    V150_1_SSE_MOIP_RIC_VENDOR_MAX                                  = 255
};

/* Annex F/T.38 - SSE RIC codes for FoIP */
enum v150_1_sse_foip_ric_code_e
{
    /* Additional info: Available modulation modes as indicated in the CM sequence (Format is defined in Table 13) */
    V150_1_SSE_FOIP_RIC_V21_FLAGS                                   = 1,
    /* Additional info: Available modulation modes as indicated in the JM sequence (Format is defined in Table 13) */
    V150_1_SSE_FOIP_RIC_V8_CM                                       = 2,
    /* Additional info: None */
    V150_1_SSE_FOIP_RIC_P_STATE_TRANSITION                          = 19
};

/* Table 13/V.150.1 - CM and JM additional information format in SSE payloads */
enum v150_1_sse_moip_ric_info_v8_cm_code_e
{
    V150_1_SSE_MOIP_RIC_INFO_V8_CM_PCM_MODE                         = 0x8000,
    V150_1_SSE_MOIP_RIC_INFO_V8_CM_V34_DUPLEX                       = 0x4000,
    V150_1_SSE_MOIP_RIC_INFO_V8_CM_V34_HALF_DUPLEX                  = 0x2000,
    V150_1_SSE_MOIP_RIC_INFO_V8_CM_V32BIS                           = 0x1000,
    V150_1_SSE_MOIP_RIC_INFO_V8_CM_V22BIS                           = 0x0800,
    V150_1_SSE_MOIP_RIC_INFO_V8_CM_V17                              = 0x0400,
    V150_1_SSE_MOIP_RIC_INFO_V8_CM_V29                              = 0x0200,
    V150_1_SSE_MOIP_RIC_INFO_V8_CM_V27TER                           = 0x0100,
    V150_1_SSE_MOIP_RIC_INFO_V8_CM_V26TER                           = 0x0080,
    V150_1_SSE_MOIP_RIC_INFO_V8_CM_V26BIS                           = 0x0040,
    V150_1_SSE_MOIP_RIC_INFO_V8_CM_V23_DUPLEX                       = 0x0020,
    V150_1_SSE_MOIP_RIC_INFO_V8_CM_V23_HALF_DUPLEX                  = 0x0010,
    V150_1_SSE_MOIP_RIC_INFO_V8_CM_V21                              = 0x0008,
    V150_1_SSE_MOIP_RIC_INFO_V8_CM_V90_V92_ANALOGUE                 = 0x0004,
    V150_1_SSE_MOIP_RIC_INFO_V8_CM_V90_V92_DIGITAL                  = 0x0002,
    V150_1_SSE_MOIP_RIC_INFO_V8_CM_V91                              = 0x0001
};

/* Table 14/V.150.1 - SSE timeout reason code definitions in SSE payload */
enum v150_1_sse_moip_ric_info_v8_timeout_reason_code_e
{
    V150_1_SSE_MOIP_RIC_INFO_TIMEOUT_NULL                           = 0,
    V150_1_SSE_MOIP_RIC_INFO_TIMEOUT_CALL_DISCRIMINATION_TIMEOUT    = 1,
    V150_1_SSE_MOIP_RIC_INFO_TIMEOUT_IP_TLP                         = 2,
    V150_1_SSE_MOIP_RIC_INFO_TIMEOUT_SSE_EXPLICIT_ACK_TIMEOUT       = 3
};

/* Table 28/V.150.1 - SSE cleardown reason code definitions in SSE payload */
enum v150_1_sse_moip_ric_info_v8_cleardown_reason_code_e
{
    V150_1_SSE_MOIP_RIC_INFO_CLEARDOWN_UNKNOWN                      = 0,
    V150_1_SSE_MOIP_RIC_INFO_CLEARDOWN_PHYSICAL_LAYER_RELEASE       = 1,
    V150_1_SSE_MOIP_RIC_INFO_CLEARDOWN_LINK_LAYER_DISCONNECT        = 2,
    V150_1_SSE_MOIP_RIC_INFO_CLEARDOWN_COMPRESSION_DISCONNECT       = 3,
    V150_1_SSE_MOIP_RIC_INFO_CLEARDOWN_ABORT                        = 4,
    V150_1_SSE_MOIP_RIC_INFO_CLEARDOWN_ON_HOOK                      = 5,
    V150_1_SSE_MOIP_RIC_INFO_CLEARDOWN_NETWORK_LAYER_TERMINATION    = 6,
    V150_1_SSE_MOIP_RIC_INFO_CLEARDOWN_ADMINISTRATIVE               = 7
};

/* V.150.1 C.4 */
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
    V150_1_SSE_STATUS_AA_RECEIVED_FAX                           = 15,
    V150_1_SSE_STATUS_CLEARDOWN                                 = 16
};

typedef struct v150_1_sse_state_s v150_1_sse_state_t;

#if defined(__cplusplus)
extern "C" {
#endif

SPAN_DECLARE(const char *) v150_1_sse_media_state_to_str(int state);

SPAN_DECLARE(const char *) v150_1_sse_moip_ric_to_str(int ric);

SPAN_DECLARE(const char *) v150_1_sse_timeout_reason_to_str(int reason);

SPAN_DECLARE(const char *) v150_1_sse_cleardown_reason_to_str(int reason);

SPAN_DECLARE(const char *) v150_1_sse_status_to_str(int status);

/*! Receive an SSE packet, broken out of an RTP stream.
    \brief Receive an SSE packet.
    \param s V.150.1 SSE context.
    \param seq_no
    \param timestamp
    \param pkt
    \param len */
SPAN_DECLARE(int) v150_1_rx_sse_packet(v150_1_state_t *s,
                                       uint16_t seq_no,
                                       uint32_t timestamp,
                                       const uint8_t pkt[],
                                       int len);

/*! Transmit an SSE packet, for insertion into an RTP packet. This is normally needed by an
    application.
    \brief Transmit an SSE packet.
    \param s V.150.1 SSE context.
    \param event
    \param ric
    \param ricinfo */
SPAN_DECLARE(int) v150_1_tx_sse_packet(v150_1_state_t *s, int event, int ric, int ricinfo);

/*! Select one of the reliability schemes from V.150.1/C.4.
    \brief Select one of the reliability schemes from V.150.1/C.4.
    \param s V.150.1 SSE context.
    \param method The chosen method.
    \param parm1 maximum transmissions.
    \param parm2 delay between transmissions, or T0, in microseconds.
    \param parm3 T1, in microseconds.
    \return 0 for Ok, else negative. */
SPAN_DECLARE(int) v150_1_set_sse_reliability_method(v150_1_state_t *s,
                                                    enum v150_1_sse_reliability_option_e method,
                                                    int parm1,
                                                    int parm2,
                                                    int parm3);

#if defined(__cplusplus)
}
#endif
#endif
/*- End of file ------------------------------------------------------------*/
