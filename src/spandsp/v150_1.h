/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v150_1.h - An implementation of V.150.1.
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

#if !defined(_SPANDSP_V150_1_H_)
#define _SPANDSP_V150_1_H_

/* Structure passed with status reports */
typedef struct
{
    int reason;
    union
    {
        struct
        {
            int local_state;
            int remote_state;
        } media_state_change;
        struct
        {
            int state;
            int cleardown_reason;
        } connection_state_change;
        struct
        {
            int bits;
            int parity_code;
            int stop_bits;
        } data_format_change;
        struct
        {
            int source;
            int type;
            int duration;   /* In ms */
        } break_received;
        struct
        {
            bool local_busy;
            bool far_busy;
        } busy_change;
        struct
        {
            int selmod;
            int tdsr;
            int rdsr;
            bool txsen;
            int txsr;
            bool rxsen;
            int rxsr;
        } physup_parameters;
        struct
        {
            int selmod;
            int tdsr;
            int rdsr;

            int selected_compression_direction;
            int selected_compression;
            int selected_error_correction;

            int compression_tx_dictionary_size;
            int compression_rx_dictionary_size;
            int compression_tx_string_length;
            int compression_rx_string_length;
            int compression_tx_history_size;
            int compression_rx_history_size;

            bool i_raw_octet_available;
            bool i_raw_bit_available;
            bool i_frame_available;
            bool i_octet_with_dlci_available;
            bool i_octet_without_dlci_available;
            bool i_char_stat_available;
            bool i_char_dyn_available;
            bool i_octet_cs_available;
            bool i_char_stat_cs_available;
            bool i_char_dyn_cs_available;
        } connect_parameters;
    } types;
} v150_1_status_t;

typedef int (*v150_1_spe_signal_handler_t) (void *user_data, int signal);

typedef int (*v150_1_tx_packet_handler_t) (void *user_data, int channel, const uint8_t msg[], int len);

typedef int (*v150_1_rx_data_handler_t) (void *user_data, const uint8_t msg[], int len, int fill);

typedef int (*v150_1_rx_status_report_handler_t) (void *user_data, v150_1_status_t *report);

typedef int (*v150_1_sse_tx_packet_handler_t) (void *user_data, bool repeat, const uint8_t pkt[], int len);

typedef int (*v150_1_sse_status_handler_t) (void *user_data, int status);

typedef span_timestamp_t (*v150_1_timer_handler_t) (void *user_data, span_timestamp_t timeout);

typedef struct v150_1_state_s v150_1_state_t;

/* The information packet types are:

    V150_1_MSGID_I_RAW_OCTET        (mandatory)
    V150_1_MSGID_I_RAW_BIT          (optional)

    These messages allow for the transport of synchronous data streams. _RAW_OCTET is for data which is
    a whole number of octets in length. _RAW_BIT is for an arbitrary number of bits. _RAW_BIT should not
    be used when there is a whole number of octets in the data blocks.

    V150_1_MSGID_I_OCTET            (mandatory)
    V150_1_MSGID_I_OCTET_CS         (optional)

    These messages allow for the transport of error corrected data. The version with _CS include a character
    sequence number, so data gaps can be detected when using an unreliable channel. The version without _CS
    has no sequence number field, but can optionally contain a DLCI field.

    V150_1_MSGID_I_CHAR_STAT        (optional)
    V150_1_MSGID_I_CHAR_DYN         (optional)
    V150_1_MSGID_I_CHAR_STAT_CS     (optional)
    V150_1_MSGID_I_CHAR_DYN_CS      (optional)
    
    These messages allow for the transport of start-stop (i.e. start bit and stop bit framed) characters, with
    different formats. The _STAT versions are for data which does not change format during a session (even though
    the format is explicit in each packet). The _DYN versions have exactly the same format as the _STAT versions,
    but the format fields can vary from packet to packet. The versions with _CS include a character sequence
    number, so data gaps can be detected when using an unreliable channel. Otherwise they are similar to the
    versions without _CS.

    V150_1_MSGID_I_FRAME            (optional)
    
    This message is used to send data for a framed protocol, like HDLC, preserving the frame boundaries as the
    data passes through the V.150.1 link. This assumes that the original frames are never bigger than the current
    maximum allowed for a V.150.1 packet.
 */

