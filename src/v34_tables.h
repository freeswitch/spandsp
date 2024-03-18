/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v34_tables.h - ITU V.34 modem tables.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2009 Steve Underwood
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

/* From Table 13/V.34 */
static const int8_t conv_encode_input[8][8] =
{
    { 0,  0,  1,  1,  8,  8,  9,  9},   /* 0 */
    { 3,  2,  2,  3, 11, 10, 10, 11},   /* 1 */
    { 5,  5,  4,  4, 13, 13, 12, 12},   /* 2 */
    { 6,  7,  7,  6, 14, 15, 15, 14},   /* 3 */
    { 8,  8,  9,  9,  0,  0,  1,  1},   /* 4 */
    {11, 10, 10, 11,  3,  2,  2,  3},   /* 5 */
    {13, 13, 12, 12,  5,  5,  4,  4},   /* 6 */
    {14, 15, 15, 14,  6,  7,  7,  6}    /* 7 */
};

#if 0
static const uint8_t v34_conv16_decode_table[16][16] =
{
    {0x00, 0x12, 0x64, 0x76},
    {0x10, 0x02, 0x74, 0x66},
    {0x20, 0x32, 0x44, 0x56},
    {0x30, 0x22, 0x54, 0x46},
    {0x40, 0x52, 0x24, 0x36},
    {0x50, 0x42, 0x34, 0x26},
    {0x60, 0x72, 0x04, 0x16},
    {0x70, 0x62, 0x14, 0x06},
    {0x49, 0x5B, 0x2D, 0x3F},
    {0x59, 0x4B, 0x3D, 0x2F},
    {0x69, 0x7B, 0x0D, 0x1F},
    {0x79, 0x6B, 0x1D, 0x0F},
    {0x09, 0x1B, 0x6D, 0x7F},
    {0x19, 0x0B, 0x7D, 0x6F},
    {0x29, 0x3B, 0x4D, 0x5F},
    {0x39, 0x2B, 0x5D, 0x4F}
};
#endif

enum
{
    V34_BAUD_RATE_2400 = 0,
    V34_BAUD_RATE_2743 = 1,
    V34_BAUD_RATE_2800 = 2,
    V34_BAUD_RATE_3000 = 3,
    V34_BAUD_RATE_3200 = 4,
    V34_BAUD_RATE_3429 = 5
};

enum
{
    V34_TRELLIS_16 = 0,
    V34_TRELLIS_32 = 1,
    V34_TRELLIS_64 = 2,
    V34_TRELLIS_RESERVED = 3
};

enum tx_clock_source_e
{
    TX_CLOCK_SOURCE_INTERNAL = 0,
    TX_CLOCK_SOURCE_SYNCED_TO_RX = 1,
    TX_CLOCK_SOURCE_EXTERNAL = 2,
    TX_CLOCK_SOURCE_RESERVED_FOR_ITU_T = 3
};

/* From Table 8/V.34 and Table 10/V.34 */
typedef struct
{
    /* The number of bits in a high mapping frame */
    uint8_t b;
    /* The minimum and expanded values for M */
    uint8_t m[2];
} mapping_t;

static const mapping_t mappings_2400[] =
{
    { 8, { 1,  1}},     /*  2400bps */
    { 9, { 1,  1}},     /*  2600bps */
    {16, { 2,  2}},     /*  4800bps */
    {17, { 2,  2}},     /*  5000bps */
    {24, { 3,  4}},     /*  7200bps */
    {25, { 4,  4}},     /*  7400bps */
    {32, { 6,  7}},     /*  9600bps */
    {33, { 7,  8}},     /*  9800bps */
    {40, {12, 14}},     /* 12000bps */
    {41, {13, 15}},     /* 12200bps */
    {48, {12, 14}},     /* 14400bps */
    {49, {13, 15}},     /* 14600bps */
    {56, {12, 14}},     /* 16800bps */
    {57, {13, 15}},     /* 17000bps */
    {64, {12, 14}},     /* 19200bps */
    {65, {13, 15}},     /* 19400bps */
    {72, {12, 14}},     /* 21600bps */
    {73, {13, 15}},     /* 21800bps */
    { 0, { 0,  0}},     /* 24000bps - invalid */
    { 0, { 0,  0}},     /* 24200bps - invalid */
    { 0, { 0,  0}},     /* 26400bps - invalid */
    { 0, { 0,  0}},     /* 26600bps - invalid */
    { 0, { 0,  0}},     /* 28800bps - invalid */
    { 0, { 0,  0}},     /* 29000bps - invalid */
    { 0, { 0,  0}},     /* 31200bps - invalid */
    { 0, { 0,  0}},     /* 31400bps - invalid */
    { 0, { 0,  0}},     /* 33600bps - invalid */
    { 0, { 0,  0}}      /* 33800bps - invalid */
};

