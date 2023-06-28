/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/v150_1.h
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

struct v150_1_state_s
{
    v150_1_tx_packet_handler_t tx_packet_handler;
    void *tx_packet_user_data;
    v150_1_rx_packet_handler_t rx_packet_handler;
    void *rx_packet_user_data;
    v150_1_rx_octet_handler_t rx_octet_handler;
    void *rx_octet_handler_user_data;
    v150_1_rx_status_report_handler_t rx_status_report_handler;
    void *rx_status_report_user_data;

    struct
    {
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

        int8_t info_msg_preferences[10];

        /* The maximum packet lengths we may generate. These vary with the channel number when using SPRT
           as the transport. So, we hold a length for each SPRT protocol channel ID. */
        int max_payload_bytes[SPRT_CHANNELS];

        /* The channel to be used for info packets */
        uint16_t info_stream_channel;
        /* The message ID to be used for info packets */
        uint16_t info_stream_msg_id;

        bool busy;

        int connection_state;
        int cleardown_reason;
    } near;
    struct
    {
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

        int break_source;
        int break_type;
        int break_duration;

        bool busy;

        int connection_state;
        int cleardown_reason;
    } far;
    int joint_connection_state;
    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

#endif
/*- End of file ------------------------------------------------------------*/
