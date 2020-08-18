/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/t42.h - ITU T.42 JPEG for FAX image processing
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2011 Steve Underwood
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

#if !defined(_SPANDSP_PRIVATE_T42_H_)
#define _SPANDSP_PRIVATE_T42_H_

#include <setjmp.h>
#include <jpeglib.h>

struct lab_params_s
{
    /* Lab gamut */
    float range_L;
    float range_a;
    float range_b;
    float offset_L;
    float offset_a;
    float offset_b;
    int ab_are_signed;

    /* Illuminant, forward and reverse */
    float x_n;
    float y_n;
    float z_n;
    float x_rn;
    float y_rn;
    float z_rn;
};

/* State of a working instance of the T.42 JPEG FAX encoder */
struct t42_encode_state_s
{
    /*! \brief Callback function to read a row of pixels from the image source. */
    t4_row_read_handler_t row_read_handler;
    /*! \brief Opaque pointer passed to row_read_handler. */
    void *row_read_user_data;
    uint32_t image_width;
    uint32_t image_length;
    uint16_t samples_per_pixel;
    int image_type;
    int no_subsampling;
    int itu_ycc;
    int quality;

    /* The X or Y direction resolution, in pixels per inch */
    int spatial_resolution;

    lab_params_t lab;

    uint8_t illuminant_code[4];
    int illuminant_colour_temperature;

    /*! \brief The size of the compressed image, in bytes. */
    int compressed_image_size;
    int compressed_image_ptr;

    int buf_size;
    uint8_t *compressed_buf;

    FILE *out;
#if defined(HAVE_OPEN_MEMSTREAM)
    size_t outsize;
#endif
    jmp_buf escape;
    char error_message[JMSG_LENGTH_MAX];
    struct jpeg_compress_struct compressor;

    JSAMPROW scan_line_out;
    JSAMPROW scan_line_in;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

/* State of a working instance of the T.42 JPEG FAX decoder */
struct t42_decode_state_s
{
    /*! A callback routine to handle decoded pixel rows */
    t4_row_write_handler_t row_write_handler;
    /*! An opaque pointer passed to row_write_handler() */
    void *row_write_user_data;
    /*! A callback routine to handle decoded comments */
    t4_row_write_handler_t comment_handler;
    /*! An opaque pointer passed to comment_handler() */
    void *comment_user_data;
    /*! The maximum length of comment to be passed to the comment handler */
    uint32_t max_comment_len;
    uint32_t image_width;
    uint32_t image_length;
    uint16_t samples_per_pixel;
    int image_type;
    int itu_ycc;

    /* The X or Y direction resolution, in pixels per inch */
    int spatial_resolution;

    lab_params_t lab;

    uint8_t illuminant_code[4];
    int illuminant_colour_temperature;

    /*! The contents for a COMMENT marker segment, to be added to the
        image at the next opportunity. This is set to NULL when nothing is
        pending. */
    uint8_t *comment;
    /*! Length of data pointed to by comment */
    size_t comment_len;

    /*! \brief The size of the compressed image, in bytes. */
    int compressed_image_size;

    int buf_size;
    uint8_t *compressed_buf;

    FILE *in;
    jmp_buf escape;
    char error_message[JMSG_LENGTH_MAX];
    struct jpeg_decompress_struct decompressor;

    /*! Flag that the data to be decoded has run out. */
    int end_of_data;

    JSAMPROW scan_line_out;
    JSAMPROW scan_line_in;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

#endif
/*- End of file ------------------------------------------------------------*/
