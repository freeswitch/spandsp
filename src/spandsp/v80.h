/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v80.h - In band DCE control and synchronous data modes for asynchronous DTEs
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2023 Steve Underwood
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

/*! \page v80_page The V.80 in band DCE control and synchronous data modes for asynchronous DTEs
\section v80_page_sec_1 What does it do?
The V.80 specification defines a procedure for controlling and monitoring the control signals of
a DCE using in band signals in the data path. It also permits synchronous communication from an
an asynchronous interface.

\section v80_page_sec_2 How does it work?
*/

#if !defined(_SPANDSP_V80_H_)
#define _SPANDSP_V80_H_

enum
{
    V80_EM = 0x19,

    /* DTE-to-DCE command definitions */
    V80_FROM_DTE_MFGEXTEND = 0x20,              /* <mfgextend><length><rest of cmd> The DCE shall decode this as a sequence of 3 + (<length> - 1Fh) characters. The meaning of <rest of cmd> is manufacturer specific */
    V80_FROM_DTE_MFG1 = 0x21,                   /* The DCE shall decode this as a manufacturer specific command */
    V80_FROM_DTE_MFG2 = 0x22,                   /* The DCE shall decode this as a manufacturer specific command */
    V80_FROM_DTE_MFG3 = 0x23,                   /* The DCE shall decode this as a manufacturer specific command */
    V80_FROM_DTE_MFG4 = 0x24,                   /* The DCE shall decode this as a manufacturer specific command */
    V80_FROM_DTE_MFG5 = 0x25,                   /* The DCE shall decode this as a manufacturer specific command */
    V80_FROM_DTE_MFG6 = 0x26,                   /* The DCE shall decode this as a manufacturer specific command */
    V80_FROM_DTE_MFG7 = 0x27,                   /* The DCE shall decode this as a manufacturer specific command */
    V80_FROM_DTE_MFG8 = 0x28,                   /* The DCE shall decode this as a manufacturer specific command */
    V80_FROM_DTE_MFG9 = 0x29,                   /* The DCE shall decode this as a manufacturer specific command */
    V80_FROM_DTE_MFG10 = 0x2A,                  /* The DCE shall decode this as a manufacturer specific command */
    V80_FROM_DTE_MFG11 = 0x2B,                  /* The DCE shall decode this as a manufacturer specific command */
    V80_FROM_DTE_MFG12 = 0x2C,                  /* The DCE shall decode this as a manufacturer specific command */
    V80_FROM_DTE_MFG13 = 0x2D,                  /* The DCE shall decode this as a manufacturer specific command */
    V80_FROM_DTE_MFG14 = 0x2E,                  /* The DCE shall decode this as a manufacturer specific command */
    V80_FROM_DTE_MFG15 = 0x2F,                  /* The DCE shall decode this as a manufacturer specific command */
    V80_FROM_DTE_EXTEND0 = 0x40,                /* <extend0><length><rest of cmd> The DCE shall decode this as a sequence of 3 + (<length> - 1Fh) characters; see 7.4 */
    V80_FROM_DTE_EXTEND1 = 0x41,                /* <extend1><length><rest of cmd> The DCE shall decode this as a sequence of 3 + (<length> - 1Fh) characters; see 7.4 */
    V80_FROM_DTE_CIRCUIT_105_OFF = 0x42,        /* Circuit 105 (request to send) is OFF */
    V80_FROM_DTE_CIRCUIT_105_ON = 0x43,         /* Circuit 105 (request to send) is ON */
    V80_FROM_DTE_CIRCUIT_108_OFF = 0x44,        /* Circuit 108 (data terminal ready) is OFF */
    V80_FROM_DTE_CIRCUIT_108_ON = 0x45,         /* Circuit 108 (data terminal ready) is ON */
    V80_FROM_DTE_CIRCUIT_133_OFF = 0x46,        /* Circuit 133 (ready for receiving) is OFF */
    V80_FROM_DTE_CIRCUIT_133_ON = 0x47,         /* Circuit 133 (ready for receiving) is ON */
    V80_FROM_DTE_SINGLE_EM_P = 0x58,            /* The DCE shall decode this as one 0x99 in user data */
    V80_FROM_DTE_DOUBLE_EM_P = 0x59,            /* The DCE shall decode this as 0x99 0x99 in user data */
    V80_FROM_DTE_FLOW_OFF = 0x5A,               /* DCE shall decode this as a command to suspend sending In-Band Commands to the DTE */
    V80_FROM_DTE_FLOW_ON = 0x5B,                /* The DCE shall decode this as permission to resume sending In-Band Commands to the DTE */
    V80_FROM_DTE_SINGLE_EM = 0x5C,              /* The DCE shall decode this as one 0x19 in user data */
    V80_FROM_DTE_DOUBLE_EM = 0x5D,              /* The DCE shall decode this as 0x19 0x19 in user data */
    V80_FROM_DTE_POLL = 0x5E,                   /* The DCE shall decode this as a command to deliver a complete set of status commands, one for each circuit or other function supported and enabled. The DCE shall deliver these commands in ascending ordinal order */