#define V150_1_CALL_DISCRIMINATION_DEFAULT_TIMEOUT              60000000

/* Indeterminate is the initial state before the correct value has been determined. The other values are from
   Table 32/V.150.1 and E.1.5, although the names don't quite match. VBD_SELECT seems to equate to VBD_PREFERRED. */
typedef enum v150_1_cdscselect_e
{
    V150_1_CDSCSELECT_INDETERMINATE                             = 0,
    V150_1_CDSCSELECT_AUDIO_RFC4733                             = 1,
    V150_1_CDSCSELECT_VBD_PREFERRED                             = 2,
    V150_1_CDSCSELECT_MIXED                                     = 3
} v150_1_cdscselect_t;

typedef enum v150_1_modem_relay_gateway_type_e
{
    V150_1_MODEM_RELAY_GATEWAY_V8                               = 0,            /*  V-MR */
    V150_1_MODEM_RELAY_GATEWAY_UNIVERSAL                        = 1             /*  U-MR */
} v150_1_modem_relay_gateway_type_t;

enum v150_1_msgid_e
{
    V150_1_MSGID_NULL                                           = 0,            /* Transport channel N/A */
    V150_1_MSGID_INIT                                           = 1,            /* Transport channel 2 */
    V150_1_MSGID_XID_XCHG                                       = 2,            /* Transport channel 2 */
    V150_1_MSGID_JM_INFO                                        = 3,            /* Transport channel 2 */
    V150_1_MSGID_START_JM                                       = 4,            /* Transport channel 2 */
    V150_1_MSGID_CONNECT                                        = 5,            /* Transport channel 2 */
    V150_1_MSGID_BREAK                                          = 6,            /* Transport channel N/A */
    V150_1_MSGID_BREAKACK                                       = 7,            /* Transport channel N/A */
    V150_1_MSGID_MR_EVENT                                       = 8,            /* Transport channel 2 */
    V150_1_MSGID_CLEARDOWN                                      = 9,            /* Transport channel 2 */
    V150_1_MSGID_PROF_XCHG                                      = 10,           /* Transport channel 2 */
    /* Reserved     11-15 */
    V150_1_MSGID_I_RAW_OCTET                                    = 16,           /* Transport channel 1 or 3 */
    V150_1_MSGID_I_RAW_BIT                                      = 17,           /* Transport channel 1 or 3 */
    V150_1_MSGID_I_OCTET                                        = 18,           /* Transport channel 1 or 3 */
    V150_1_MSGID_I_CHAR_STAT                                    = 19,           /* Transport channel 1 or 3 */
    V150_1_MSGID_I_CHAR_DYN                                     = 20,           /* Transport channel 1 or 3 */
    V150_1_MSGID_I_FRAME                                        = 21,           /* Transport channel 1 or 3 */
    V150_1_MSGID_I_OCTET_CS                                     = 22,           /* Transport channel 1 or 3 (only makes sense for 3) */
    V150_1_MSGID_I_CHAR_STAT_CS                                 = 23,           /* Transport channel 1 or 3 (only makes sense for 3) */
    V150_1_MSGID_I_CHAR_DYN_CS                                  = 24,           /* Transport channel 1 or 3 (only makes sense for 3) */
    /* Reserved     25-99 */
    V150_1_MSGID_VENDOR_MIN                                     = 100,          /* N/A */
    V150_1_MSGID_VENDOR_MAX                                     = 127           /* N/A */
};

