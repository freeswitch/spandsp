/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/t4_t6_decode.h - definitions for T.4/T.6 fax decoding
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003, 2009 Steve Underwood
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

#if !defined(_SPANDSP_PRIVATE_T4_T6_DECODE_H_)
#define _SPANDSP_PRIVATE_T4_T6_DECODE_H_

/*!
    T.4 1D, T4 2D and T6 decompressor state.
*/
struct t4_t6_decode_state_s
{
    /*! \brief Callback function to write a row of pixels to the image destination. */
    t4_row_write_handler_t row_write_handler;
    /*! \brief Opaque pointer passed to row_write_handler. */
    void *row_write_user_data;

    /*! \brief The type of compression used between the FAX machines. */
    int encoding;
    /*! \brief Width of the current page, in pixels. */
    int image_width;

    /*! \brief Length of the current page, in pixels. */
    int image_length;
    /*! \brief The current number of bytes per row of uncompressed image data. */
    int bytes_per_row;

    /*! \brief The current number of bits in the current encoded row. */
    int row_bits;
    /*! \brief Pointer to the buffer for the current pixel row. */
    uint8_t *row_buf;

    /*! \brief This variable is set if we are treating the current row as a 2D encoded
               one. */
    int row_is_2d;
    /*! \brief The current length of the current row. */
    int row_len;

    /*! \brief Black and white run-lengths for the current row. */
    uint32_t *cur_runs;
    /*! \brief Black and white run-lengths for the reference row. */
    uint32_t *ref_runs;

    /*! \brief This variable is used to count the consecutive EOLS we have seen. If it
               reaches six, this is the end of the image. It is initially set to -1 for
               1D and 2D decoding, as an indicator that we must wait for the first EOL,
               before decoding any image data. */
    int consecutive_eols;

    /*! \brief The reference or starting changing element on the coding line. At the
               start of the coding line, a0 is set on an imaginary white changing element
               situated just before the first element on the line. During the coding of
               the coding line, the position of a0 is defined by the previous coding mode.
               (See T.4/4.2.1.3.2.). */
    int a0;
    /*! \brief The first changing element on the reference line to the right of a0 and of
               opposite colour to a0. */
    int b1;
    /*! \brief The length of the in-progress run of black or white. */
    int run_length;
    /*! \brief 2D horizontal mode control. */
    int black_white;
    /*! \brief True if the current run is black */
    bool in_black;

    /*! \brief The current step into the current row run-lengths buffer. */
    int a_cursor;
    /*! \brief The current step into the reference row run-lengths buffer. */
    int b_cursor;

    /*! \brief Incoming bit buffer for decompression. */
    uint32_t rx_bitstream;
    /*! \brief The number of bits currently in rx_bitstream. */
    int rx_bits;
    /*! \brief The number of bits to be skipped before trying to match the next code word. */
    int rx_skip_bits;

    /*! \brief Decoded pixel stream buffer. */
    uint32_t pixel_stream;
    /*! \brief The number of pixels currently in pixel_stream. */
    int pixels;

    /*! \brief The minimum bits in any row of the current page. For monitoring only. */
    int min_row_bits;
    /*! \brief The maximum bits in any row of the current page. For monitoring only. */
    int max_row_bits;

    /*! \brief The size of the compressed image, in bits. */
    int compressed_image_size;
    /*! \brief The current number of consecutive bad rows. */
    int curr_bad_row_run;
    /*! \brief The longest run of consecutive bad rows seen in the current page. */
    int longest_bad_row_run;
    /*! \brief The total number of bad rows in the current page. */
    int bad_rows;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

#endif
/*- End of file ------------------------------------------------------------*/
