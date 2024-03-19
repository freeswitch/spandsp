/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v18.c - V.18 text telephony for the deaf.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004-2015 Steve Underwood
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <inttypes.h>
#include <memory.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#else
#include "spandsp/stdbool.h"
#endif
#include "floating_fudge.h"

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/logging.h"
#include "spandsp/fast_convert.h"
#include "spandsp/queue.h"
#include "spandsp/async.h"
#include "spandsp/complex.h"
#include "spandsp/dds.h"
#include "spandsp/tone_detect.h"
#include "spandsp/tone_generate.h"
#include "spandsp/super_tone_rx.h"
#include "spandsp/power_meter.h"
#include "spandsp/fsk.h"
#include "spandsp/dtmf.h"
#include "spandsp/modem_connect_tones.h"
#include "spandsp/v8.h"
#include "spandsp/v18.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/queue.h"
#include "spandsp/private/tone_generate.h"
#include "spandsp/private/async.h"
#include "spandsp/private/power_meter.h"
#include "spandsp/private/fsk.h"
#include "spandsp/private/dtmf.h"
#include "spandsp/private/modem_connect_tones.h"
#include "spandsp/private/v18.h"

#include <stdlib.h>

/*
    Ways in which a V.18 call may start
    -----------------------------------

    Originate:
        ANS
            Silence for 0.5s then send TXP
        DTMF
            Proceed as Annex B
        1650Hz (V21 ch 2 low) [1650Hz +-12Hz]
            Proceed as Annex F in call mode
        1300Hz (V.25 Calling tone) [1300Hz +-16Hz]
            Proceed as Annex E in call mode
        1400Hz/1800Hz (Weitbrecht) [1400Hz +-5% and 1800Hz +-5%]
            Detect rate and proceed as Annex A
        980Hz/1180Hz (V21 ch 1) [980Hz +-12Hz, 1180Hz +-12Hz]
            Start timer Tr
        2225Hz (Bell ANS)
            Proceed as Annex D call mode
        1270Hz (Bell103 ch 2 high)
            Proceed as Annex D answer mode
        390Hz (V23 ch 2 low)
            Proceed as Annex E answer mode

    Answer:
        ANS
            Monitor as caller for 980Hz or 1300Hz
        CI/XCI
            Respond with ANSam
        1300Hz (V.25 Calling tone) [1300Hz +-16Hz]
            Probe
        Timer Ta (3s)
            Probe
        1400Hz/1800Hz (Weitbrecht) [1400Hz +-5% and 1800Hz +-5%]
            Detect rate and proceed as Annex A
        DTMF
            Proceed as Annex B
        980Hz (V21 ch 1 low) [980Hz +-12Hz]
            Start timer Te
        1270Hz (Bell103 ch 2 high)
            Proceed as Annex D answer mode
        2225Hz (Bell ANS)
            Proceed as Annex D call mode
        1650Hz (V21 ch 2 low) [1650Hz +-12Hz]
            Proceed as Annex F answer mode
        ANSam
            Proceed as V.8 caller Annex G
*/

/*
    Automoding

When originating the DCE should send:

    Silence 1 s
    CI 400 ms
    Silence 2 s
    CI 400 ms
    Silence 2 s
    CI 400 ms
    Silence 2 s
    XCI 3 s
    Silence 1 s
    CI 400 ms
    Silence 2 s
    etc

and prepare to detect:

    390Hz (only when sending XCI).
    980Hz or 1180Hz (Note: take care to avoid detecting echos of the CI signal)
    1270Hz
    1300Hz (V.25 calling tone)
    1650Hz
    1400Hz or 1800Hz
    2100Hz (ANSam) as defined in ITU-T V.8
    2100Hz (ANS) as defined in ITU-T V.25
    2225Hz (Bell 103 answer tone)
    DTMF tones


When answering the DCE should prepare to detect:

    V.23 high-band signals
    980Hz or 1180Hz
    1300Hz (V.25 Calling tone)
    1400Hz or 1800Hz
    signal CI
    1270Hz
    1650Hz
    2100Hz (ANS)
    2100Hz (ANSam) as defined in ITU-T V.8
    2225Hz
    DTMF tones
*/

#define GOERTZEL_SAMPLES_PER_BLOCK  102

#if defined(SPANDSP_USE_FIXED_POINTx)
/* The fixed point version scales the 16 bit signal down by 7 bits, so the Goertzels will fit in a 32 bit word */
#define FP_SCALE(x)                         ((int16_t) (x/128.0 + ((x >= 0.0)  ?  0.5  :  -0.5)))
#if defined(SPANDSP_USE_INTRINSICS_IN_INITIALIZERS)
static const float tone_to_total_energy     = GOERTZEL_SAMPLES_PER_BLOCK*db_to_power_ratio(-0.85f);
#else
static const float tone_to_total_energy     = 83.868f           /* -0.85dB */
#endif
#else
#define FP_SCALE(x)                         (x)
#if defined(SPANDSP_USE_INTRINSICS_IN_INITIALIZERS)
static const float tone_to_total_energy     = GOERTZEL_SAMPLES_PER_BLOCK*db_to_power_ratio(-0.85f);
#else
static const float tone_to_total_energy     = 83.868f;          /* -0.85dB */
#endif
#endif

static void v18_set_modem(v18_state_t *s, int mode);

static goertzel_descriptor_t tone_set_desc[GOERTZEL_TONE_SET_ENTRIES];
static bool goertzel_descriptors_inited = false;
static const float tone_set_frequency[GOERTZEL_TONE_SET_ENTRIES] =
{
    390.0f,     // V.23 low channel
    980.0f,
    1180.0f,
    1270.0f,
    1300.0f,    // (V.25 Calling tone)
    1400.0f,
    1650.0f,
    1800.0f,
    2225.0f     // Bell 103 answer tone
};
static const span_sample_timer_t tone_set_target_duration[GOERTZEL_TONE_SET_ENTRIES] =
{
    milliseconds_to_samples(3000),      /* 390Hz */
    milliseconds_to_samples(1500),      /* 980Hz */
    0,                                  /* 1180Hz */
    milliseconds_to_samples(700),       /* 1270Hz */
    milliseconds_to_samples(1700),      /* 1300Hz */
    0,                                  /* 1400Hz */
    milliseconds_to_samples(460),       /* 1650Hz */
    0,                                  /* 1800Hz */
    milliseconds_to_samples(460)        /* 2225Hz */
};

static const bool tone_set_enabled[2][GOERTZEL_TONE_SET_ENTRIES] =
{
    {
        true,                           /* 390Hz */
        true,                           /* 980Hz */
        true,                           /* 1180Hz */
        true,                           /* 1270Hz */
        false,                          /* 1300Hz */
        true,                           /* 1400Hz */
        false,                          /* 1650Hz */
        true,                           /* 1800Hz */
        true                            /* 2225Hz */
    },
    {
        true,                           /* 390Hz */
        true,                           /* 980Hz */
        true,                           /* 1180Hz */
        true,                           /* 1270Hz */
        true,                           /* 1300Hz */
        true,                           /* 1400Hz */
        true,                           /* 1650Hz */
        true,                           /* 1800Hz */
        false                           /* 2225Hz */
    }
};

/*! The baudot code to shift from alpha to digits and symbols */
#define BAUDOT_FIGURE_SHIFT     0x1B
/*! The baudot code to shift from digits and symbols to alpha */
#define BAUDOT_LETTER_SHIFT     0x1F

struct dtmf_to_ascii_s
{
    const char *dtmf;
    char ascii;
};