enum v150_1_support_e
{
    V150_1_SUPPORT_I_RAW_BIT                                    = 0x0800,
    V150_1_SUPPORT_I_FRAME                                      = 0x0400,
    V150_1_SUPPORT_I_CHAR_STAT                                  = 0x0200,
    V150_1_SUPPORT_I_CHAR_DYN                                   = 0x0100,
    V150_1_SUPPORT_I_OCTET_CS                                   = 0x0080,       /* See V.150.1 Amendment 2 */
    V150_1_SUPPORT_I_CHAR_STAT_CS                               = 0x0040,       /* See V.150.1 Amendment 2 */
    V150_1_SUPPORT_I_CHAR_DYN_CS                                = 0x0020        /* See V.150.1 Amendment 2 */
};

enum v150_1_jm_category_id_e
{
    V150_1_JM_CATEGORY_ID_PROTOCOLS                             = 0x5,
    V150_1_JM_CATEGORY_ID_CALL_FUNCTION_1                       = 0x8,
    V150_1_JM_CATEGORY_ID_MODULATION_MODES                      = 0xA,
    V150_1_JM_CATEGORY_ID_PSTN_ACCESS                           = 0xB,
    V150_1_JM_CATEGORY_ID_PCM_MODEM_AVAILABILITY                = 0xE,
    V150_1_JM_CATEGORY_ID_EXTENSION                             = 0x0
};

enum v150_1_jm_call_function_e
{
    V150_1_JM_CALL_FUNCTION_T30_TX                              = (0x1 << 9),
    V150_1_JM_CALL_FUNCTION_V18                                 = (0x2 << 9),
    V150_1_JM_CALL_FUNCTION_V_SERIES                            = (0x3 << 9),
    V150_1_JM_CALL_FUNCTION_H324                                = (0x4 << 9),
    V150_1_JM_CALL_FUNCTION_T30_RX                              = (0x5 << 9),
    V150_1_JM_CALL_FUNCTION_T101                                = (0x6 << 9)
};

enum v150_1_jm_modulation_mode_e
{
    V150_1_JM_MODULATION_MODE_V34_AVAILABLE                     = 0x800,
    V150_1_JM_MODULATION_MODE_V34_HALF_DUPLEX_AVAILABLE         = 0x400,
    V150_1_JM_MODULATION_MODE_V32_V32bis_AVAILABLE              = 0x200,
    V150_1_JM_MODULATION_MODE_V22_V22bis_AVAILABLE              = 0x100,
    V150_1_JM_MODULATION_MODE_V17_AVAILABLE                     = 0x080,
    V150_1_JM_MODULATION_MODE_V29_AVAILABLE                     = 0x040,
    V150_1_JM_MODULATION_MODE_V27ter_AVAILABLE                  = 0x020,
    V150_1_JM_MODULATION_MODE_V26ter_AVAILABLE                  = 0x010,
    V150_1_JM_MODULATION_MODE_V26bis_AVAILABLE                  = 0x008,
    V150_1_JM_MODULATION_MODE_V23_AVAILABLE                     = 0x004,
    V150_1_JM_MODULATION_MODE_V23_HALF_DUPLEX_AVAILABLE         = 0x002,
    V150_1_JM_MODULATION_MODE_V21_AVAILABLE                     = 0x001
};

enum v150_1_jm_protocol_e
{
    V150_1_JM_PROTOCOL_V42_LAPM                                 = (0x4 << 9)
};

enum v150_1_jm_access_e
{
    V150_1_JM_ACCESS_CALL_DCE_CELLULAR                          = (0x4 << 9),
    V150_1_JM_ACCESS_ANSWER_DCE_CELLULAR                        = (0x2 << 9),
    V150_1_JM_ACCESS_DCE_DIGITAL_NETWORK                        = (0x1 << 9)
};

enum v150_1_jm_pcm_mode_e
{
    V150_1_JM_PCM_V90_V92_ANALOGUE_MODEM_AVAILABLE              = (0x4 << 9),
    V150_1_JM_PCM_V90_V92_DIGITAL_MODEM_AVAILABLE               = (0x2 << 9),
    V150_1_JM_PCM_V91_MODEM_AVAILABLE                           = (0x1 << 9)
};