static const mapping_t mappings_2743[] =
{
    { 0, { 0,  0}},     /*  2400bps - invalid */
    { 0, { 0,  0}},     /*  2600bps - invalid */
    {14, { 2,  2}},     /*  4800bps */
    {15, { 2,  2}},     /*  5000bps */
    {21, { 3,  3}},     /*  7200bps */
    {22, { 3,  3}},     /*  7400bps */
    {28, { 4,  5}},     /*  9600bps */
    {29, { 5,  5}},     /*  9800bps */
    {35, { 8,  9}},     /* 12000bps */
    {36, { 8, 10}},     /* 12200bps */
    {42, {14, 17}},     /* 14400bps */
    {43, {15, 18}},     /* 14600bps */
    {49, {13, 15}},     /* 16800bps */
    {50, {14, 17}},     /* 17000bps */
    {56, {12, 14}},     /* 19200bps */
    {57, {13, 15}},     /* 19400bps */
    {63, {11, 13}},     /* 21600bps */
    {64, {12, 14}},     /* 21800bps */
    {70, {10, 12}},     /* 24000bps */
    {71, {11, 13}},     /* 24200bps */
    {77, { 9, 11}},     /* 26400bps */
    {78, {10, 12}},     /* 26600bps */
    { 0, { 0,  0}},     /* 28800bps - invalid */
    { 0, { 0,  0}},     /* 29000bps - invalid */
    { 0, { 0,  0}},     /* 31200bps - invalid */
    { 0, { 0,  0}},     /* 31400bps - invalid */
    { 0, { 0,  0}},     /* 33600bps - invalid */
    { 0, { 0,  0}}      /* 33800bps - invalid */
};

static const mapping_t mappings_2800[] =
{
    { 0, { 0,  0}},     /*  2400bps - invalid */
    { 0, { 0,  0}},     /*  2600bps - invalid */
    {14, { 2,  2}},     /*  4800bps */
    {15, { 2,  2}},     /*  5000bps */
    {21, { 3,  3}},     /*  7200bps */
    {22, { 3,  3}},     /*  7400bps */
    {28, { 4,  5}},     /*  9600bps */
    {28, { 4,  5}},     /*  9800bps */
    {35, { 8,  9}},     /* 12000bps */
    {35, { 8,  9}},     /* 12200bps */
    {42, {14, 17}},     /* 14400bps */
    {42, {14, 17}},     /* 14600bps */
    {48, {12, 14}},     /* 16800bps */
    {49, {13, 15}},     /* 17000bps */
    {55, {11, 13}},     /* 19200bps */
    {56, {12, 14}},     /* 19400bps */
    {62, {10, 12}},     /* 21600bps */
    {63, {11, 13}},     /* 21800bps */
    {69, { 9, 11}},     /* 24000bps */
    {70, {10, 12}},     /* 24200bps */
    {76, { 8, 10}},     /* 26400bps */
    {76, { 8, 10}},     /* 26600bps */
    { 0, { 0,  0}},     /* 28800bps - invalid */
    { 0, { 0,  0}},     /* 29000bps - invalid */
    { 0, { 0,  0}},     /* 31200bps - invalid */
    { 0, { 0,  0}},     /* 31400bps - invalid */
    { 0, { 0,  0}},     /* 33600bps - invalid */
    { 0, { 0,  0}}      /* 33800bps - invalid */
};

