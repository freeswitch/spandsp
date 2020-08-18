/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/t38_gateway.h - A T.38, less the packet exchange part
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2005, 2006, 2007 Steve Underwood
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

#if !defined(_SPANDSP_PRIVATE_T38_GATEWAY_H_)
#define _SPANDSP_PRIVATE_T38_GATEWAY_H_

/*! The number of HDLC transmit buffers */
#define T38_TX_HDLC_BUFS        256
/*! The maximum length of an HDLC frame buffer. This must be big enough for ECM frames. */
#define T38_MAX_HDLC_LEN        260
/*! The receive buffer length */
#define T38_RX_BUF_LEN          2048

/*!
    T.38 gateway T.38 side channel descriptor.
*/
typedef struct
{
    /*! \brief Core T.38 IFP support */
    t38_core_state_t t38;

    /*! \brief If NSF, NSC, and NSS are to be suppressed by altering their contents to
               something the far end will not recognise, this is the amount to overwrite. */
    int suppress_nsx_len[2];
    /*! \brief If NSF, NSC, and NSS are to be suppressed by altering their contents to
               something the far end will not recognise, this is the string to use for overwriting. */
    uint8_t suppress_nsx_string[2][MAX_NSX_SUPPRESSION];

    /*! \brief True if we need to corrupt the HDLC frame in progress, so the receiver cannot
               interpret it. The two values are for the two directions. */
    bool corrupt_current_frame[2];

    /*! \brief the current class of field being received - i.e. none, non-ECM or HDLC */
    int current_rx_field_class;
    /*! \brief The T.38 indicator currently in use */
    int in_progress_rx_indicator;

    /*! \brief The current T.38 data type being sent. */
    int current_tx_data_type;
} t38_gateway_t38_state_t;

/*!
    T.38 gateway audio side channel descriptor.
*/
typedef struct
{
    /*! \brief The FAX modem set for the audio side fo the gateway. */
    fax_modems_state_t modems;
} t38_gateway_audio_state_t;

/*!
    T.38 gateway T.38 side state.
*/
typedef struct
{
    /*! \brief non-ECM and HDLC modem receive data buffer. */
    uint8_t data[T38_RX_BUF_LEN];
    /*! \brief Current pointer into the data buffer. */
    int data_ptr;
    /*! \brief The current octet being received as non-ECM data. */
    uint16_t bit_stream;
    /*! \brief The number of bits taken from the modem for the current scan row. This
               is used during non-ECM transmission with fill bit removal to see that
               T.38 packet transmissions do not stretch too far apart. */
    int bits_absorbed;
    /*! \brief The current bit number in the current non-ECM octet. */
    int bit_no;
    /*! \brief Progressively calculated CRC for HDLC messages received from a modem. */
    uint16_t crc;
    /*! \brief True if non-ECM fill bits are to be stripped when sending image data. */
    bool fill_bit_removal;
    /*! \brief The number of octets to send in each image packet (non-ECM or ECM) at
               the current rate and the current specified packet interval. */
    int octets_per_data_packet;

    /*! \brief The number of bits into the non-ECM buffer */
    int in_bits;
    /*! \brief The number of octets fed out from the non-ECM buffer */
    int out_octets;
} t38_gateway_to_t38_state_t;

/*!
    T.38 gateway HDLC buffer.
*/
typedef struct
{
    /*! \brief HDLC message buffers. */
    uint8_t buf[T38_MAX_HDLC_LEN];
    /*! \brief HDLC message lengths. */
    int16_t len;
    /*! \brief HDLC message status flags. */
    uint16_t flags;
    /*! \brief HDLC buffer contents. */
    int16_t contents;
} t38_gateway_hdlc_buf_t;

/*!
    T.38 gateway HDLC state.
*/
typedef struct
{
    /*! \brief HDLC message buffers. */
    t38_gateway_hdlc_buf_t buf[T38_TX_HDLC_BUFS];
    /*! \brief HDLC buffer number for input. */
    int in;
    /*! \brief HDLC buffer number for output. */
    int out;
} t38_gateway_hdlc_state_t;

/*!
    T.38 gateway core descriptor.
*/
typedef struct
{
    /*! \brief A bit mask of the currently supported modem types. */
    int supported_modems;
    /*! \brief True if ECM FAX mode is allowed through the gateway. */
    bool ecm_allowed;
    /*! \brief Required time between T.38 transmissions, in ms. */
    int ms_per_tx_chunk;

    /*! \brief True if in image data modem is to use short training. This usually
               follows image_data_mode, but in ECM mode T.30 defines recovery
               conditions in which long training is used for image data. */
    bool short_train;
    /*! \brief True if in image data mode, as opposed to TCF mode. */
    bool image_data_mode;
    /*! \brief The minimum permitted bits per FAX scan line row. */
    int min_row_bits;

    /*! \brief True if we should count the next MCF as a page end, else false */
    bool count_page_on_mcf;
    /*! \brief The number of pages for which a confirm (MCF) message was returned. */
    int pages_confirmed;

    /*! \brief True if we are in error correcting (ECM) mode */
    bool ecm_mode;
    /*! \brief The current bit rate for the fast modem. */
    int fast_bit_rate;
    /*! \brief The current fast receive modem type. */
    int fast_rx_modem;
    /*! \brief The type of fast receive modem currently active, which may be T38_NONE */
    int fast_rx_active;

    /*! \brief The current timed operation. */
    int timed_mode;
    /*! \brief The number of samples until the next timeout event */
    int samples_to_timeout;

    /*! Buffer for HDLC and non-ECM data going to the T.38 channel */
    t38_gateway_to_t38_state_t to_t38;
    /*! Buffer for data going to an HDLC modem. */
    t38_gateway_hdlc_state_t hdlc_to_modem;
    /*! Buffer for data going to a non-ECM mode modem. */
    t38_non_ecm_buffer_state_t non_ecm_to_modem;

    /*! \brief A pointer to a callback routine to be called when frames are
        exchanged. */
    t38_gateway_real_time_frame_handler_t real_time_frame_handler;
    /*! \brief An opaque pointer supplied in real time frame callbacks. */
    void *real_time_frame_user_data;
} t38_gateway_core_state_t;

/*!
    T.38 gateway state.
*/
struct t38_gateway_state_s
{
    /*! T.38 side state */
    t38_gateway_t38_state_t t38x;
    /*! Audio side state */
    t38_gateway_audio_state_t audio;
    /*! T.38 core state */
    t38_gateway_core_state_t core;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

#endif
/*- End of file ------------------------------------------------------------*/