enum v150_1_selmod_e
{
    V150_1_SELMOD_NULL                                          = 0,
    V150_1_SELMOD_V92                                           = 1,
    V150_1_SELMOD_V91                                           = 2,
    V150_1_SELMOD_V90                                           = 3,
    V150_1_SELMOD_V34                                           = 4,
    V150_1_SELMOD_V32bis                                        = 5,
    V150_1_SELMOD_V32                                           = 6,
    V150_1_SELMOD_V22bis                                        = 7,
    V150_1_SELMOD_V22                                           = 8,
    V150_1_SELMOD_V17                                           = 9,
    V150_1_SELMOD_V29                                           = 10,
    V150_1_SELMOD_V27ter                                        = 11,
    V150_1_SELMOD_V26ter                                        = 12,
    V150_1_SELMOD_V26bis                                        = 13,
    V150_1_SELMOD_V23                                           = 14,
    V150_1_SELMOD_V21                                           = 15,
    V150_1_SELMOD_BELL212                                       = 16,
    V150_1_SELMOD_BELL103                                       = 17,
    V150_1_SELMOD_VENDOR_MIN                                    = 18,
    V150_1_SELMOD_VENDOR_MAX                                    = 30
};

enum v150_1_symbol_rate_e
{
    V150_1_SYMBOL_RATE_NULL                                     = 0,
    V150_1_SYMBOL_RATE_600                                      = 1,
    V150_1_SYMBOL_RATE_1200                                     = 2,
    V150_1_SYMBOL_RATE_1600                                     = 3,
    V150_1_SYMBOL_RATE_2400                                     = 4,
    V150_1_SYMBOL_RATE_2743                                     = 5,
    V150_1_SYMBOL_RATE_3000                                     = 6,
    V150_1_SYMBOL_RATE_3200                                     = 7,
    V150_1_SYMBOL_RATE_3429                                     = 8,
    V150_1_SYMBOL_RATE_8000                                     = 9
};

enum v150_1_compress_e
{
    V150_1_COMPRESS_NEITHER_WAY                                 = 0,
    V150_1_COMPRESS_TX_ONLY                                     = 1,
    V150_1_COMPRESS_RX_ONLY                                     = 2,
    V150_1_COMPRESS_BIDIRECTIONAL                               = 3
};

enum v150_1_compression_e
{
    V150_1_COMPRESSION_NONE                                     = 0,
    V150_1_COMPRESSION_V42BIS                                   = 1,
    V150_1_COMPRESSION_V44                                      = 2,
    V150_1_COMPRESSION_MNP5                                     = 3
};

enum v150_1_error_correction_e
{
    V150_1_ERROR_CORRECTION_NONE                                = 0,
    V150_1_ERROR_CORRECTION_V42_LAPM                            = 1,
    V150_1_ERROR_CORRECTION_V42_ANNEX_A                         = 2     /* Annex A is no longer in V.42, so this should be obsolete */
};

enum v150_1_break_source_e
{
    V150_1_BREAK_SOURCE_V42_LAPM                                = 0,
    V150_1_BREAK_SOURCE_V42_ANNEX_A                             = 1,    /* Annex A is no longer in V.42, so this should be obsolete */
    V150_1_BREAK_SOURCE_V14                                     = 2
};

enum v150_1_break_type_e
{
    V150_1_BREAK_TYPE_NOT_APPLICABLE                            = 0,
    V150_1_BREAK_TYPE_DESTRUCTIVE_EXPEDITED                     = 1,
    V150_1_BREAK_TYPE_NON_DESTRUCTIVE_EXPEDITED                 = 2,
    V150_1_BREAK_TYPE_NON_DESTRUCTIVE_NON_EXPEDITED             = 3
};

enum v150_1_mr_event_id_e
{
    V150_1_MR_EVENT_ID_NULL                                     = 0,
    V150_1_MR_EVENT_ID_RATE_RENEGOTIATION                       = 1,
    V150_1_MR_EVENT_ID_RETRAIN                                  = 2,
    V150_1_MR_EVENT_ID_PHYSUP                                   = 3
};