static const mapping_t mappings_3000[] =
{
    { 0, { 0,  0}},     /*  2400bps - invalid */
    { 0, { 0,  0}},     /*  2600bps - invalid */
    {13, { 2,  2}},     /*  4800bps */
    {14, { 2,  2}},     /*  5000bps */
    {20, { 2,  3}},     /*  7200bps */
    {20, { 2,  3}},     /*  7400bps */
    {26, { 4,  4}},     /*  9600bps */
    {27, { 4,  5}},     /*  9800bps */
    {32, { 6,  7}},     /* 12000bps */
    {33, { 7,  8}},     /* 12200bps */
    {39, {11, 13}},     /* 14400bps */
    {39, {11, 13}},     /* 14600bps */
    {45, { 9, 11}},     /* 16800bps */
    {46, {10, 12}},     /* 17000bps */
    {52, { 8, 10}},     /* 19200bps */
    {52, { 8, 10}},     /* 19400bps */
    {58, {14, 17}},     /* 21600bps */
    {59, {15, 18}},     /* 21800bps */
    {64, {12, 14}},     /* 24000bps */
    {65, {13, 15}},     /* 24200bps */
    {71, {11, 13}},     /* 26400bps */
    {71, {11, 13}},     /* 26600bps */
    {77, { 9, 11}},     /* 28800bps */
    {78, {10, 12}},     /* 29000bps */
    { 0, { 0,  0}},     /* 31200bps - invalid */
    { 0, { 0,  0}},     /* 31400bps - invalid */
    { 0, { 0,  0}},     /* 33600bps - invalid */
    { 0, { 0,  0}}      /* 33800bps - invalid */
};

static const mapping_t mappings_3200[] =
{
    { 0, { 0,  0}},     /*  2400bps - invalid */
    { 0, { 0,  0}},     /*  2600bps - invalid */
    {12, { 1,  1}},     /*  4800bps */
    {13, { 2,  2}},     /*  5000bps */
    {18, { 2,  2}},     /*  7200bps */
    {19, { 2,  2}},     /*  7400bps */
    {24, { 3,  4}},     /*  9600bps */
    {25, { 4,  4}},     /*  9800bps */
    {30, { 5,  6}},     /* 12000bps */
    {31, { 6,  6}},     /* 12200bps */
    {36, { 8, 10}},     /* 14400bps */
    {37, { 9, 11}},     /* 14600bps */
    {42, {14, 17}},     /* 16800bps */
    {43, {15, 18}},     /* 17000bps */
    {48, {12, 14}},     /* 19200bps */
    {49, {13, 15}},     /* 19400bps */
    {54, {10, 12}},     /* 21600bps */
    {55, {11, 13}},     /* 21800bps */
    {60, { 8, 10}},     /* 24000bps */
    {61, { 9, 11}},     /* 24200bps */
    {66, {14, 17}},     /* 26400bps */
    {67, {15, 18}},     /* 26600bps */
    {72, {12, 14}},     /* 28800bps */
    {73, {13, 15}},     /* 29000bps */
    {78, {10, 12}},     /* 31200bps */
    {79, {11, 13}},     /* 31400bps */
    { 0, { 0,  0}},     /* 33600bps - invalid */
    { 0, { 0,  0}}      /* 33800bps - invalid */
};

static const mapping_t mappings_3429[] =
{
    { 0, { 0,  0}},     /*  2400bps - invalid */
    { 0, { 0,  0}},     /*  2600bps - invalid */
    {12, { 1,  1}},     /*  4800bps */
    {12, { 1,  1}},     /*  5000bps */
    {17, { 2,  2}},     /*  7200bps */
    {18, { 2,  2}},     /*  7400bps */
    {23, { 3,  3}},     /*  9600bps */
    {23, { 3,  3}},     /*  9800bps */
    {28, { 4,  5}},     /* 12000bps */
    {29, { 5,  5}},     /* 12200bps */
    {34, { 7,  8}},     /* 14400bps */
    {35, { 8,  9}},     /* 14600bps */
    {40, {12, 14}},     /* 16800bps */
    {40, {12, 14}},     /* 17000bps */
    {45, { 9, 11}},     /* 19200bps */
    {46, {10, 12}},     /* 19400bps */
    {51, {15, 18}},     /* 21600bps */
    {51, {15, 18}},     /* 21800bps */
    {56, {12, 14}},     /* 24000bps */
    {57, {13, 15}},     /* 24200bps */
    {62, {10, 12}},     /* 26400bps */
    {63, {11, 13}},     /* 26600bps */
    {68, { 8, 10}},     /* 28800bps */
    {68, { 8, 10}},     /* 29000bps */
    {73, {13, 15}},     /* 31200bps */
    {74, {14, 17}},     /* 31400bps */
    {79, {11, 13}},     /* 33600bps */
    {79, {11, 13}}      /* 33800bps */
};