    /* DCE-to-DTE command definitions */
    V80_FROM_DCE_EXTENDMFG = 0x30,              /* <extendmfgx><length><rest of cmd> The DCE shall encode this as a sequence of 3 + (<length> - 1Fh) characters. The meaning of <rest of cmd> is manufacturer specific */
    V80_FROM_DCE_MFG1 = 0x31,                   /* The DCE shall encode this as a manufacturer specific command */
    V80_FROM_DCE_MFG2 = 0x32,                   /* The DCE shall encode this as a manufacturer specific command */
    V80_FROM_DCE_MFG3 = 0x33,                   /* The DCE shall encode this as a manufacturer specific command */
    V80_FROM_DCE_MFG4 = 0x34,                   /* The DCE shall encode this as a manufacturer specific command */
    V80_FROM_DCE_MFG5 = 0x35,                   /* The DCE shall encode this as a manufacturer specific command */
    V80_FROM_DCE_MFG6 = 0x36,                   /* The DCE shall encode this as a manufacturer specific command */
    V80_FROM_DCE_MFG7 = 0x37,                   /* The DCE shall encode this as a manufacturer specific command */
    V80_FROM_DCE_MFG8 = 0x38,                   /* The DCE shall encode this as a manufacturer specific command */
    V80_FROM_DCE_MFG9 = 0x39,                   /* The DCE shall encode this as a manufacturer specific command */
    V80_FROM_DCE_MFG10 = 0x3A,                  /* The DCE shall encode this as a manufacturer specific command */
    V80_FROM_DCE_MFG11 = 0x3B,                  /* The DCE shall encode this as a manufacturer specific command */
    V80_FROM_DCE_MFG12 = 0x3C,                  /* The DCE shall encode this as a manufacturer specific command */
    V80_FROM_DCE_MFG13 = 0x3D,                  /* The DCE shall encode this as a manufacturer specific command */
    V80_FROM_DCE_MFG14 = 0x3E,                  /* The DCE shall encode this as a manufacturer specific command */
    V80_FROM_DCE_MFG15 = 0x3F,                  /* The DCE shall encode this as a manufacturer specific command */
    V80_FROM_DCE_EXTEND0 = 0x60,                /* <extend0><length><rest of cmd> The DCE shall encode this as a sequence of 3 + (<length> - 1Fh) characters; see 7.5 */
    V80_FROM_DCE_EXTEND1 = 0x61,                /* <extend1><length><rest of cmd> The DCE shall encode this as a sequence of 3 + (<length> - 1Fh) characters; see 7.5 */
    V80_FROM_DCE_CIRCUIT_106_OFF = 0x62,        /* Circuit 106 (ready for sending) is OFF */
    V80_FROM_DCE_CIRCUIT_106_ON = 0x63,         /* Circuit 106 (ready for sending) is ON */
    V80_FROM_DCE_CIRCUIT_107_OFF = 0x64,        /* Circuit 107 (data set ready) is OFF */
    V80_FROM_DCE_CIRCUIT_107_ON = 0x65,         /* Circuit 107 (data set ready) is ON */
    V80_FROM_DCE_CIRCUIT_109_OFF = 0x66,        /* Circuit 109 (data channel received line signal detector) is OFF */
    V80_FROM_DCE_CIRCUIT_109_ON = 0x67,         /* Circuit 109 (data channel received line signal detector) is ON */
    V80_FROM_DCE_CIRCUIT_110_OFF = 0x68,        /* Circuit 110 is OFF */
    V80_FROM_DCE_CIRCUIT_110_ON = 0x69,         /* Circuit 110 is ON */
    V80_FROM_DCE_CIRCUIT_125_OFF = 0x6A,        /* Circuit 125 (calling indicator) is OFF */
    V80_FROM_DCE_CIRCUIT_125_ON = 0x6B,         /* Circuit 125 (calling indicator) is ON */
    V80_FROM_DCE_CIRCUIT_132_OFF = 0x6C,        /* Circuit 132 (return to non-data mode) is OFF */
    V80_FROM_DCE_CIRCUIT_132_ON = 0x6D,         /* Circuit 132 (return to non-data mode) is ON */
    V80_FROM_DCE_CIRCUIT_142_OFF = 0x6E,        /* Circuit 142 (test indicator) is OFF */
    V80_FROM_DCE_CIRCUIT_142_ON = 0x6F,         /* Circuit 142 (test indicator)is ON */
    V80_FROM_DCE_SINGLE_EM_P = 0x76,            /* The DCE shall encode this as one 0x99 in user data */
    V80_FROM_DCE_DOUBLE_EM_P = 0x77,            /* The DCE shall encode this as 0x99 0x99 in user data */
    V80_FROM_DCE_OFF_LINE = 0x78,               /* Line status is ONLINE (off hook) */
    V80_FROM_DCE_ON_LINE = 0x79,                /* Line status is OFFLINE (on hook) */
    V80_FROM_DCE_FLOW_OFF = 0x7A,               /* The DCE shall encode this as a command to the DTE to suspend sending In-Band Commands to the DCE */
    V80_FROM_DCE_FLOW_ON = 0x7B,                /* The DCE shall encode this as a command to the DTE to resume sending In-Band Commands to the DCE (\*/
    V80_FROM_DCE_SINGLE_EM = 0x7C,              /* The DCE shall encode this as one 0x19 in user data */
    V80_FROM_DCE_DOUBLE_EM = 0x7D,              /* The DCE shall encode this as 0x19 0x19 in user data */
    V80_FROM_DCE_POLL = 0x7E,                   /* The DCE shall encode this as a command to the DTE to deliver a complete set of commands, one for each circuit or other function supported by the DTE. Commands shall be delivered in ascending ordinal order */