enum v150_1_mr_event_reason_e
{
    V150_1_MR_EVENT_REASON_NULL                                 = 0,
    V150_1_MR_EVENT_REASON_INITIATION                           = 1,
    V150_1_MR_EVENT_REASON_RESPONDING                           = 2
};

/* The cleardown reasons here should match the ones for SSE */
enum v150_1_cleardown_reason_e
{
    V150_1_CLEARDOWN_REASON_UNKNOWN                             = 0,
    V150_1_CLEARDOWN_REASON_PHYSICAL_LAYER_RELEASE              = 1,    /* Data pump release */
    V150_1_CLEARDOWN_REASON_LINK_LAYER_DISCONNECT               = 2,    /* Receiving a V.42 DISC */
    V150_1_CLEARDOWN_REASON_DATA_COMPRESSION_DISCONNECT         = 3,
    V150_1_CLEARDOWN_REASON_ABORT                               = 4,    /* As specified in SDL */
    V150_1_CLEARDOWN_REASON_ON_HOOK                             = 5,    /* Gateway receives on-hook from an end-point */
    V150_1_CLEARDOWN_REASON_NETWORK_LAYER_TERMINATION           = 6,
    V150_1_CLEARDOWN_REASON_ADMINISTRATIVE                      = 7     /* Operator action at gateway */
};

enum v150_1_data_bits_e
{
    V150_1_DATA_BITS_5                                          = 0,
    V150_1_DATA_BITS_6                                          = 1,
    V150_1_DATA_BITS_7                                          = 2,
    V150_1_DATA_BITS_8                                          = 3
};

enum v150_1_parity_e
{
    V150_1_PARITY_UNKNOWN                                       = 0,
    V150_1_PARITY_NONE                                          = 1,
    V150_1_PARITY_EVEN                                          = 2,
    V150_1_PARITY_ODD                                           = 3,
    V150_1_PARITY_SPACE                                         = 4,
    V150_1_PARITY_MARK                                          = 5
    /* Values 6 and 7 are reserved */
};

enum v150_1_stop_bits_e
{
    V150_1_STOP_BITS_1                                          = 0,
    V150_1_STOP_BITS_2                                          = 1
    /* Values 2 and 3 are reserved */
};

enum v150_1_state_e
{
    V150_1_STATE_IDLE                                           = 0,
    V150_1_STATE_INITED                                         = 1,
    /* RETRAIN means the modem has detected a poor quality connection and is retraining. */
    V150_1_STATE_RETRAIN                                        = 2,
    /* RATE_RENEGOTIATION means the modem is trying to reneogiate the physical layer. */
    V150_1_STATE_RATE_RENEGOTIATION                             = 3,
    /* PHYSUP means the modem-to-modem link has been established. It does NOT mean
       an end to end connection has been established, as this state occurs before any
       error correction or compression has been negotiated. */
    V150_1_STATE_PHYSUP                                         = 4,
    /* CONNECTED means a full end to end link has been established, and data may be sent
       and received. */
    V150_1_STATE_CONNECTED                                      = 5
};

/* Table C.1/V.150.1 plus amendments */
enum v150_1_media_states_e
{
    V150_1_MEDIA_STATE_ITU_RESERVED_0                           = 0,        /* Reserved for future use by ITU-T */
    V150_1_MEDIA_STATE_INITIAL_AUDIO                            = 1,        /* Initial Audio */
    V150_1_MEDIA_STATE_VOICE_BAND_DATA                          = 2,        /* Voice Band Data (VBD) */
    V150_1_MEDIA_STATE_MODEM_RELAY                              = 3,        /* Modem Relay */
    V150_1_MEDIA_STATE_FAX_RELAY                                = 4,        /* Fax Relay */
    V150_1_MEDIA_STATE_TEXT_RELAY                               = 5,        /* Text Relay */
    V150_1_MEDIA_STATE_TEXT_PROBE                               = 6,        /* Text Probe (Amendment 2) */
    V150_1_MEDIA_STATE_ITU_RESERVED_MIN                         = 7,        /* Start of ITU reserved range */
    V150_1_MEDIA_STATE_ITU_RESERVED_MAX                         = 31,       /* End of ITU reserved range */
    V150_1_MEDIA_STATE_RESERVED_MIN                             = 32,       /* Start of vendor defined reserved range */
    V150_1_MEDIA_STATE_RESERVED_MAX                             = 63,       /* End of vendor defined reserved range */
    V150_1_MEDIA_STATE_INDETERMINATE                            = 64        /* Indeterminate */
};