/* From Table 1/V.34, Table 2/V.34, Table 7/V.34 and Table 9/V.34 */
typedef struct
{
    /*! Approximate baud rate (i.e. nearest integer value). */
    int baud_rate;
    /*! The internal code for the maximum bit rate (0-26, ((bit_rate/2400) - 1) << 1) */
    int max_bit_rate_code;
    int a;
    int c;
    /*! The numerator of the number of samples per symbol ratio. */
    int samples_per_symbol_numerator;
    /*! The denominator of the number of samples per symbol ratio. */
    int samples_per_symbol_denominator;
    struct
    {
        int d;
        int e;
    } low_high[2];
    int j;
    int p;
    const mapping_t *mappings;
} baud_rate_parameters_t;

static const baud_rate_parameters_t baud_rate_parameters[] =
{
    {2400, (21600/2400 - 1) << 1,  1, 1, 10,  3, {{2, 3}, {3, 4}}, 7, 12, mappings_2400}, /*  2400 baud */
    {2743, (26400/2400 - 1) << 1,  8, 7, 35, 12, {{3, 5}, {2, 3}}, 8, 12, mappings_2743}, /* ~2743 baud */
    {2800, (26400/2400 - 1) << 1,  7, 6, 20,  7, {{3, 5}, {2, 3}}, 7, 14, mappings_2800}, /*  2800 baud */
    {3000, (28800/2400 - 1) << 1,  5, 4,  8,  3, {{3, 5}, {2, 3}}, 7, 15, mappings_3000}, /*  3000 baud */
    {3200, (31200/2400 - 1) << 1,  4, 3,  5,  2, {{4, 7}, {3, 5}}, 7, 16, mappings_3200}, /*  3200 baud */
    {3429, (33600/2400 - 1) << 1, 10, 7,  7,  3, {{4, 7}, {4, 7}}, 8, 15, mappings_3429}  /* ~3429 baud */
};

#if defined(SPANDSP_USE_FIXED_POINT)
#define PP_SYMBOL_SCALE(x)      ((int16_t) (32767.0f*x + ((x >= 0.0)  ?  0.5  :  -0.5)))
#else
#define PP_SYMBOL_SCALE(x)      (x)
#endif

#if defined(SPANDSP_USE_FIXED_POINT)
#define LINE_PROBE_SCALE(x)     ((int16_t) (x + ((x >= 0.0)  ?  0.5  :  -0.5)))
#else
#define LINE_PROBE_SCALE(x)     (x)
#endif

static const uint8_t k_table[16][4] =
{
    {0, 1, 2, 3},
    {2, 3, 0, 1},
    {1, 0, 3, 2},
    {3, 2, 1, 0},
    {4, 5, 6, 7},
    {6, 7, 4, 5},
    {5, 4, 7, 6},
    {7, 6, 5, 4},
    {2, 3, 0, 1},
    {0, 1, 2, 3},
    {3, 2, 1, 0},
    {1, 0, 3, 2},
    {6, 7, 4, 5},
    {4, 5, 6, 7},
    {7, 6, 5, 4},
    {5, 4, 7, 6}
};

/*! V.34/Table A.4 Modem control super-frame categories */
enum
{
    V34_RATER = 0x00,
    V34_RATEU = 0x03,
    V34_PRECODER = 0x05,
    V34_PRECODEU = 0x0A
};

static const v34_capabilities_t v34_capabilities =
{
    {true, true, true, true, true, true},
    {true, true, true, true, true, true},
    true,
    0,
    true,
    TX_CLOCK_SOURCE_INTERNAL,
    false,
    true
};

/*- End of file ------------------------------------------------------------*/