static const struct dtmf_to_ascii_s dtmf_to_ascii[] =
{
    {"###0", '!'},
    {"###1", 'C'},
    {"###2", 'F'},
    {"###3", 'I'},
    {"###4", 'L'},
    {"###5", 'O'},
    {"###6", 'R'},
    {"###7", 'U'},
    {"###8", 'X'},
    {"###9", ';'},
    {"##*1", 'A'},
    {"##*2", 'D'},
    {"##*3", 'G'},
    {"##*4", 'J'},
    {"##*5", 'M'},
    {"##*6", 'P'},
    {"##*7", 'S'},
    {"##*8", 'V'},
    {"##*9", 'Y'},
    {"##1", 'B'},
    {"##2", 'E'},
    {"##3", 'H'},
    {"##4", 'K'},
    {"##5", 'N'},
    {"##6", 'Q'},
    {"##7", 'T'},
    {"##8", 'W'},
    {"##9", 'Z'},
    {"##0", ' '},
#if defined(WIN32)  ||  (defined(__SVR4)  &&  defined (__sun))
    {"#*1", 'X'},   // (Note 1) 111 1011
    {"#*2", 'X'},   // (Note 1) 111 1100
    {"#*3", 'X'},   // (Note 1) 111 1101
    {"#*4", 'X'},   // (Note 1) 101 1011
    {"#*5", 'X'},   // (Note 1) 101 1100
    {"#*6", 'X'},   // (Note 1) 101 1101
#else
    {"#*1", 0xE6},  // (Note 1) 111 1011 (UTF-8 C3 86)
    {"#*2", 0xF8},  // (Note 1) 111 1100 (UTF-8 C3 98)
    {"#*3", 0xE5},  // (Note 1) 111 1101 (UTF-8 C3 85)
    {"#*4", 0xC6},  // (Note 1) 101 1011 (UTF-8 C3 A6)
    {"#*5", 0xD8},  // (Note 1) 101 1100 (UTF-8 C3 B8)
    {"#*6", 0xC5},  // (Note 1) 101 1101 (UTF-8 C3 A5)
#endif
    {"#0", '?'},
    {"#1", 'c'},
    {"#2", 'f'},
    {"#3", 'i'},
    {"#4", 'l'},
    {"#5", 'o'},
    {"#6", 'r'},
    {"#7", 'u'},
    {"#8", 'x'},
    {"#9", '.'},
    {"*#0", '0'},
    {"*#1", '1'},
    {"*#2", '2'},
    {"*#3", '3'},
    {"*#4", '4'},
    {"*#5", '5'},
    {"*#6", '6'},
    {"*#7", '7'},
    {"*#8", '8'},
    {"*#9", '9'},
    {"**1", '+'},
    {"**2", '-'},
    {"**3", '='},
    {"**4", ':'},
    {"**5", '%'},
    {"**6", '('},
    {"**7", ')'},
    {"**8", ','},
    {"**9", '\n'},
    {"*0", '\b'},
    {"*1", 'a'},
    {"*2", 'd'},
    {"*3", 'g'},
    {"*4", 'j'},
    {"*5", 'm'},
    {"*6", 'p'},
    {"*7", 's'},
    {"*8", 'v'},
    {"*9", 'y'},
    {"0", ' '},
    {"1", 'b'},
    {"2", 'e'},
    {"3", 'h'},
    {"4", 'k'},
    {"5", 'n'},
    {"6", 'q'},
    {"7", 't'},
    {"8", 'w'},
    {"9", 'z'},
    {"", '\0'}
};

static const char *ascii_to_dtmf[128] =
{
    "",         /* NULL */
    "",         /* SOH */
    "",         /* STX */
    "",         /* ETX */
    "",         /* EOT */
    "",         /* ENQ */
    "",         /* ACK */
    "",         /* BEL */
    "*0",       /* BACK SPACE */
    "0",        /* HT >> SPACE */
    "**9",      /* LF */
    "**9",      /* VT >> LF */
    "**9",      /* FF >> LF */
    "",         /* CR */
    "",         /* SO */
    "",         /* SI */
    "",         /* DLE */
    "",         /* DC1 */
    "",         /* DC2 */
    "",         /* DC3 */
    "",         /* DC4 */
    "",         /* NAK */
    "",         /* SYN */
    "",         /* ETB */
    "",         /* CAN */
    "",         /* EM */
    "#0",       /* SUB >> ? */
    "",         /* ESC */
    "**9",      /* IS4 >> LF */
    "**9",      /* IS3 >> LF */
    "**9",      /* IS2 >> LF */
    "0",        /* IS1 >> SPACE */
    "0",        /* SPACE */
    "###0",     /* ! */
    "",         /* " */
    "",         /* # */
    "",         /* $ */
    "**5",      /* % */
    "**1",      /* & >> + */
    "",         /* ' */
    "**6",      /* ( */
    "**7",      /* ) */
    "#9",       /* _ >> . */
    "**1",      /* + */
    "**8",      /* , */
    "**2",      /* - */
    "#9",       /* . */
    "",         /* / */
    "*#0",      /* 0 */
    "*#1",      /* 1 */
    "*#2",      /* 2 */
    "*#3",      /* 3 */
    "*#4",      /* 4 */
    "*#5",      /* 5 */
    "*#6",      /* 6 */
    "*#7",      /* 7 */
    "*#8",      /* 8 */
    "*#9",      /* 9 */
    "**4",      /* : */
    "###9",     /* ; */
    "**6",      /* < >> ( */
    "**3",      /* = */
    "**7",      /* > >> ) */
    "#0",       /* ? */
    "###8",     /* @ >> X */
    "##*1",     /* A */
    "##1",      /* B */
    "###1",     /* C */
    "##*2",     /* D */
    "##2",      /* E */
    "###2",     /* F */
    "##*3",     /* G */
    "##3",      /* H */
    "###3",     /* I */
    "##*4",     /* J */
    "##4",      /* K */
    "###4",     /* L */
    "##*5",     /* M */
    "##5",      /* N */
    "###5",     /* O */
    "##*6",     /* P */
    "##6",      /* Q */
    "###6",     /* R */
    "##*7",     /* S */
    "##7",      /* T */
    "###7",     /* U */
    "##*8",     /* V */
    "##8",      /* W */
    "###8",     /* X */
    "##*9",     /* Y */
    "##9",      /* Z */
    "#*4",      /* 0xC6 (National code) (UTF-8 C3 86) */
    "#*5",      /* 0xD8 (National code) (UTF-8 C3 98) */
    "#*6",      /* 0xC5 (National code) (UTF-8 C3 85) */
    "",         /* ^ */
    "0",        /* _ >> SPACE */
    "",         /* ` */
    "*1",       /* a */
    "1",        /* b */
    "#1",       /* c */
    "*2",       /* d */
    "2",        /* e */
    "#2",       /* f */
    "*3",       /* g */
    "3",        /* h */
    "#3",       /* i */
    "*4",       /* j */
    "4",        /* k */
    "#4",       /* l */
    "*5",       /* m */
    "5",        /* n */
    "#5",       /* o */
    "*6",       /* p */
    "6",        /* q */
    "#6",       /* r */
    "*7",       /* s */
    "7",        /* t */
    "#7",       /* u */
    "*8",       /* v */
    "8",        /* w */
    "#8",       /* x */
    "*9",       /* y */
    "9",        /* z */
    "#*1",      /* 0xE6 (National code) (UTF-8 C3 A6) */
    "#*2",      /* 0xF8 (National code) (UTF-8 C3 B8) */
    "#*3",      /* 0xE5 (National code) (UTF-8 C3 A5) */
    "0",        /* ~ >> SPACE */
    "*0"        /* DEL >> BACK SPACE */
};

/* XCI is:
    400 ms mark;
    XCI marker;
    800 ms mark;
    XCI marker;
    800 ms mark;
    XCI marker;
    800 ms mark;
    XCI marker;
    100 ms mark. */
static const uint8_t xci[] = "01111111110111111111";