/* Definitions for the mrmods field used in the SDP which controls V.150.1 */
enum v150_1_mrmods_e
{
    V150_1_MRMODS_V34                                           = 1,
    V150_1_MRMODS_V34_HALF_DUPLEX                               = 2,
    V150_1_MRMODS_V32BIS                                        = 3,
    V150_1_MRMODS_V22BIS                                        = 4,
    V150_1_MRMODS_V17                                           = 5,
    V150_1_MRMODS_V29_HALF_DUPLEX                               = 6,
    V150_1_MRMODS_V27TER                                        = 7,
    V150_1_MRMODS_V26TER                                        = 8,
    V150_1_MRMODS_V26BIS                                        = 9,
    V150_1_MRMODS_V23_DUPLEX                                    = 10,
    V150_1_MRMODS_V23_HALF_DUPLEX                               = 11,
    V150_1_MRMODS_V21                                           = 12,
    V150_1_MRMODS_V90_ANALOGUE                                  = 13,
    V150_1_MRMODS_V90_DIGITAL                                   = 14,
    V150_1_MRMODS_V91                                           = 15,
    V150_1_MRMODS_V92_ANALOGUE                                  = 16,
    V150_1_MRMODS_V92_DIGITAL                                   = 17
};

enum v150_1_status_reason_e
{
    V150_1_STATUS_REASON_NULL                                   = 0,
    V150_1_STATUS_REASON_MEDIA_STATE_CHANGED                    = 1,
    V150_1_STATUS_REASON_CONNECTION_STATE_CHANGED               = 2,
    V150_1_STATUS_REASON_DATA_FORMAT_CHANGED                    = 3,
    V150_1_STATUS_REASON_BREAK_RECEIVED                         = 4,
    V150_1_STATUS_REASON_RATE_RETRAIN_RECEIVED                  = 5,
    V150_1_STATUS_REASON_RATE_RENEGOTIATION_RECEIVED            = 6,
    V150_1_STATUS_REASON_BUSY_CHANGED                           = 7,
    V150_1_STATUS_REASON_CONNECTION_STATE_PHYSUP                = 8,
    V150_1_STATUS_REASON_CONNECTION_STATE_CONNECTED             = 9
};