    /* Synchronous Access Mode In-Band Commands */
    V80_TRANSPARENCY_T1 = 0x5C,                 /* Transmit/receive one EM */
    V80_TRANSPARENCY_T5 = 0x5D,                 /* Transmit/receive two EMs */
    V80_TRANSPARENCY_T2 = 0x76,                 /* Transmit/receive one 0x99 */
    V80_TRANSPARENCY_T6 = 0x77,                 /* Transmit/receive two 0x99s */
    V80_TRANSPARENCY_T3 = 0xA0,                 /* Transmit/receive DC1 */
    V80_TRANSPARENCY_T4 = 0xA1,                 /* Transmit/receive DC3 */
    V80_TRANSPARENCY_T7 = 0xA2,                 /* Transmit/receive DC1 DC1 */
    V80_TRANSPARENCY_T8 = 0xA3,                 /* Transmit/receive DC3 DC3 */
    V80_TRANSPARENCY_T9 = 0xA4,                 /* Transmit/receive EM 0x99 */
    V80_TRANSPARENCY_T10 = 0xA5,                /* Transmit/receive EM DC1 */
    V80_TRANSPARENCY_T11 = 0xA6,                /* Transmit/receive EM DC3 */
    V80_TRANSPARENCY_T12 = 0xA7,                /* Transmit/receive 0x99 EM */
    V80_TRANSPARENCY_T13 = 0xA8,                /* Transmit/receive 0x99 DC1 */
    V80_TRANSPARENCY_T14 = 0xA9,                /* Transmit/receive 0x99 DC3 */
    V80_TRANSPARENCY_T15 = 0xAA,                /* Transmit/receive DC1 EM */
    V80_TRANSPARENCY_T16 = 0xAB,                /* Transmit/receive DC1 0x99 */
    V80_TRANSPARENCY_T17 = 0xAC,                /* Transmit/receive DC1 DC3 */
    V80_TRANSPARENCY_T18 = 0xAD,                /* Transmit/receive DC3 EM */
    V80_TRANSPARENCY_T19 = 0xAE,                /* Transmit/receive DC3 0x99 */
    V80_TRANSPARENCY_T20 = 0xAF,                /* Transmit/receive DC3 DC1 */