/* The entries here must match the order in which the related names are defined in v18.h */
static const int automoding_sequences[][6] =
{
    {
        /* Global */
        V18_MODE_WEITBRECHT_5BIT_4545,
        V18_MODE_BELL103,
        V18_MODE_V21TEXTPHONE,
        V18_MODE_V23VIDEOTEX,
        V18_MODE_EDT,
        V18_MODE_DTMF
    },
    {
        /* None */
        V18_MODE_WEITBRECHT_5BIT_4545,
        V18_MODE_BELL103,
        V18_MODE_V21TEXTPHONE,
        V18_MODE_V23VIDEOTEX,
        V18_MODE_EDT,
        V18_MODE_DTMF
    },
    {
        /* Australia */
        V18_MODE_WEITBRECHT_5BIT_50,
        V18_MODE_V21TEXTPHONE,
        V18_MODE_V23VIDEOTEX,
        V18_MODE_EDT,
        V18_MODE_DTMF,
        V18_MODE_BELL103
    },
    {
        /* Ireland */
        V18_MODE_WEITBRECHT_5BIT_50,
        V18_MODE_V21TEXTPHONE,
        V18_MODE_V23VIDEOTEX,
        V18_MODE_EDT,
        V18_MODE_DTMF,
        V18_MODE_BELL103
    },
    {
        /* Germany */
        V18_MODE_EDT,
        V18_MODE_V21TEXTPHONE,
        V18_MODE_V23VIDEOTEX,
        V18_MODE_WEITBRECHT_5BIT_50,
        V18_MODE_DTMF,
        V18_MODE_BELL103
    },
    {
        /* Switzerland */
        V18_MODE_EDT,
        V18_MODE_V21TEXTPHONE,
        V18_MODE_V23VIDEOTEX,
        V18_MODE_WEITBRECHT_5BIT_50,
        V18_MODE_DTMF,
        V18_MODE_BELL103
    },
    {
        /* Italy */
        V18_MODE_EDT,
        V18_MODE_V21TEXTPHONE,
        V18_MODE_V23VIDEOTEX,
        V18_MODE_WEITBRECHT_5BIT_50,
        V18_MODE_DTMF,
        V18_MODE_BELL103
    },
    {
        /* Spain */
        V18_MODE_EDT,
        V18_MODE_V21TEXTPHONE,
        V18_MODE_V23VIDEOTEX,
        V18_MODE_WEITBRECHT_5BIT_50,
        V18_MODE_DTMF,
        V18_MODE_BELL103
    },
    {
        /* Austria */
        V18_MODE_EDT,
        V18_MODE_V21TEXTPHONE,
        V18_MODE_V23VIDEOTEX,
        V18_MODE_WEITBRECHT_5BIT_50,
        V18_MODE_DTMF,
        V18_MODE_BELL103
    },
    {
        /* Netherlands */
        V18_MODE_DTMF,
        V18_MODE_V21TEXTPHONE,
        V18_MODE_V23VIDEOTEX,
        V18_MODE_WEITBRECHT_5BIT_50,
        V18_MODE_EDT,
        V18_MODE_BELL103
    },
    {
        /* Iceland */
        V18_MODE_V21TEXTPHONE,
        V18_MODE_DTMF,
        V18_MODE_WEITBRECHT_5BIT_50,
        V18_MODE_EDT,
        V18_MODE_V23VIDEOTEX,
        V18_MODE_BELL103
    },
    {
        /* Norway */
        V18_MODE_V21TEXTPHONE,
        V18_MODE_DTMF,
        V18_MODE_WEITBRECHT_5BIT_50,
        V18_MODE_EDT,
        V18_MODE_V23VIDEOTEX,
        V18_MODE_BELL103
    },
    {
        /* Sweden */
        V18_MODE_V21TEXTPHONE,
        V18_MODE_DTMF,
        V18_MODE_WEITBRECHT_5BIT_50,
        V18_MODE_EDT,
        V18_MODE_V23VIDEOTEX,
        V18_MODE_BELL103
    },
    {
        /* Finland */
        V18_MODE_V21TEXTPHONE,
        V18_MODE_DTMF,
        V18_MODE_WEITBRECHT_5BIT_50,
        V18_MODE_EDT,
        V18_MODE_V23VIDEOTEX,
        V18_MODE_BELL103
    },
    {
        /* Denmark */
        V18_MODE_V21TEXTPHONE,
        V18_MODE_DTMF,
        V18_MODE_WEITBRECHT_5BIT_50,
        V18_MODE_EDT,
        V18_MODE_V23VIDEOTEX,
        V18_MODE_BELL103
    },
    {
        /* UK */
        V18_MODE_V21TEXTPHONE,
        V18_MODE_WEITBRECHT_5BIT_50,
        V18_MODE_V23VIDEOTEX,
        V18_MODE_EDT,
        V18_MODE_DTMF,
        V18_MODE_BELL103
    },
    {
        /* USA */
        V18_MODE_WEITBRECHT_5BIT_4545,
        V18_MODE_BELL103,
        V18_MODE_V21TEXTPHONE,
        V18_MODE_V23VIDEOTEX,
        V18_MODE_EDT,
        V18_MODE_DTMF
    },
    {
        /* France */
        V18_MODE_V23VIDEOTEX,
        V18_MODE_EDT,
        V18_MODE_DTMF,
        V18_MODE_WEITBRECHT_5BIT_50,
        V18_MODE_V21TEXTPHONE,
        V18_MODE_BELL103
    },
    {
        /* Belgium */
        V18_MODE_V23VIDEOTEX,
        V18_MODE_EDT,
        V18_MODE_DTMF,
        V18_MODE_WEITBRECHT_5BIT_50,
        V18_MODE_V21TEXTPHONE,
        V18_MODE_BELL103
    }
};

SPAN_DECLARE(const char *) v18_status_to_str(int status)
{
    switch (status)
    {
    case V18_STATUS_SWITCH_TO_NONE:
        return "Switched to None mode";
    case V18_STATUS_SWITCH_TO_WEITBRECHT_5BIT_4545:
        return "Switched to Weitbrecht TDD (45.45bps) mode";
    case V18_STATUS_SWITCH_TO_WEITBRECHT_5BIT_476:
        return "Switched to Weitbrecht TDD (47.6bps) mode";
    case V18_STATUS_SWITCH_TO_WEITBRECHT_5BIT_50:
        return "Switched to Weitbrecht TDD (50bps) mode";
    case V18_STATUS_SWITCH_TO_DTMF:
        return "Switched to DTMF mode";
    case V18_STATUS_SWITCH_TO_EDT:
        return "Switched to EDT mode";
    case V18_STATUS_SWITCH_TO_BELL103:
        return "Switched to Bell 103 mode";
    case V18_STATUS_SWITCH_TO_V23VIDEOTEX:
        return "Switched to V.23 Videotex mode";
    case V18_STATUS_SWITCH_TO_V21TEXTPHONE:
        return "Switched to V.21 mode";
    case V18_STATUS_SWITCH_TO_V18TEXTPHONE:
        return "Switched to V.18 text telephone mode";
    }
    /*endswitch*/
    return "???";
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) v18_mode_to_str(int mode)
{
    switch ((mode & 0xFFF))
    {
    case V18_MODE_NONE:
        return "None";
    case V18_MODE_WEITBRECHT_5BIT_4545:
        return "Weitbrecht TDD (45.45bps)";
    case V18_MODE_WEITBRECHT_5BIT_476:
        return "Weitbrecht TDD (47.6bps)";
    case V18_MODE_WEITBRECHT_5BIT_50:
        return "Weitbrecht TDD (50bps)";
    case V18_MODE_DTMF:
        return "DTMF";
    case V18_MODE_EDT:
        return "EDT";
    case V18_MODE_BELL103:
        return "Bell 103";
    case V18_MODE_V23VIDEOTEX:
        return "V.23 Videotex";
    case V18_MODE_V21TEXTPHONE:
        return "V.21";
    case V18_MODE_V18TEXTPHONE:
        return "V.18 text telephone";
    }
    /*endswitch*/
    return "???";
}
/*- End of function --------------------------------------------------------*/

static const char *v18_tone_to_str(int tone)
{
    switch (tone)
    {
    case GOERTZEL_TONE_SET_390HZ:
        return "390Hz tone";
    case GOERTZEL_TONE_SET_980HZ:
        return "980Hz tone";
    case GOERTZEL_TONE_SET_1180HZ:
        return "1180Hz tone";
    case GOERTZEL_TONE_SET_1270HZ:
        return "1270Hz tone";
    case GOERTZEL_TONE_SET_1300HZ:
        return "1300Hz tone";
    case GOERTZEL_TONE_SET_1400HZ:
        return "1400Hz tone";
    case GOERTZEL_TONE_SET_1650HZ:
        return "1650Hz tone";
    case GOERTZEL_TONE_SET_1800HZ:
        return "1800Hz tone";
    case GOERTZEL_TONE_SET_2225HZ:
        return "2225Hz tone";
    }
    /*endswitch*/
    return "???";
}
/*- End of function --------------------------------------------------------*/