#if defined(__cplusplus)
extern "C" {
#endif

SPAN_DECLARE(int) v150_1_state_machine(v150_1_state_t *s, int signal, const uint8_t *msg, int len);

SPAN_DECLARE(const char *) v150_1_msg_id_to_str(int msg_id);

SPAN_DECLARE(const char *) v150_1_data_bits_to_str(int code);

SPAN_DECLARE(const char *) v150_1_parity_to_str(int code);

SPAN_DECLARE(const char *) v150_1_stop_bits_to_str(int code);

SPAN_DECLARE(const char *) v150_1_mr_event_type_to_str(int type);

SPAN_DECLARE(const char *) v150_1_cleardown_reason_to_str(int type);

SPAN_DECLARE(const char *) v150_1_symbol_rate_to_str(int code);

SPAN_DECLARE(const char *) v150_1_modulation_to_str(int modulation);

SPAN_DECLARE(const char *) v150_1_compression_to_str(int compression);

SPAN_DECLARE(const char *) v150_1_compression_direction_to_str(int direction);

SPAN_DECLARE(const char *) v150_1_error_correction_to_str(int correction);

SPAN_DECLARE(const char *) v150_1_break_source_to_str(int source);

SPAN_DECLARE(const char *) v150_1_break_type_to_str(int type);

SPAN_DECLARE(const char *) v150_1_state_to_str(int state);

SPAN_DECLARE(const char *) v150_1_status_reason_to_str(int status);

SPAN_DECLARE(const char *) v150_1_jm_category_to_str(int category);

SPAN_DECLARE(const char *) v150_1_jm_info_modulation_to_str(int modulation);

SPAN_DECLARE(const char *) v150_1_signal_to_str(int modulation);

SPAN_DECLARE(const char *) v150_1_media_state_to_str(int modulation);

SPAN_DECLARE(int) v150_1_set_parity(v150_1_state_t *s, int mode);

SPAN_DECLARE(int) v150_1_set_stop_bits(v150_1_state_t *s, int bits);

SPAN_DECLARE(int) v150_1_set_bits_per_character(v150_1_state_t *s, int bits);

SPAN_DECLARE(int) v150_1_tx_null(v150_1_state_t *s);

SPAN_DECLARE(int) v150_1_tx_init(v150_1_state_t *s);

SPAN_DECLARE(int) v150_1_tx_xid_xchg(v150_1_state_t *s);

SPAN_DECLARE(int) v150_1_tx_jm_info(v150_1_state_t *s);

SPAN_DECLARE(int) v150_1_tx_start_jm(v150_1_state_t *s);

SPAN_DECLARE(int) v150_1_tx_connect(v150_1_state_t *s);

SPAN_DECLARE(int) v150_1_tx_break(v150_1_state_t *s, int source, int type, int duration);

SPAN_DECLARE(int) v150_1_tx_break_ack(v150_1_state_t *s);

SPAN_DECLARE(int) v150_1_tx_mr_event(v150_1_state_t *s, int event_id);

SPAN_DECLARE(int) v150_1_tx_cleardown(v150_1_state_t *s, int reason);

SPAN_DECLARE(int) v150_1_tx_prof_xchg(v150_1_state_t *s);

SPAN_DECLARE(int) v150_1_tx_info_stream(v150_1_state_t *s, const uint8_t buf[], int len);

SPAN_DECLARE(int) v150_1_process_rx_msg(v150_1_state_t *s, int chan, int seq_no, const uint8_t buf[], int len);

SPAN_DECLARE(int) v150_1_set_local_tc_payload_bytes(v150_1_state_t *s, int channel, int max_len);

SPAN_DECLARE(int) v150_1_get_local_tc_payload_bytes(v150_1_state_t *s, int channel);

SPAN_DECLARE(int) v150_1_set_info_stream_tx_mode(v150_1_state_t *s, int channel, int msg_id);

SPAN_DECLARE(int) v150_1_set_info_stream_msg_priorities(v150_1_state_t *s, int msg_ids[]);

SPAN_DECLARE(int) v150_1_set_local_busy(v150_1_state_t *s, bool busy);

SPAN_DECLARE(bool) v150_1_get_far_busy_status(v150_1_state_t *s);

SPAN_DECLARE(int) v150_1_set_modulation(v150_1_state_t *s, int modulation);

SPAN_DECLARE(int) v150_1_set_compression_direction(v150_1_state_t *s, int compression_direction);

SPAN_DECLARE(int) v150_1_set_compression(v150_1_state_t *s, int compression);

SPAN_DECLARE(int) v150_1_set_compression_parameters(v150_1_state_t *s,
                                                    int tx_dictionary_size,
                                                    int rx_dictionary_size,
                                                    int tx_string_length,
                                                    int rx_string_length,
                                                    int tx_history_size,
                                                    int rx_history_size);

SPAN_DECLARE(int) v150_1_set_error_correction(v150_1_state_t *s, int error_correction);

SPAN_DECLARE(int) v150_1_set_tx_symbol_rate(v150_1_state_t *s, bool enable, int rate);

SPAN_DECLARE(int) v150_1_set_rx_symbol_rate(v150_1_state_t *s, bool enable, int rate);

SPAN_DECLARE(int) v150_1_set_tx_data_signalling_rate(v150_1_state_t *s, int rate);

SPAN_DECLARE(int) v150_1_set_rx_data_signalling_rate(v150_1_state_t *s, int rate);

SPAN_DECLARE(void) v150_1_set_near_cdscselect(v150_1_state_t *s, v150_1_cdscselect_t select);

SPAN_DECLARE(void) v150_1_set_far_cdscselect(v150_1_state_t *s, v150_1_cdscselect_t select);

SPAN_DECLARE(void) v150_1_set_near_modem_relay_gateway_type(v150_1_state_t *s, v150_1_modem_relay_gateway_type_t type);

SPAN_DECLARE(void) v150_1_set_far_modem_relay_gateway_type(v150_1_state_t *s, v150_1_modem_relay_gateway_type_t type);

SPAN_DECLARE(void) v150_1_set_rfc4733_mode(v150_1_state_t *s, bool rfc4733_preferred);

SPAN_DECLARE(void) v150_1_set_call_discrimination_timeout(v150_1_state_t *s, int timeout);

SPAN_DECLARE(int) v150_1_timer_expired(v150_1_state_t *s, span_timestamp_t now);

SPAN_DECLARE(logging_state_t *) v150_1_get_logging_state(v150_1_state_t *s);

SPAN_DECLARE(int) v150_1_test_rx_sprt_msg(v150_1_state_t *s, int chan, int seq_no, const uint8_t buf[], int len);

/*! Initialise a V.150.1 context. This must be called before the first use of the context, to
    initialise its contents.
    \brief Initialise a V.150.1 context.
    \param s The V.150.1 context.
    \param sprt_tx_packet_handler Callback routine to handle the transmission of SPRT packets.
    \param sprt_tx_packet_handler_user_data An opaque pointer, passed in calls to the SPRT packet tx handler
    \param sprt_tx_payload_type The payload type for transmitted SPRT packets.
    \param sprt_rx_payload_type The payload type expected in received SPRT packets.
    \param sse_tx_packet_handler Callback routine to handle the transmission of SSE packets.
    \param sse_tx_packet_user_data An opaque pointer, passed in calls to the SSE tx packet handler
    \param v150_1_timer_handler Callback routine to control SPRT, SSE and overall V.150.1 timers.
    \param v150_1_timer_user_data An opaque pointer, passed in calls to the timer handler
    \param rx_data_handler Callback routine to handle the octet stream from an SPRT interaction
    \param rx_data_handler_user_data An opaque pointer, passed in calls to the rx octet handler.
    \param rx_status_report_handler Callback routine for V.150.1 protocol status reports
    \param rx_status_report_user_data An opaque pointer, passed in calls to the rx status report handler
    \param spe_signal_handler
    \param spe_signal_handler_user_data
    \return A pointer to the V.150.1 context, or NULL if there was a problem. */
SPAN_DECLARE(v150_1_state_t *) v150_1_init(v150_1_state_t *s,
                                           sprt_tx_packet_handler_t sprt_tx_packet_handler,
                                           void *sprt_tx_packet_handler_user_data,
                                           uint8_t sprt_tx_payload_type,
                                           uint8_t sprt_rx_payload_type,
                                           v150_1_sse_tx_packet_handler_t sse_tx_packet_handler,
                                           void *sse_tx_packet_user_data,
                                           v150_1_timer_handler_t v150_1_timer_handler,
                                           void *v150_1_timer_user_data,
                                           v150_1_rx_data_handler_t rx_data_handler,
                                           void *rx_data_handler_user_data,
                                           v150_1_rx_status_report_handler_t rx_status_report_handler,
                                           void *rx_status_report_user_data,
                                           v150_1_spe_signal_handler_t spe_signal_handler,
                                           void *spe_signal_handler_user_data);

SPAN_DECLARE(int) v150_1_release(v150_1_state_t *s);

SPAN_DECLARE(int) v150_1_free(v150_1_state_t *s);

#if defined(__cplusplus)
}
#endif
#endif
/*- End of file ------------------------------------------------------------*/