    V80_MARK = 0xB0,        /* Begin transparent sub-mode                                                               HDLC abort detected in framed sub-mode */
    V80_FLAG = 0xB1,        /* Transmit a flag; enter framed sub-mode if currently in Transparent sub-Mode. If enabled, precede with FCS if this follows a non-flag octet sequence              Non-flag to flag transition detected. Preceding data was valid frame; FCS valid if CRC checking was enabled */
    V80_ERR = 0xB2,         /* transmit Abort                                                                           Non-flag to flag transition detected. Preceding data was not a valid frame */
    V80_HUNT = 0xB3,        /* Put receiver in hunt condition                                                           not applicable */
    V80_UNDER = 0xB4,       /* not applicable                                                                           transmit data underrun */
    V80_TOVER = 0xB5,       /* not applicable                                                                           transmit data overrun */
    V80_ROVER = 0xB6,       /* not applicable                                                                           receive data overrun */
    V80_RESUME = 0xB7,      /* Resume after transmit underrun or overrun                                                not applicable */
    V80_BNUM = 0xB8,        /* not applicable                                                                           the following octets, <octnum0 = <octnum1>, specify the number of octets in the transmit data buffer. */
    V80_UNUM = 0xB9,        /* not applicable                                                                           the following octets, <octnum0 = <octnum1>, specify the number of discarded octets duplex carrier control duplex carrier status */
    /* Duplex carrier control */
    V80_EOT = 0xBA,         /* Terminate carrier, return to command state                                               loss of carrier detected, return to command state */
    V80_ECS = 0xBB,         /* Go to on-line command state                                                              confirmation of EM esc = command */
    V80_RRN = 0xBC,         /* Request rate reneg. (duplex)                                                             indicate rate reneg. (duplex) */
    V80_RTN = 0xBD,         /* Request rate retrain (duplex)                                                            indicate rate retrain (duplex) */
    V80_RATE = 0xBE,        /* Following octets, <tx = <rx>, set max. tx and rx rates                                   retrain/reneg. completed; following octets, <tx><rx>, indicate tx and rx rates V.34 HD carrier control V.34 HD duplex carrier status */
    /* V.34 HD carrier control */
    V80_PRI = 0xBC,         /* Go to primary ch. operation                                                              pri. ch. operation commenced; following octet, <prate>, indicates bit rate */
    V80_CTL = 0xBF,         /* Go to control ch. operation                                                              ctl. ch. operation commenced; following octets, <prate><crate>, indicates bit rates */
    V80_RTNH = 0xBD,        /* Initiate pri. channel retrain                                                            indicate pri. channel retrain */
    V80_RTNC = 0xC0,        /* Initiate ctl. channel retrain                                                            indicate ctl. channel retrain */
    V80_RATEH = 0xBE,       /* Following octets, <maxp = <prefc>, set max. pri, rate and preferred ctl. ch. rate        not applicable */
    V80_EOTH = 0xBA,        /* Terminate carrier                                                                        carrier termination detected */
    //V80_ECS = 0xBB        /* Go to command state                                                                      not applicable */
};

/* Primary channel data signalling rate codes */
enum
{
    V80_BIT_RATE_1200 = 0x20,
    V80_BIT_RATE_2400 = 0x21,
    V80_BIT_RATE_4800 = 0x22,
    V80_BIT_RATE_7200 = 0x23,
    V80_BIT_RATE_9600 = 0x24,
    V80_BIT_RATE_12000 = 0x25,
    V80_BIT_RATE_16800 = 0x27,
    V80_BIT_RATE_19200 = 0x28,
    V80_BIT_RATE_21600 = 0x29,
    V80_BIT_RATE_24000 = 0x2A,
    V80_BIT_RATE_26400 = 0x2B,
    V80_BIT_RATE_28800 = 0x2C,
    V80_BIT_RATE_31200 = 0x2D,
    V80_BIT_RATE_33600 = 0x2E,
    V80_BIT_RATE_32000 = 0x2F,
    V80_BIT_RATE_56000 = 0x30,
    V80_BIT_RATE_64000 = 0x31
};

typedef struct v80_state_s v80_state_t;

#if defined(__cplusplus)
extern "C"
{
#endif

SPAN_DECLARE(const char *) v80_escape_to_str(int esc);

SPAN_DECLARE(int) v80_bit_rate_code_to_bit_rate(int rate_code);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