static uint16_t encode_baudot(v18_state_t *s, uint8_t ch)
{
    static const uint8_t conv[128] =
    {
        0xFF, /* NUL */
        0xFF, /* SOH */
        0xFF, /* STX */
        0xFF, /* ETX */
        0xFF, /* EOT */
        0xFF, /* ENQ */
        0xFF, /* ACK */
        0xFF, /* BEL */
        0x40, /* BS */
        0x44, /* HT >> SPACE */
        0x42, /* LF */
        0x42, /* VT >> LF */
        0x42, /* FF >> LF */
        0x48, /* CR */
        0xFF, /* SO */
        0xFF, /* SI */
        0xFF, /* DLE */
        0xFF, /* DC1 */
        0xFF, /* DC2 */
        0xFF, /* DC3 */
        0xFF, /* DC4 */
        0xFF, /* NAK */
        0xFF, /* SYN */
        0xFF, /* ETB */
        0xFF, /* CAN */
        0xFF, /* EM */
        0x99, /* SUB >> ? */
        0xFF, /* ESC */
        0x42, /* IS4 >> LF */
        0x42, /* IS3 >> LF */
        0x42, /* IS2 >> LF */
        0x44, /* IS1 >> SPACE */
        0x44, /*   */
        0x8D, /* ! */
        0x91, /* " */
        0x89, /* # >> $ */
        0x89, /* $ */
        0x9D, /* % >> / */
        0x9A, /* & >> + */
        0x8B, /* ' */
        0x8F, /* ( */
        0x92, /* ) */
        0x9C, /* * >> . */
        0x9A, /* + */
        0x8C, /* , */
        0x83, /* - */
        0x9C, /* . */
        0x9D, /* / */
        0x96, /* 0 */
        0x97, /* 1 */
        0x93, /* 2 */
        0x81, /* 3 */
        0x8A, /* 4 */
        0x90, /* 5 */
        0x95, /* 6 */
        0x87, /* 7 */
        0x86, /* 8 */
        0x98, /* 9 */
        0x8E, /* : */
        0x9E, /* ; */
        0x8F, /* < >> )*/
        0x94, /* = */
        0x92, /* > >> ( */
        0x99, /* ? */
        0x1D, /* @ >> X */
        0x03, /* A */
        0x19, /* B */
        0x0E, /* C */
        0x09, /* D */
        0x01, /* E */
        0x0D, /* F */
        0x1A, /* G */
        0x14, /* H */
        0x06, /* I */
        0x0B, /* J */
        0x0F, /* K */
        0x12, /* L */
        0x1C, /* M */
        0x0C, /* N */
        0x18, /* O */
        0x16, /* P */
        0x17, /* Q */
        0x0A, /* R */
        0x05, /* S */
        0x10, /* T */
        0x07, /* U */
        0x1E, /* V */
        0x13, /* W */
        0x1D, /* X */
        0x15, /* Y */
        0x11, /* Z */
        0x8F, /* [ >> (*/
        0x9D, /* \ >> / */
        0x92, /* ] >> ) */
        0x8B, /* ^ >> ' */
        0x44, /* _ >> Space */
        0x8B, /* ` >> ' */
        0x03, /* a */
        0x19, /* b */
        0x0E, /* c */
        0x09, /* d */
        0x01, /* e */
        0x0D, /* f */
        0x1A, /* g */
        0x14, /* h */
        0x06, /* i */
        0x0B, /* j */
        0x0F, /* k */
        0x12, /* l */
        0x1C, /* m */
        0x0C, /* n */
        0x18, /* o */
        0x16, /* p */
        0x17, /* q */
        0x0A, /* r */
        0x05, /* s */
        0x10, /* t */
        0x07, /* u */
        0x1E, /* v */
        0x13, /* w */
        0x1D, /* x */
        0x15, /* y */
        0x11, /* z */
        0x8F, /* { >> ( */
        0x8D, /* | >> ! */
        0x92, /* } >> ) */
        0x44, /* ~ >> Space */
        0xFF, /* DEL */
    };
    uint16_t shift;

    ch = conv[ch & 0x7F];
    /* Is it a non-existant code? */
    if (ch == 0xFF)
        return 0;
    /*endif*/
    /* Is it a code present in both character sets? */
    if ((ch & 0x40))
        return 0x8000 | (ch & 0x1F);
    /*endif*/
    /* Need to allow for a possible character set change. */
    if ((ch & 0x80))
    {
        if (!s->repeat_shifts  &&  s->baudot_tx_shift == 1)
            return ch & 0x1F;
        /*endif*/
        s->baudot_tx_shift = 1;
        shift = BAUDOT_FIGURE_SHIFT;
    }
    else
    {
        if (!s->repeat_shifts  &&  s->baudot_tx_shift == 0)
            return ch & 0x1F;
        /*endif*/
        s->baudot_tx_shift = 0;
        shift = BAUDOT_LETTER_SHIFT;
    }
    /*endif*/
    return 0x8000 | (shift << 5) | (ch & 0x1F);
}
/*- End of function --------------------------------------------------------*/

static uint8_t decode_baudot(v18_state_t *s, uint8_t ch)
{
    static const uint8_t conv[2][32] =
    {
        {"\bE\nA SIU\rDRJNFCKTZLWHYPQOBG^MXV^"},
        {"\b3\n- -87\r$4',!:(5\")2=6019?+^./;^"}
    };

    switch (ch)
    {
    case BAUDOT_FIGURE_SHIFT:
        s->baudot_rx_shift = 1;
        break;
    case BAUDOT_LETTER_SHIFT:
        s->baudot_rx_shift = 0;
        break;
    default:
        return conv[s->baudot_rx_shift][ch];
    }
    /*endswitch*/
    /* Return 0xFF if we did not produce a character */
    return 0xFF;
}
/*- End of function --------------------------------------------------------*/

static int v18_tdd_get_async_byte(void *user_data)
{
    v18_state_t *s;
    int ch;
    uint16_t x;

    s = (v18_state_t *) user_data;

    if (s->next_byte != 0xFF)
    {
        s->rx_suppression_timer = milliseconds_to_samples(300);
        x = s->next_byte;
        s->next_byte = (uint8_t) 0xFF;
        return x;
    }
    /*endif*/
    for (;;)
    {
        if ((ch = queue_read_byte(&s->queue.queue)) < 0)
        {
            if (s->tx_draining)
            {
                /* The FSK should now be switched off. */
                s->tx_draining = false;
                return SIG_STATUS_END_OF_DATA;
            }
            /*endif*/
            /* This should give us 300ms of idling before shutdown. It is
               not exact, and will vary a little with the actual bit rate. */
            span_log(&s->logging, SPAN_LOG_FLOW, "Tx shutdown with delay\n");
            async_tx_presend_bits(&s->async_tx, 14);
            s->tx_draining = true;
            s->rx_suppression_timer = milliseconds_to_samples(300);
            return SIG_STATUS_LINK_IDLE;
        }
        /*endif*/
        if ((x = encode_baudot(s, ch)) != 0)
            break;
        /*endif*/
    }
    /*endfor*/
    s->rx_suppression_timer = milliseconds_to_samples(300);
    if (s->tx_signal_on == 1)
    {
        /* This should give us about 150ms of idling before the first character. It is
           not exact, and will vary a little with the actual bit rate. */
        async_tx_presend_bits(&s->async_tx, 7);
        s->tx_signal_on = 2;
    }
    /*endif*/
    if ((x & 0x3E0))
    {
        s->next_byte = (uint8_t) (x & 0x1F);
        return (uint8_t) ((x >> 5) & 0x1F);
    }
    /*endif*/
    s->next_byte = (uint8_t) 0xFF;
    return (uint8_t) (x & 0x1F);
}
/*- End of function --------------------------------------------------------*/

static void v18_dtmf_get(void *user_data)
{
    v18_state_t *s;
    int ch;
    int len;
    const char *t;

    s = (v18_state_t *) user_data;
    if (s->tx_suppression_timer)
        return;
    /*endif*/
    if ((ch = queue_read_byte(&s->queue.queue)) < 0)
        return;
    /*endif*/
    if ((ch & 0x80))
    {
        /* There are a few characters which mean something above 0x7F, as laid out in
           Table B.1/V.18 and Table B.2/V.18 */
        /* TODO: Make these work as UTF-8, instead of the current 8 bit encoding */
        switch (ch)
        {
        case 0xC6:
            /* UTF-8 C3 86 */
            t = ascii_to_dtmf[0x5B];
            break;
        case 0xD8:
            /* UTF-8 C3 98 */
            t = ascii_to_dtmf[0x5C];
            break;
        case 0xC5:
            /* UTF-8 C3 85 */
            t = ascii_to_dtmf[0x5D];
            break;
        case 0xE6:
            /* UTF-8 C3 A6 */
            t = ascii_to_dtmf[0x7B];
            break;
        case 0xF8:
            /* UTF-8 C3 B8 */
            t = ascii_to_dtmf[0x7C];
            break;
        case 0xE5:
            /* UTF-8 C3 A5 */
            t = ascii_to_dtmf[0x7D];
            break;
        default:
            t = "";
            break;
        }
        /*endswitch*/
    }
    else
    {
        t = ascii_to_dtmf[ch];
    }
    /*endif*/
    len = strlen(t);
    if (len > 0)
    {
        dtmf_tx_put(&s->dtmf_tx, t, len);
        s->rx_suppression_timer = milliseconds_to_samples(300 + 100*len);
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static int v18_edt_get_async_byte(void *user_data)
{
    v18_state_t *s;
    int ch;

    s = (v18_state_t *) user_data;
    if ((ch = queue_read_byte(&s->queue.queue)) >= 0)
    {
        s->rx_suppression_timer = milliseconds_to_samples(300);
        return ch;
    }
    /*endif*/
    /* Nothing to send */
    if (s->tx_signal_on)
    {
        /* The FSK should now be switched off. */
        span_log(&s->logging, SPAN_LOG_FLOW, "Turning off the carrier\n");
        s->tx_signal_on = 0;
    }
    /*endif*/
    return SIG_STATUS_LINK_IDLE;
}
/*- End of function --------------------------------------------------------*/

static int v18_bell103_get_async_byte(void *user_data)
{
    v18_state_t *s;
    int ch;

    s = (v18_state_t *) user_data;
    if ((ch = queue_read_byte(&s->queue.queue)) >= 0)
    {
        return ch;
    }
    /*endif*/
    return SIG_STATUS_LINK_IDLE;
}
/*- End of function --------------------------------------------------------*/

static int v18_videotex_get_async_byte(void *user_data)
{
    v18_state_t *s;
    int ch;

    s = (v18_state_t *) user_data;
    if ((ch = queue_read_byte(&s->queue.queue)) >= 0)
    {
        return ch;
    }
    /*endif*/
    return SIG_STATUS_LINK_IDLE;
}
/*- End of function --------------------------------------------------------*/

static int v18_textphone_get_async_byte(void *user_data)
{
    v18_state_t *s;
    int ch;

    s = (v18_state_t *) user_data;
    if ((ch = queue_read_byte(&s->queue.queue)) >= 0)
    {
        return ch;
    }
    /*endif*/
    return SIG_STATUS_LINK_IDLE;
}
/*- End of function --------------------------------------------------------*/

static void v18_tdd_put_async_byte(void *user_data, int byte)
{
    v18_state_t *s;
    uint8_t octet;

    s = (v18_state_t *) user_data;
    if (byte < 0)
    {
        /* Special conditions */
        span_log(&s->logging, SPAN_LOG_FLOW, "TDD signal status is %s (%d)\n", signal_status_to_str(byte), byte);
        switch (byte)
        {
        case SIG_STATUS_CARRIER_UP:
            s->msg_in_progress_timer = 0;
            s->rx_msg_len = 0;
            break;
        case SIG_STATUS_CARRIER_DOWN:
            if (s->rx_msg_len > 0)
            {
                /* Whatever we have to date constitutes the message */
                s->rx_msg[s->rx_msg_len] = '\0';
                if (s->put_msg)
                    s->put_msg(s->put_msg_user_data, s->rx_msg, s->rx_msg_len);
                /*endif*/
                s->rx_msg_len = 0;
            }
            /*endif*/
            break;
        default:
            span_log(&s->logging, SPAN_LOG_WARNING, "Unexpected special put byte value - %d!\n", byte);
            break;
        }
        /*endswitch*/
        return;
    }
    /*endif*/
    if (s->rx_suppression_timer > 0)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Rx suppressed byte 0x%02x (%d)\n", byte, s->rx_suppression_timer);
        return;
    }
    /*endif*/
    span_log(&s->logging, SPAN_LOG_FLOW, "Rx byte 0x%02x\n", byte);
    if ((octet = decode_baudot(s, byte)) != 0xFF)
    {
        s->rx_msg[s->rx_msg_len++] = octet;
        span_log(&s->logging, SPAN_LOG_FLOW, "Rx byte 0x%02x '%c'\n", octet, octet);
    }
    /*endif*/
    if (s->rx_msg_len > 0)
    {
        s->rx_msg[s->rx_msg_len] = '\0';
        if (s->put_msg)
            s->put_msg(s->put_msg_user_data, s->rx_msg, s->rx_msg_len);
        /*endif*/
        s->rx_msg_len = 0;
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static int decode_dtmf_cmp(const void *s, const void *t)
{
    const char *ss;
    struct dtmf_to_ascii_s *tt;

    ss = (const char *) s;
    tt = (struct dtmf_to_ascii_s *) t;
    return strncmp(ss, tt->dtmf, strlen(tt->dtmf));
}
/*- End of function --------------------------------------------------------*/

static int decode_dtmf(v18_state_t *s, char msg[], const char dtmf[])
{
    int entries;
    int len;
    const char *t;
    char *u;
    struct dtmf_to_ascii_s *ss;

    entries = sizeof(dtmf_to_ascii)/sizeof(dtmf_to_ascii[0]) - 1;
    t = dtmf;
    u = msg;
    while (*t)
    {
        ss = bsearch(t, dtmf_to_ascii, entries, sizeof(dtmf_to_ascii[0]), decode_dtmf_cmp);
        if (ss)
        {
            len = strlen(ss->dtmf);
            *u++ = ss->ascii;
            return len;
        }
        /*endif*/
        /* Can't match the code. Let's assume this is a code we just don't know, and skip over it */
        while (*t == '#'  ||  *t == '*')
            t++;
        /*endwhile*/
        if (*t)
            t++;
        /*endif*/
    }
    /*endwhile*/
    *u = '\0';
    return u - msg;
}
/*- End of function --------------------------------------------------------*/

static void v18_dtmf_put(void *user_data, const char dtmf[], int len)
{
    v18_state_t *s;
    char buf[128];
    int i;
    int matched;

    s = (v18_state_t *) user_data;
    if (s->current_mode != V18_MODE_DTMF)
    {
        /* We must have received DTMF while in automoding */
        s->status_handler(s->status_handler_user_data, V18_STATUS_SWITCH_TO_DTMF);
        v18_set_modem(s, V18_MODE_DTMF);
    }
    /*endif*/
    s->tx_suppression_timer = milliseconds_to_samples(400);
    if (s->rx_suppression_timer > 0)
        return;
    /*endif*/
    for (i = 0;  i < len;  i++)
    {
        s->rx_msg[s->rx_msg_len++] = dtmf[i];
        if (dtmf[i] >= '0'  &&  dtmf[i] <= '9')
        {
            s->rx_msg[s->rx_msg_len] = '\0';
            if ((matched = decode_dtmf(s, buf, (const char *) s->rx_msg)) > 0)
            {
                buf[1] = '\0';
                s->put_msg(s->put_msg_user_data, (const uint8_t *) buf, 1);
            }
            /*endif*/
            if (s->rx_msg_len > matched)
                memcpy(&s->rx_msg[0], &s->rx_msg[matched], s->rx_msg_len - matched);
            /*endif*/
            s->rx_msg_len -= matched;
        }
        /*endif*/
    }
    /*endfor*/
    s->msg_in_progress_timer = seconds_to_samples(5);
}
/*- End of function --------------------------------------------------------*/

static void v18_edt_put_async_byte(void *user_data, int byte)
{
    v18_state_t *s;
    s = (v18_state_t *) user_data;
    if (s->rx_suppression_timer > 0)
        return;
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static void v18_bell103_put_async_byte(void *user_data, int byte)
{
    v18_state_t *s;
    s = (v18_state_t *) user_data;
    if (s->rx_suppression_timer > 0)
        return;
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static void v18_videotex_put_async_byte(void *user_data, int byte)
{
    v18_state_t *s;
    s = (v18_state_t *) user_data;
    if (s->rx_suppression_timer > 0)
        return;
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static void v18_textphone_put_async_byte(void *user_data, int byte)
{
    v18_state_t *s;
    s = (v18_state_t *) user_data;
    if (s->rx_suppression_timer > 0)
        return;
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static int v18_txp_get_bit(void *user_data)
{
    /* TXP is:
        A break (10 1's)
        A start stop framed 0xD4
        A start stop framed 0xD8
        A start stop framed 0x50
        Repeated */
    static const uint8_t txp[] = "1111111111000101011100001101110000010101";
    int bit;

    v18_state_t *s;
    s = (v18_state_t *) user_data;
    bit = (txp[s->txp_cnt] == '1')  ?  1  :  0;
    s->txp_cnt++;
    if (s->txp_cnt >= 40)
        s->txp_cnt = 0;
    /*endif*/
    return bit;
}
/*- End of function --------------------------------------------------------*/

static void v18_set_modem(v18_state_t *s, int mode)
{
    int tx_modem;
    int rx_modem;

    switch (mode)
    {
    case V18_MODE_WEITBRECHT_5BIT_4545:
        s->repeat_shifts = (mode & V18_MODE_REPETITIVE_SHIFTS_OPTION) != 0;
        fsk_tx_init(&s->fsk_tx, &preset_fsk_specs[FSK_WEITBRECHT_4545], async_tx_get_bit, &s->async_tx);
        async_tx_init(&s->async_tx, 5, ASYNC_PARITY_NONE, 2, false, v18_tdd_get_async_byte, s);
        /* Schedule an explicit shift at the start of baudot transmission */
        s->baudot_tx_shift = 2;
        /* TDD uses 5 bit data, no parity and 1.5 stop bits. We scan for the first stop bit, and
           ride over the fraction. */
        fsk_rx_init(&s->fsk_rx,
                    &preset_fsk_specs[FSK_WEITBRECHT_4545],
                    FSK_FRAME_MODE_FRAMED,
                    v18_tdd_put_async_byte,
                    s);
        fsk_rx_set_frame_parameters(&s->fsk_rx, 5, ASYNC_PARITY_NONE, 2);
        s->baudot_rx_shift = 0;
        s->next_byte = (uint8_t) 0xFF;
        break;
    case V18_MODE_WEITBRECHT_5BIT_476:
        s->repeat_shifts = (mode & V18_MODE_REPETITIVE_SHIFTS_OPTION) != 0;
        fsk_tx_init(&s->fsk_tx, &preset_fsk_specs[FSK_WEITBRECHT_476], async_tx_get_bit, &s->async_tx);
        async_tx_init(&s->async_tx, 5, ASYNC_PARITY_NONE, 2, false, v18_tdd_get_async_byte, s);
        /* Schedule an explicit shift at the start of baudot transmission */
        s->baudot_tx_shift = 2;
        /* TDD uses 5 bit data, no parity and 1.5 stop bits. We scan for the first stop bit, and
           ride over the fraction. */
        fsk_rx_init(&s->fsk_rx,
                    &preset_fsk_specs[FSK_WEITBRECHT_476],
                    FSK_FRAME_MODE_FRAMED,
                    v18_tdd_put_async_byte,
                    s);
        fsk_rx_set_frame_parameters(&s->fsk_rx, 5, ASYNC_PARITY_NONE, 2);
        s->baudot_rx_shift = 0;
        s->next_byte = (uint8_t) 0xFF;
        break;
    case V18_MODE_WEITBRECHT_5BIT_50:
        s->repeat_shifts = (mode & V18_MODE_REPETITIVE_SHIFTS_OPTION) != 0;
        fsk_tx_init(&s->fsk_tx, &preset_fsk_specs[FSK_WEITBRECHT_50], async_tx_get_bit, &s->async_tx);
        async_tx_init(&s->async_tx, 5, ASYNC_PARITY_NONE, 2, false, v18_tdd_get_async_byte, s);
        /* Schedule an explicit shift at the start of baudot transmission */
        s->baudot_tx_shift = 2;
        /* TDD uses 5 bit data, no parity and 1.5 stop bits. We scan for the first stop bit, and
           ride over the fraction. */
        fsk_rx_init(&s->fsk_rx,
                    &preset_fsk_specs[FSK_WEITBRECHT_50],
                    FSK_FRAME_MODE_FRAMED,
                    v18_tdd_put_async_byte,
                    s);
        fsk_rx_set_frame_parameters(&s->fsk_rx, 5, ASYNC_PARITY_NONE, 2);
        s->baudot_rx_shift = 0;
        s->next_byte = (uint8_t) 0xFF;
        break;
    case V18_MODE_DTMF:
        dtmf_tx_init(&s->dtmf_tx, v18_dtmf_get, s);
        dtmf_rx_init(&s->dtmf_rx, v18_dtmf_put, s);
        break;
    case V18_MODE_EDT:
        fsk_tx_init(&s->fsk_tx, &preset_fsk_specs[FSK_V21CH1_110], async_tx_get_bit, &s->async_tx);
        async_tx_init(&s->async_tx, 7, ASYNC_PARITY_EVEN, 2, false, v18_edt_get_async_byte, s);
        fsk_rx_init(&s->fsk_rx,
                    &preset_fsk_specs[FSK_V21CH1_110],
                    FSK_FRAME_MODE_FRAMED,
                    v18_edt_put_async_byte,
                    s);
        fsk_rx_set_frame_parameters(&s->fsk_rx, 7, ASYNC_PARITY_EVEN, 2);
        break;
    case V18_MODE_BELL103:
        if (s->calling_party)
        {
            tx_modem = FSK_BELL103CH1;
            rx_modem = FSK_BELL103CH2;
        }
        else
        {
            tx_modem = FSK_BELL103CH2;
            rx_modem = FSK_BELL103CH1;
        }
        /*endif*/
        fsk_tx_init(&s->fsk_tx, &preset_fsk_specs[tx_modem], async_tx_get_bit, &s->async_tx);
        async_tx_init(&s->async_tx, 7, ASYNC_PARITY_EVEN, 1, false, v18_bell103_get_async_byte, s);
        fsk_rx_init(&s->fsk_rx,
                    &preset_fsk_specs[rx_modem],
                    FSK_FRAME_MODE_FRAMED,
                    v18_bell103_put_async_byte,
                    s);
        fsk_rx_set_frame_parameters(&s->fsk_rx, 7, ASYNC_PARITY_EVEN, 1);
        span_log(&s->logging, SPAN_LOG_FLOW, "Turning on the carrier\n");
        s->tx_signal_on = 1;
        break;
    case V18_MODE_V23VIDEOTEX:
        if (s->calling_party)
        {
            tx_modem = FSK_V23CH2;
            rx_modem = FSK_V23CH1;
        }
        else
        {
            tx_modem = FSK_V23CH1;
            rx_modem = FSK_V23CH2;
        }
        /*endif*/
        fsk_tx_init(&s->fsk_tx, &preset_fsk_specs[tx_modem], async_tx_get_bit, &s->async_tx);
        async_tx_init(&s->async_tx, 7, ASYNC_PARITY_EVEN, 1, false, v18_videotex_get_async_byte, s);
        fsk_rx_init(&s->fsk_rx,
                    &preset_fsk_specs[rx_modem],
                    FSK_FRAME_MODE_FRAMED,
                    v18_videotex_put_async_byte,
                    s);
        fsk_rx_set_frame_parameters(&s->fsk_rx, 7, ASYNC_PARITY_EVEN, 1);
        span_log(&s->logging, SPAN_LOG_FLOW, "Turning on the carrier\n");
        s->tx_signal_on = 1;
        break;
    case V18_MODE_V21TEXTPHONE:
        if (s->calling_party)
        {
            tx_modem = FSK_V21CH1;
            rx_modem = FSK_V21CH2;
        }
        else
        {
            tx_modem = FSK_V21CH2;
            rx_modem = FSK_V21CH1;
        }
        /*endif*/
#if 1
        fsk_tx_init(&s->fsk_tx, &preset_fsk_specs[tx_modem], async_tx_get_bit, &s->async_tx);
        async_tx_init(&s->async_tx, 7, ASYNC_PARITY_EVEN, 1, false, v18_textphone_get_async_byte, s);
#else
        fsk_tx_init(&s->fsk_tx, &preset_fsk_specs[tx_modem], v18_txp_get_bit, s);
#endif
        fsk_rx_init(&s->fsk_rx,
                    &preset_fsk_specs[rx_modem],
                    FSK_FRAME_MODE_FRAMED,
                    v18_textphone_put_async_byte,
                    s);
        fsk_rx_set_frame_parameters(&s->fsk_rx, 7, ASYNC_PARITY_EVEN, 1);
        span_log(&s->logging, SPAN_LOG_FLOW, "Turning on the carrier\n");
        s->tx_signal_on = 1;
        break;
    case V18_MODE_V18TEXTPHONE:
        fsk_tx_init(&s->fsk_tx, &preset_fsk_specs[FSK_V21CH1], async_tx_get_bit, &s->async_tx);
        async_tx_init(&s->async_tx, 7, ASYNC_PARITY_EVEN, 1, false, v18_textphone_get_async_byte, s);
        fsk_rx_init(&s->fsk_rx,
                    &preset_fsk_specs[FSK_V21CH1],
                    FSK_FRAME_MODE_FRAMED,
                    v18_textphone_put_async_byte,
                    s);
        fsk_rx_set_frame_parameters(&s->fsk_rx, 7, ASYNC_PARITY_EVEN, 1);
        break;
    }
    /*endswitch*/
    s->current_mode = mode;
}
/*- End of function --------------------------------------------------------*/

static int caller_tone_scan(v18_state_t *s, const int16_t amp[], int samples)
{
#if defined(SPANDSP_USE_FIXED_POINTx)
    int32_t tone_set_energy[GOERTZEL_TONE_SET_ENTRIES];
    int32_t max_energy;
    int16_t xamp;
#else
    float tone_set_energy[GOERTZEL_TONE_SET_ENTRIES];
    float max_energy;
    float xamp;
#endif
    int sample;
    int limit;
    int tone_is;
    int i;
    int j;

    dtmf_rx(&s->dtmf_rx, amp, samples);
    modem_connect_tones_rx(&s->answer_tone_rx, amp, samples);
    for (sample = 0;  sample < samples;  sample = limit)
    {
        /* The block length is optimised to meet the DTMF specs. */
        if ((samples - sample) >= (GOERTZEL_SAMPLES_PER_BLOCK - s->current_goertzel_sample))
            limit = sample + (GOERTZEL_SAMPLES_PER_BLOCK - s->current_goertzel_sample);
        else
            limit = samples;
        /*endif*/
        for (j = sample;  j < limit;  j++)
        {
            xamp = amp[j];
            xamp = goertzel_preadjust_amp(xamp);
#if defined(SPANDSP_USE_FIXED_POINTx)
            s->energy += ((int32_t) xamp*xamp);
#else
            s->energy += xamp*xamp;
#endif
            for (i = 0;  i < GOERTZEL_TONE_SET_ENTRIES;  i++)
            {
                goertzel_samplex(&s->tone_set[i], xamp);
            }
            /*endfor*/
        }
        /*endfor*/
        if (s->tone_duration < INT_MAX - (limit - sample))
            s->tone_duration += (limit - sample);
        /*endif*/
        s->current_goertzel_sample += (limit - sample);
        if (s->current_goertzel_sample < GOERTZEL_SAMPLES_PER_BLOCK)
            continue;
        /*endif*/

        /* We are at the end of a tone detection block */
        max_energy = 0;
        for (i = 0;  i < GOERTZEL_TONE_SET_ENTRIES;  i++)
        {
            tone_set_energy[i] = goertzel_result(&s->tone_set[i]);
            if (tone_set_energy[i] > max_energy)
            {
                max_energy = tone_set_energy[i];
                tone_is = i;
            }
            /*endif*/
        }
        /*endif*/

        /* Basic signal level test */
        /* Fraction of total energy test */
        if (max_energy < s->threshold
            ||
            max_energy <= tone_to_total_energy*s->energy)
        {
            tone_is = 0;
        }
        /*endif*/
        if (tone_is != s->in_tone)
        {
            /* Any change of tone will restart this persistence check. */
            s->target_tone_duration = tone_set_target_duration[tone_is];
            s->tone_duration = 0;
            s->in_tone = tone_is;
        }
        else
        {
            if (s->target_tone_duration  &&  s->tone_duration >= s->target_tone_duration)
            {
                /* We have a confirmed tone */
                span_log(&s->logging, SPAN_LOG_FLOW, "Tone %s (%d) seen\n", v18_tone_to_str(s->in_tone), s->in_tone);
                switch (s->in_tone)
                {
                case GOERTZEL_TONE_SET_390HZ:
                    /* Proceed as Annex E in answer mode */
                    s->status_handler(s->status_handler_user_data, V18_STATUS_SWITCH_TO_V23VIDEOTEX);
                    v18_set_modem(s, V18_MODE_V23VIDEOTEX);
                    break;
                case GOERTZEL_TONE_SET_980HZ:
                    s->status_handler(s->status_handler_user_data, V18_STATUS_SWITCH_TO_V21TEXTPHONE);
                    v18_set_modem(s, V18_MODE_V21TEXTPHONE);
                    break;
                case GOERTZEL_TONE_SET_1180HZ:
                    s->status_handler(s->status_handler_user_data, V18_STATUS_SWITCH_TO_V21TEXTPHONE);
                    v18_set_modem(s, V18_MODE_V21TEXTPHONE);
                    break;
                case GOERTZEL_TONE_SET_1270HZ:
                    /* Proceed as Annex D in answer mode */
                    s->status_handler(s->status_handler_user_data, V18_STATUS_SWITCH_TO_BELL103);
                    v18_set_modem(s, V18_MODE_BELL103);
                    break;
                case GOERTZEL_TONE_SET_1300HZ:
                    s->status_handler(s->status_handler_user_data, V18_STATUS_SWITCH_TO_V23VIDEOTEX);
                    v18_set_modem(s, V18_MODE_V23VIDEOTEX);
                    break;
                case GOERTZEL_TONE_SET_1400HZ:
                case GOERTZEL_TONE_SET_1800HZ:
                    /* Find the bit rate */
                    s->status_handler(s->status_handler_user_data, V18_STATUS_SWITCH_TO_WEITBRECHT_5BIT_476);
                    v18_set_modem(s, V18_MODE_WEITBRECHT_5BIT_476); /* TODO: */
                    break;
                case GOERTZEL_TONE_SET_1650HZ:
                    s->status_handler(s->status_handler_user_data, V18_STATUS_SWITCH_TO_V21TEXTPHONE);
                    v18_set_modem(s, V18_MODE_V21TEXTPHONE);
                    break;
                case GOERTZEL_TONE_SET_2225HZ:
                    /* Proceed as Annex E in caller mode */
                    s->status_handler(s->status_handler_user_data, V18_STATUS_SWITCH_TO_BELL103);
                    v18_set_modem(s, V18_MODE_BELL103);
                    break;
                }
                /*endswitch*/
                s->target_tone_duration = 0;
            }
            /*endif*/
        }
        /*endif*/
        s->energy = FP_SCALE(0.0f);
        s->current_goertzel_sample = 0;
    }
    /*endfor*/
    return samples;
}
/*- End of function --------------------------------------------------------*/

static int answerer_tone_scan(v18_state_t *s, const int16_t amp[], int samples)
{
#if defined(SPANDSP_USE_FIXED_POINTx)
    int32_t tone_set_energy[GOERTZEL_TONE_SET_ENTRIES];
    int32_t max_energy;
    int16_t xamp;
#else
    float tone_set_energy[GOERTZEL_TONE_SET_ENTRIES];
    float max_energy;
    float xamp;
#endif
    int sample;
    int limit;
    int tone_is;
    int i;
    int j;

    dtmf_rx(&s->dtmf_rx, amp, samples);
    modem_connect_tones_rx(&s->answer_tone_rx, amp, samples);
    for (sample = 0;  sample < samples;  sample = limit)
    {
        /* The block length is optimised to meet the DTMF specs. */
        if ((samples - sample) >= (GOERTZEL_SAMPLES_PER_BLOCK - s->current_goertzel_sample))
            limit = sample + (GOERTZEL_SAMPLES_PER_BLOCK - s->current_goertzel_sample);
        else
            limit = samples;
        /*endif*/
        for (j = sample;  j < limit;  j++)
        {
            xamp = amp[j];
            xamp = goertzel_preadjust_amp(xamp);
#if defined(SPANDSP_USE_FIXED_POINTx)
            s->energy += ((int32_t) xamp*xamp);
#else
            s->energy += xamp*xamp;
#endif
            for (i = 0;  i < GOERTZEL_TONE_SET_ENTRIES;  i++)
            {
                goertzel_samplex(&s->tone_set[i], xamp);
            }
            /*endfor*/
        }
        /*endfor*/
        if (s->tone_duration < INT_MAX - (limit - sample))
            s->tone_duration += (limit - sample);
        /*endif*/
        s->current_goertzel_sample += (limit - sample);
        if (s->current_goertzel_sample < GOERTZEL_SAMPLES_PER_BLOCK)
            continue;
        /*endif*/

        /* We are at the end of a tone detection block */
        max_energy = 0;
        for (i = 0;  i < GOERTZEL_TONE_SET_ENTRIES;  i++)
        {
            tone_set_energy[i] = goertzel_result(&s->tone_set[i]);
            if (tone_set_energy[i] > max_energy)
            {
                max_energy = tone_set_energy[i];
                tone_is = i;
            }
            /*endif*/
        }
        /*endif*/

        /* Basic signal level test */
        /* Fraction of total energy test */
        if (max_energy < s->threshold
            ||
            max_energy <= tone_to_total_energy*s->energy)
        {
            tone_is = 0;
        }
        /*endif*/
        if (tone_is != s->in_tone)
        {
            /* Any change of tone will restart this persistence check. */
            s->target_tone_duration = tone_set_target_duration[tone_is];
            s->tone_duration = 0;
            s->in_tone = tone_is;
        }
        else
        {
            if (s->target_tone_duration  &&  s->tone_duration >= s->target_tone_duration)
            {
                /* We have a confirmed tone */
                span_log(&s->logging, SPAN_LOG_FLOW, "Tone %s (%d) seen\n", v18_tone_to_str(s->in_tone), s->in_tone);
                switch (s->in_tone)
                {
                case GOERTZEL_TONE_SET_980HZ:
                    s->status_handler(s->status_handler_user_data, V18_STATUS_SWITCH_TO_V21TEXTPHONE);
                    v18_set_modem(s, V18_MODE_V21TEXTPHONE);
                    break;
                case GOERTZEL_TONE_SET_1180HZ:
                    s->status_handler(s->status_handler_user_data, V18_STATUS_SWITCH_TO_V21TEXTPHONE);
                    v18_set_modem(s, V18_MODE_V21TEXTPHONE);
                    break;
                case GOERTZEL_TONE_SET_1270HZ:
                    /* Proceed as Annex D in answer mode */
                    s->status_handler(s->status_handler_user_data, V18_STATUS_SWITCH_TO_BELL103);
                    v18_set_modem(s, V18_MODE_BELL103);
                    break;
                case GOERTZEL_TONE_SET_1300HZ:
                    s->status_handler(s->status_handler_user_data, V18_STATUS_SWITCH_TO_V23VIDEOTEX);
                    v18_set_modem(s, V18_MODE_V23VIDEOTEX);
                    break;
                case GOERTZEL_TONE_SET_1400HZ:
                case GOERTZEL_TONE_SET_1800HZ:
                    /* Find the bit rate */
                    s->status_handler(s->status_handler_user_data, V18_STATUS_SWITCH_TO_WEITBRECHT_5BIT_476);
                    v18_set_modem(s, V18_MODE_WEITBRECHT_5BIT_476); /* TODO: */
                    break;
                case GOERTZEL_TONE_SET_1650HZ:
                    /* Proceed as Annex F in answer mode */
                    s->status_handler(s->status_handler_user_data, V18_STATUS_SWITCH_TO_V21TEXTPHONE);
                    v18_set_modem(s, V18_MODE_V21TEXTPHONE);
                    break;
                case GOERTZEL_TONE_SET_2225HZ:
                    /* Proceed as Annex D in caller mode */
                    s->status_handler(s->status_handler_user_data, V18_STATUS_SWITCH_TO_BELL103);
                    v18_set_modem(s, V18_MODE_BELL103);
                    break;
                }
                /*endswitch*/
                s->target_tone_duration = 0;
            }
            /*endif*/
        }
        /*endif*/
        s->energy = FP_SCALE(0.0f);
        s->current_goertzel_sample = 0;
    }
    /*endfor*/
    return samples;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v18_tx(v18_state_t *s, int16_t *amp, int max_len)
{
    int len;
    int lenx;

    len = 0;
    if (s->tx_suppression_timer > 0)
    {
        if (s->tx_suppression_timer > max_len)
            s->tx_suppression_timer -= max_len;
        else
            s->tx_suppression_timer = 0;
        /*endif*/
    }
    /*endif*/
    switch (s->tx_state)
    {
    case V18_TX_STATE_ORIGINATING_1:
        /* Send 1s of silence */
        break;
    case V18_TX_STATE_ORIGINATING_2:
        /* Send CI and XCI as per V.18/5.1.1 */
        break;
    case V18_TX_STATE_ORIGINATING_3:
        /* ??? */
        break;
    case V18_TX_STATE_ANSWERING_1:
        /* Send silence */
        break;
    case V18_TX_STATE_ANSWERING_2:
        /* Send ANSam */
        break;
    case V18_TX_STATE_ANSWERING_3:
        break;
    case V18_TX_STATE_ORIGINATING_42:
        //len = tone_gen(&s->alert_tone_gen, amp, max_len);
        if (s->tx_signal_on)
        {
            switch (s->current_mode)
            {
            case V18_MODE_NONE:
                break;
            case V18_MODE_DTMF:
                if (len < max_len)
                    len += dtmf_tx(&s->dtmf_tx, amp, max_len - len);
                /*endif*/
                break;
            default:
                if (len < max_len)
                {
                    if ((lenx = fsk_tx(&s->fsk_tx, amp + len, max_len - len)) <= 0)
                        s->tx_signal_on = 0;
                    /*endif*/
                    len += lenx;
                }
                /*endif*/
                break;
            }
            /*endswitch*/
        }
        /*endif*/
        break;
    }
    /*endswitch*/
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v18_rx(v18_state_t *s, const int16_t amp[], int len)
{
    if (s->rx_suppression_timer > 0)
    {
        if (s->rx_suppression_timer > len)
            s->rx_suppression_timer -= len;
        else
            s->rx_suppression_timer = 0;
        /*endif*/
    }
    /*endif*/
    switch (s->rx_state)
    {
    case V18_RX_STATE_ORIGINATING_1:
        /* Listen for:
            ANS
            ANSam
            DTMF
            1400Hz/1800Hz (Weitbrecht)
            980Hz/1180Hz (V.21)
            1270Hz/2225Hz (Bell 103)
            390Hz (V.23 75bps channel)
        */
        caller_tone_scan(s, amp, len);
        break;
    case V18_RX_STATE_ANSWERING_1:
        /* Listen for:
            ANS
            ANSam
            CI/XCI
            DTMF
            1650Hz/1850Hz (V.21)
            1270Hz/2225Hz (Bell 103)
            1300Hz (V.25 calling tone)
        */
        answerer_tone_scan(s, amp, len);
        break;
    case V18_RX_STATE_ORIGINATING_42:
        /* We have negotiated, and are now running one of protocols */
        /* The protocols are either DTMF, or an FSK modem. The modems
           all function the same, once they are selected, and initialised. */
        if ((s->current_mode & V18_MODE_DTMF))
        {
            /* Apply a message timeout. */
            if (s->msg_in_progress_timer)
            {
                s->msg_in_progress_timer -= len;
                if (s->msg_in_progress_timer <= 0)
                {
                    s->msg_in_progress_timer = 0;
                    s->rx_msg_len = 0;
                }
                /*endif*/
            }
            /*endif*/
            dtmf_rx(&s->dtmf_rx, amp, len);
        }
        else
        {
            fsk_rx(&s->fsk_rx, amp, len);
        }
        /*endif*/
        break;
    }
    /*endswitch*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v18_rx_fillin(v18_state_t *s, int len)
{
    if (s->rx_suppression_timer > 0)
    {
        if (s->rx_suppression_timer > len)
            s->rx_suppression_timer -= len;
        else
            s->rx_suppression_timer = 0;
        /*endif*/
    }
    /*endif*/
    if (s->autobauding)
    {
    }
    else
    {
        if (s->current_mode != V18_MODE_NONE)
        {
            if ((s->current_mode & V18_MODE_DTMF))
            {
                /* Apply a message timeout. */
                if (s->msg_in_progress_timer)
                {
                    s->msg_in_progress_timer -= len;
                    if (s->msg_in_progress_timer <= 0)
                    {
                        s->msg_in_progress_timer = 0;
                        s->rx_msg_len = 0;
                    }
                    /*endif*/
                }
                /*endif*/
                dtmf_rx_fillin(&s->dtmf_rx, len);
            }
            else
            {
                fsk_rx_fillin(&s->fsk_rx, len);
            }
            /*endif*/
        }
        /*endif*/
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v18_put(v18_state_t *s, const char msg[], int len)
{
    int i;

    /* This returns the number of characters that would not fit in the buffer.
       The buffer will only be loaded if the whole string of digits will fit,
       in which case zero is returned. */
    if (len < 0)
    {
        if ((len = strlen(msg)) == 0)
            return 0;
        /*endif*/
    }
    /*endif*/
    /* TODO: Deal with out of space condition */
    if ((i = queue_write(&s->queue.queue, (const uint8_t *) msg, len)) < 0)
        return i;
    /*endif*/
    /* Begin to send the carrier */
    if (s->tx_signal_on == 0)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Turning on the carrier\n");
        s->tx_signal_on = 1;
    }
    /*endif*/
    return i;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v18_get_current_mode(v18_state_t *s)
{
    return s->current_mode;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) v18_get_logging_state(v18_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

static void answer_tone_put(void *user_data, int code, int level, int delay)
{
}
/*- End of function --------------------------------------------------------*/

static void init_v18_descriptors(void)
{
    int i;

    for (i = 0;  i < GOERTZEL_TONE_SET_ENTRIES;  i++)
        make_goertzel_descriptor(&tone_set_desc[i], tone_set_frequency[i], GOERTZEL_SAMPLES_PER_BLOCK);
    /*endfor*/
    goertzel_descriptors_inited = true;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v18_set_stored_message(v18_state_t *s, const char *msg)
{
    strncpy(s->stored_message, msg, 80);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(v18_state_t *) v18_init(v18_state_t *s,
                                     bool calling_party,
                                     int mode,
                                     int nation,
                                     span_put_msg_func_t put_msg,
                                     void *put_msg_user_data,
                                     span_modem_status_func_t status_handler,
                                     void *status_handler_user_data)
{
    int i;

    if (nation < 0  ||  nation >= V18_AUTOMODING_END)
        return NULL;
    /*endif*/

    if (s == NULL)
    {
        if ((s = (v18_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
        /*endif*/
    }
    /*endif*/
    memset(s, 0, sizeof(*s));
    s->calling_party = calling_party;
    s->initial_mode = mode & ~V18_MODE_REPETITIVE_SHIFTS_OPTION;
    s->put_msg = put_msg;
    s->put_msg_user_data = put_msg_user_data;
    s->status_handler = status_handler;
    s->status_handler_user_data = status_handler_user_data;

    strcpy(s->stored_message, "V.18 pls");

    if (!goertzel_descriptors_inited)
        init_v18_descriptors();
    /*endif*/
    for (i = 0;  i < GOERTZEL_TONE_SET_ENTRIES;  i++)
        goertzel_init(&s->tone_set[i], &tone_set_desc[i]);
    /*endfor*/
    dtmf_rx_init(&s->dtmf_rx, v18_dtmf_put, s);
    modem_connect_tones_rx_init(&s->answer_tone_rx,
                                MODEM_CONNECT_TONES_ANSAM_PR,
                                answer_tone_put,
                                s);

    v18_set_modem(s, s->initial_mode);
    s->nation = nation;
    if (nation == V18_AUTOMODING_NONE)
    {
        s->autobauding = false;
        s->current_mode = s->initial_mode;
        s->tx_state = V18_TX_STATE_ORIGINATING_42;
        s->rx_state = V18_RX_STATE_ORIGINATING_42;
    }
    else
    {
        s->autobauding = true;
        s->current_mode = V18_MODE_NONE;
    }
    /*endif*/
    queue_init(&s->queue.queue, 128, QUEUE_READ_ATOMIC | QUEUE_WRITE_ATOMIC);
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v18_release(v18_state_t *s)
{
    queue_release(&s->queue.queue);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v18_free(v18_state_t *s)
{
    queue_release(&s->queue.queue);
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
