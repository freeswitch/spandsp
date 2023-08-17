/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v150_1.c - An implementation of the main part of V.150.1. SPRT is not included in
 *            this code.
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <inttypes.h>
#include <memory.h>
#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#else
#include <spandsp/stdbool.h>
#endif

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/unaligned.h"
#include "spandsp/logging.h"
#include "spandsp/async.h"
#include "spandsp/sprt.h"
#include "spandsp/v150_1.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/v150_1.h"

/* Terminology

    V.150.1 has several components. The terms used for these are:

    Signalling state events (SSE)
        An RTP payload type which encodes indications of changes between audio, FoIP, MoIP, and ToIP modes.
        In SDP this is referred to as v150fw.
    Simple packet relay transport (SPRT)
        A hybrid unreliable plus reliable packet over UDP protocol, compatible with sending RTP to and from the
        same UDP port. You can also find the term IP-TLP associated wtih this protocol. In SDP this is referred
        to udpsprt.
    The actual V.150.1 modem relay protocol.
        These are are messages which typically pass across an SPRT transport. In SDP this is referred to as v150mr.
*/

/*  A Cisco box in V.150.1 mode is quite fussy about what it receives to trigger it into a V.8 exchange with an attached
    modem.

    Simply sending a bunch of /ANSam RFC3733/RFC4734 packets gets you nowhere, but this does contradict what RFC4734
    says.
    
    Waiting 200ms after answer, sending 450ms of ANSam, then switching to sending /ANSam until a v150fw packet
    arrives, then sending /ANSam-end, sounds compliant, but a Cisco doesn't like that. It would never happen connected
    to a real modem, as it takes a while to detect ANSam, and be sure the AM part if really there. A real modem
    connected to a Cisco causes the Cisco to send something like 200ms of ANSam, before the switch to /ANSam. Trying
    to mimic that gets you farther.
    
    When I failed to send /ANSam-end at the end of my tone the Cisco behaved quirkily. However, when I call into the
    Cisco, it just stops sending /ANSam, and never seems to send any /ANSam-end packets.

    Cisco seems to consistently accept the following as a valid ANSamPR, resulting in a v150fw CM packet being received
    from the Cisco:
        ANSWER
        Send 40ms to several seconds of silence
        Send 11 to 20 ANSam packets at 20ms per packet
            22 fails, and you get an v150fw AA message, instead of a CM message. This is reasonable, as the phase
            reversal is almost late, and if you consider the sending end would need some time to detect the
            initial tone, its really quite late. 
            21 acts really quirky, and you may get nothing back. The Cisco seems to get really messed up. No RTP or
            SPRT comes from it until the calling hangs up.
            Values between 1 and 10 seem quirky. 10 fails, and you get an v150fw AA message, instead of a CM message.
            Some values between 1 and 10 often work OK, while others give an AA.
        Send sustained /ANSam at 20ms per packet, until...
        ..... v150fw packet received
        Send 4 /ANSam end packets at 20ms intervals
*/

/* A Cisco box has the following parameters:

modem relay latency <milliseconds>
    Specifies the estimated one-way delay across the IP network.
    Range is 100 to 1000. Default is 200.

modem relay sse redundancy interval <milliseconds>
    Specifies the timer value for redundant transmission of SSEs.
    Range is 5ms to 50ms. Default is 20ms.

modem relay sse redundancy packet <number>
    Specifies the SSE packet transmission count before disconnecting.
    Range is 1 to 5 packets. Default is 3.

modem relay sse t1 <milliseconds>
    Specifies the repeat interval, in milliseconds (ms), for initial audio SSEs used for resetting the SSE protocol state machine (clearing the call) following error recovery.
    Range is 500ms to 3000ms. Default is 1000ms.

modem relay sse retries <value>
    Specifies the number of SSE packet retries, repeated every t1 interval, before disconnecting.
    Range is 0 to 5. Default is 5.

modem relay sprt retries <value>
    Specifies the number of SPRT packet retries, repeated every t1 interval, before disconnecting.
    Range is 0 to 10. Default is 10.

modem relay sprt v14 receive playback hold-time <milliseconds>
    Configures the time, in ms, to hold incoming data in the V.14 receive queue.
    Range is 20ms to 250ms. Default is 50ms.

modem relay sprt v14 transmit hold-time <milliseconds>
    Configures the time to wait, in ms, after the first character is ready before sending
    the SPRT packet.
    Range is 10ms to 30ms. Default is 20ms.

modem relay sprt v14 transmit maximum hold-count <characters>
    Configures the number of V.14 characters to be received on the modem interface that will
    trigger sending an SPRT packet.
    Range is 8 to 128. Default is 16.
*/

/*

There are two defined versions of a modem relay gateway:

U-MR:   A Universal Modem Relay

    A UMR needs to support V.92 digital, V.90 digital, V.34, V.32bis, V.32, V.22bis, V.22, V.23 and V.21

V-MR:   A V.8 Modem Relay
    A VMR doesn't have to support any specific set of modulations. Instead, V.8 is used to negotiate a
    common one. Inter-gateway messages exchanged during call setup can be used for each end to inform the
    other which modulations are supported.


The SPRT related SDP needs a entry like:

    a=fmtp:120 mr=1;mg=0;CDSCselect=1;jmdelay=no;versn=1.1

mr=0 for V-MR
  =1 for U-MR

mg=0 for no transcompression
  =1 for single transcompression
  =2 for double transcompression

CDSCselect=1 for audio RFC4733
          =2 for VBD preferred
          =3 for Mixed

mrmods=1-4,10-12,14,17
    where 1 = V.34 duplex
          2 = V.34 half-duplex
          3 = V.32bis/V.32
          4 = V.22bis/V.22
          5 = V.17
          6 = V.29
          7 = V.27ter
          8 = V.26ter
          9 = V.26bis
         10 = V.23 duplex
         11 = V.23 half-duplex
         12 = V.21
         13 = V.90 analogue
         14 = V.90 digital
         15 = V/91
         16 = V.92 analogue
         17 = V.92 digital
         
jmdelay=no      JM delay not supported
       =yes     JM delay supported

versn=1.1   This is optional. The current version is 1.1

txalgs=1 V.44
      =2 MNP5

v42bNumCodeWords=1024

v42bMaxStringLength=32

v44NumTxCodewords=1024

v44NumRxCodewords=1024

v44MaxTxStringLength=64

v44MaxRxStringLength=64

V44LenTxHistory=3072

V44LenRxHistory=3072

TCXpreference=1
             =2



    a=sprtparm: 140 132 132 140 32 8
    
    These are the maximum payload sizes for the 4 channels, and the maximum window sizes for
    the two reliable channels. A '$' may be used for unspecified values.


    a=vndpar: <vendorIDformat> <vendorID> <vendorSpecificDataTag> <vendorSpecificData>

<vendorIDformat>=1 for T.35
                =2 for IANA private enterprise number
                
<vendorID>
<vendorSpecificDataTag>
<vendorSpecificData>

Voice band data (VBD) mode:

<---------------------------------- Data compression ---------------------------------->
<---------------------------------- Error correction ---------------------------------->
<------------------------------------- Modulation ------------------------------------->
                               <-- Encapsulated G.711 -->

<----------- PSTN ------------><-----Packet network ----><----------- PSTN ------------>


The various modem relay error correction and compression scenarios:

MR1

<---------------------------------- Data compression ---------------------------------->
<----- Error correction ------>                          <----- Error correction ------>
<-------- Modulation --------->                          <-------- Modulation --------->
                               <-- Reliable transport -->

<----------- PSTN ------------><-----Packet network ----><----------- PSTN ------------>


MR2

<----- Data compression ------>                          <----- Data compression ------>
<----- Error correction ------>                          <----- Error correction ------>
<-------- Modulation --------->                          <-------- Modulation --------->
                               <----- MR2a or MR2b ----->

<----------- PSTN ------------><-----Packet network ----><----------- PSTN ------------>

MR2a: Reliable transport without data compression
MR2b: Reliable transport with data compression



MR3

<----- Data compression ------><------------------ Data compression ------------------->
<------------------ Data compression -------------------><----- Data compression ------>
<----- Error correction ------>                          <----- Error correction ------>
<-------- Modulation --------->                          <-------- Modulation --------->
                               <-- Reliable transport -->

<----------- PSTN ------------><-----Packet network ----><----------- PSTN ------------>



MR4

<------------------ Data compression -------------------><----- Data compression ------>
<----- Error correction ------>                          <----- Error correction ------>
<-------- Modulation --------->                          <-------- Modulation --------->
                               <-- Reliable transport -->

<----------- PSTN ------------><-----Packet network ----><----------- PSTN ------------>

*/

#if 0

ASN.1 definition, from V.150.1

V150MOIP-CAPABILITY DEFINITIONS AUTOMATIC TAGS ::= BEGIN
IMPORTS
    NonStandardParameter FROM MULTIMEDIA-SYSTEM-CONTROL;
V150MoIPCapability ::= SEQUENCE
{
    nonStandard SEQUENCE OF NonStandardParameter OPTIONAL,
    modemRelayType CHOICE
    {
        v-mr NULL,
        u-mr NULL,
        ...
    },
    gatewayType CHOICE
    {
        ntcx NULL,  -- No Transcompression
        stcx NULL,  -- Single Transcompression
        dtcx CHOICE -- Double Transcompression
        {
            single NULL, -- Preferred mode between two gateways
            double NULL, -- with double transcompression ability
            ...
        },
        ...
    },
    callDiscriminationMode CHOICE
    {
        audio NULL,
        g2-choice NULL,
        combination NULL,
        ...
    },
    sprtParameters SEQUENCE
    {
        maxPayloadSizeChannel0  INTEGER(140..256) OPTIONAL,     -- Default 140
        maxPayloadSizeChannel1  INTEGER(132..256) OPTIONAL,     -- Default 132
        maxWindowSizeChannel1   INTEGER(32..96) OPTIONAL,       -- Default 32
        maxPayloadSizeChannel2  INTEGER(132..256) OPTIONAL,     -- Default 132
        maxWindowSizeChannel2   INTEGER(8..32) OPTIONAL,        -- Default 8
        maxPayloadSizeChannel3  INTEGER(140..256) OPTIONAL,     -- Default 140
        ...
    } OPTIONAL,
    modulationSupport SEQUENCE
    {
        v34FullDuplex   NULL OPTIONAL,
        v34HalfDuplex   NULL OPTIONAL,
        v32bis-v32      NULL OPTIONAL,
        v22bis-v22      NULL OPTIONAL,
        v17             NULL OPTIONAL,
        v29HalfDuplex   NULL OPTIONAL,
        v27ter          NULL OPTIONAL,
        v26ter          NULL OPTIONAL,
        v26bis          NULL OPTIONAL,
        v23FullDuplex   NULL OPTIONAL,
        v23HalfDuplex   NULL OPTIONAL,
        v21             NULL OPTIONAL,
        v90Analog       NULL OPTIONAL,
        v90Digital      NULL OPTIONAL,
        v92Analog       NULL OPTIONAL,
        v92Digital      NULL OPTIONAL,
        v91             NULL OPTIONAL,
        ...
    },
    compressionMode SEQUENCE
    {
        -- Including a SEQUENCE for a particular compression mode, but not
        -- including any of the optional parameters within the SEQUENCE,
        -- indicates support for the specific compression mode, but assumes that
        -- all parameter values are set to their default values
        mnp5                    NULL OPTIONAL,
        v44 SEQUENCE
        {
            numTxCodewords      INTEGER(256..65535),
            numRxCodewords      INTEGER(256..65535),
            maxTxStringLength   INTEGER(32..255),
            maxRxStringLength   INTEGER(32..255),
            lenTxHistory        INTEGER(512..65535),
            lenRxHistory        INTEGER(512..65535),
            ...
        } OPTIONAL,
        v42bis SEQUENCE
        {
            numCodewords        INTEGER(512..65535) OPTIONAL,
            maxStringLength     INTEGER(6..250) OPTIONAL,
            ...
        } OPTIONAL,
        ...
    } OPTIONAL,
    delayedJMEnabled        BOOLEAN,
    ...
}
#endif

/* Used to verify the message type is compatible with the transmission
   control channel it arrived on */
static uint8_t channel_check[25] = 
{
    0x0F,       /* V150_1_MSGID_NULL */
    0x04,       /* V150_1_MSGID_INIT */
    0x04,       /* V150_1_MSGID_XID_XCHG */
    0x04,       /* V150_1_MSGID_JM_INFO */
    0x04,       /* V150_1_MSGID_START_JM */
    0x04,       /* V150_1_MSGID_CONNECT */
    0x0F,       /* V150_1_MSGID_BREAK */
    0x0F,       /* V150_1_MSGID_BREAKACK */
    0x04,       /* V150_1_MSGID_MR_EVENT */
    0x04,       /* V150_1_MSGID_CLEARDOWN */
    0x04,       /* V150_1_MSGID_PROF_XCHG */
    0x00,       /* Reserved (11) */
    0x00,       /* Reserved (12) */
    0x00,       /* Reserved (13) */
    0x00,       /* Reserved (14) */
    0x00,       /* Reserved (15) */
    0x0A,       /* V150_1_MSGID_I_RAW_OCTET */
    0x0A,       /* V150_1_MSGID_I_RAW_BIT       (optional) */
    0x0A,       /* V150_1_MSGID_I_OCTET */
    0x0A,       /* V150_1_MSGID_I_CHAR_STAT     (optional) */
    0x0A,       /* V150_1_MSGID_I_CHAR_DYN      (optional) */
    0x0A,       /* V150_1_MSGID_I_FRAME         (optional) */
    0x0A,       /* V150_1_MSGID_I_OCTET_CS      (optional) (this only makes sense for the SPRT_TCID_UNRELIABLE_SEQUENCED channel) */
    0x0A,       /* V150_1_MSGID_I_CHAR_STAT_CS  (optional) (this only makes sense for the SPRT_TCID_UNRELIABLE_SEQUENCED channel) */
    0x0A        /* V150_1_MSGID_I_CHAR_DYN_CS   (optional) (this only makes sense for the SPRT_TCID_UNRELIABLE_SEQUENCED channel) */
};

static struct
{
    uint16_t min_payload_bytes;
    uint16_t max_payload_bytes;
} channel_parm_limits[SPRT_CHANNELS] =
{
    {
        SPRT_MIN_TC0_PAYLOAD_BYTES,
        SPRT_MAX_TC0_PAYLOAD_BYTES
    },
    {
        SPRT_MIN_TC1_PAYLOAD_BYTES,
        SPRT_MAX_TC1_PAYLOAD_BYTES
    },
    {
        SPRT_MIN_TC2_PAYLOAD_BYTES,
        SPRT_MAX_TC2_PAYLOAD_BYTES
    },
    {
        SPRT_MIN_TC3_PAYLOAD_BYTES,
        SPRT_MAX_TC3_PAYLOAD_BYTES
    }
};

SPAN_DECLARE(const char *) v150_1_msg_id_to_str(int msg_id)
{
    const char *res;
    
    res = "unknown";
    switch (msg_id)
    {
    case V150_1_MSGID_NULL:
        res = "NULL";
        break;
    case V150_1_MSGID_INIT:
        res = "INIT";
        break;
    case V150_1_MSGID_XID_XCHG:
        res = "XID xchg";
        break;
    case V150_1_MSGID_JM_INFO:
        res = "JM info";
        break;
    case V150_1_MSGID_START_JM:
        res = "Start JM";
        break;
    case V150_1_MSGID_CONNECT:
        res = "Connect";
        break;
    case V150_1_MSGID_BREAK:
        res = "Break";
        break;
    case V150_1_MSGID_BREAKACK:
        res = "Break ack";
        break;
    case V150_1_MSGID_MR_EVENT:
        res = "MR event";
        break;
    case V150_1_MSGID_CLEARDOWN:
        res = "Cleardown";
        break;
    case V150_1_MSGID_PROF_XCHG:
        res = "Prof xchg";
        break;
    case V150_1_MSGID_I_RAW_OCTET:
        res = "I raw octet";
        break;
    case V150_1_MSGID_I_RAW_BIT:
        res = "I raw bit";
        break;
    case V150_1_MSGID_I_OCTET:
        res = "I octet";
        break;
    case V150_1_MSGID_I_CHAR_STAT:
        res = "I char stat";
        break;
    case V150_1_MSGID_I_CHAR_DYN:
        res = "I char dyn";
        break;
    case V150_1_MSGID_I_FRAME:
        res = "I frame";
        break;
    case V150_1_MSGID_I_OCTET_CS:
        res = "I octet cs";
        break;
    case V150_1_MSGID_I_CHAR_STAT_CS:
        res = "I char stat cs";
        break;
    case V150_1_MSGID_I_CHAR_DYN_CS:
        res = "I char dyn cs";
        break;
    }
    /*endswitch*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) v150_1_data_bits_to_str(int code)
{
    const char *res;
    
    res = "unknown";
    switch (code)
    {
    case V150_1_DATA_BITS_5:
        res = "5 bits";
        break;
    case V150_1_DATA_BITS_6:
        res = "6 bits";
        break;
    case V150_1_DATA_BITS_7:
        res = "7 bits";
        break;
    case V150_1_DATA_BITS_8:
        res = "8 bits";
        break;
    }
    /*endswitch*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) v150_1_parity_to_str(int code)
{
    const char *res;
    
    res = "unknown";
    switch (code)
    {
    case V150_1_PARITY_UNKNOWN:
        res = "unknown";
        break;
    case V150_1_PARITY_NONE:
        res = "none";
        break;
        break;
    case V150_1_PARITY_EVEN:
        res = "even";
        break;
    case V150_1_PARITY_ODD:
        res = "odd";
        break;
    case V150_1_PARITY_SPACE:
        res = "space";
        break;
    case V150_1_PARITY_MARK:
        res = "mark";
        break;
    }
    /*endswitch*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) v150_1_stop_bits_to_str(int code)
{
    const char *res;
    
    res = "unknown";
    switch (code)
    {
    case V150_1_STOP_BITS_1:
        res = "1 bit";
        break;
    case V150_1_STOP_BITS_2:
        res = "2 bits";
        break;
    }
    /*endswitch*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) v150_1_mr_event_type_to_str(int type)
{
    const char *res;
    
    res = "unknown";
    switch (type)
    {
    case V150_1_MR_EVENT_ID_NULL:
        res = "NULL";
        break;
    case V150_1_MR_EVENT_ID_RATE_RENEGOTIATION:
        res = "Renegotiation";
        break;
    case V150_1_MR_EVENT_ID_RETRAIN:
        res = "Retrain";
        break;
    case V150_1_MR_EVENT_ID_PHYSUP:
        res = "Physically up";
        break;
    }
    /*endswitch*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) v150_1_cleardown_reason_to_str(int type)
{
    const char *res;
    
    res = "unknown";
    switch (type)
    {
    case V150_1_CLEARDOWN_REASON_UNKNOWN:
        res = "Unknown";
        break;
    case V150_1_CLEARDOWN_REASON_PHYSICAL_LAYER_RELEASE:
        res = "Physical layer release";
        break;
    case V150_1_CLEARDOWN_REASON_LINK_LAYER_DISCONNECT:
        res = "Link layer disconnect";
        break;
    case V150_1_CLEARDOWN_REASON_DATA_COMPRESSION_DISCONNECT:
        res = "Data compression disconnect";
        break;
    case V150_1_CLEARDOWN_REASON_ABORT:
        res = "Abort";
        break;
    case V150_1_CLEARDOWN_REASON_ON_HOOK:
        res = "On hook";
        break;
    case V150_1_CLEARDOWN_REASON_NETWORK_LAYER_TERMINATION:
        res = "Network layer termination";
        break;
    case V150_1_CLEARDOWN_REASON_ADMINISTRATIVE:
        res = "Administrative";
        break;
    }
    /*endswitch*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) v150_1_symbol_rate_to_str(int code)
{
    const char *res;
    
    res = "unknown";
    switch (code)
    {
    case V150_1_SYMBOL_RATE_NULL:
        res = "NULL";
        break;
    case V150_1_SYMBOL_RATE_600:
        res = "600 baud";
        break;
    case V150_1_SYMBOL_RATE_1200:
        res = "1200 baud";
        break;
    case V150_1_SYMBOL_RATE_1600:
        res = "1600 baud";
        break;
    case V150_1_SYMBOL_RATE_2400:
        res = "2400 baud";
        break;
    case V150_1_SYMBOL_RATE_2743:
        res = "2743 baud";
        break;
    case V150_1_SYMBOL_RATE_3000:
        res = "3000 baud";
        break;
    case V150_1_SYMBOL_RATE_3200:
        res = "3200 baud";
        break;
    case V150_1_SYMBOL_RATE_3429:
        res = "3429 baud";
        break;
    case V150_1_SYMBOL_RATE_8000:
        res = "8000 baud";
        break;
    }
    /*endswitch*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) v150_1_modulation_to_str(int modulation)
{
    const char *res;

    res = "unknown";
    switch (modulation)
    {
    case V150_1_SELMOD_NULL:
        res = "NULL";
        break;
    case V150_1_SELMOD_V92:
        res = "V.92";
        break;
    case V150_1_SELMOD_V91:
        res = "V.91";
        break;
    case V150_1_SELMOD_V90:
        res = "V90";
        break;
    case V150_1_SELMOD_V34:
        res = "V.34";
        break;
    case V150_1_SELMOD_V32bis:
        res = "V.32bis";
        break;
    case V150_1_SELMOD_V32:
        res = "V.32";
        break;
    case V150_1_SELMOD_V22bis:
        res = "V.22bis";
        break;
    case V150_1_SELMOD_V22:
        res = "V.22";
        break;
    case V150_1_SELMOD_V17:
        res = "V.17";
        break;
    case V150_1_SELMOD_V29:
        res = "V.29";
        break;
    case V150_1_SELMOD_V27ter:
        res = "V.27ter";
        break;
    case V150_1_SELMOD_V26ter:
        res = "V.26ter";
        break;
    case V150_1_SELMOD_V26bis:
        res = "V.26bis";
        break;
    case V150_1_SELMOD_V23:
        res = "V.23";
        break;
    case V150_1_SELMOD_V21:
        res = "V.21";
        break;
    case V150_1_SELMOD_BELL212:
        res = "Bell 212";
        break;
    case V150_1_SELMOD_BELL103:
        res = "Bell 103";
        break;
    }
    /*endswitch*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) v150_1_compression_to_str(int compression)
{
    const char *res;

    res = "unknown";
    switch (compression)
    {
    case V150_1_COMPRESSION_NONE:
        res = "None";
        break;
    case V150_1_COMPRESSION_V42BIS:
        res = "V.42bis";
        break;
    case V150_1_COMPRESSION_V44:
        res = "V.44";
        break;
    case V150_1_COMPRESSION_MNP5:
        res = "MNP5";
        break;
    }
    /*endswitch*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) v150_1_compression_direction_to_str(int direction)
{
    const char *res;

    res = "unknown";
    switch (direction)
    {
    case V150_1_COMPRESS_NEITHER_WAY:
        res = "Neither way";
        break;
    case V150_1_COMPRESS_TX_ONLY:
        res = "Tx only";
        break;
    case V150_1_COMPRESS_RX_ONLY:
        res = "Rx only";
        break;
    case V150_1_COMPRESS_BIDIRECTIONAL:
        res = "Bidirectional";
        break;
    }
    /*endswitch*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) v150_1_error_correction_to_str(int correction)
{
    const char *res;

    res = "unknown";
    switch (correction)
    {
    case V150_1_ERROR_CORRECTION_NONE:
        res = "None";
        break;
    case V150_1_ERROR_CORRECTION_V42_LAPM:
        res = "V.42 LAPM";
        break;
    case V150_1_ERROR_CORRECTION_V42_ANNEX_A:
        res = "V.42 annex A";
        break;
    }
    /*endswitch*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) v150_1_break_source_to_str(int source)
{
    const char *res;

    res = "unknown";
    switch (source)
    {
    case V150_1_BREAK_SOURCE_V42_LAPM:
        res = "V.42 LAPM";
        break;
    case V150_1_BREAK_SOURCE_V42_ANNEX_A:
        res = "V.42 annex A";
        break;
    case V150_1_BREAK_SOURCE_V14:
        res = "V.14";
        break;
    }
    /*endswitch*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) v150_1_break_type_to_str(int type)
{
    const char *res;

    res = "unknown";
    switch (type)
    {
    case V150_1_BREAK_TYPE_NOT_APPLICABLE:
        res = "Non applicable";
        break;
    case V150_1_BREAK_TYPE_DESTRUCTIVE_EXPEDITED:
        res = "Destructive, expedited";
        break;
    case V150_1_BREAK_TYPE_NON_DESTRUCTIVE_EXPEDITED:
        res = "Non-destructive, expedited";
        break;
    case V150_1_BREAK_TYPE_NON_DESTRUCTIVE_NON_EXPEDITED:
        res = "Non-destructive, non-expedited";
        break;
    }
    /*endswitch*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) v150_1_state_to_str(int state)
{
    const char *res;

    res = "unknown";
    switch (state)
    {
    case V150_1_STATE_IDLE:
        res = "Idle";
        break;
    case V150_1_STATE_INITED:
        res = "Inited";
        break;
    case V150_1_STATE_PHYSUP:
        res = "Physically up";
        break;
    case V150_1_STATE_CONNECTED:
        res = "Connected";
        break;
    }
    /*endswitch*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_set_bits_per_character(v150_1_state_t *s, int bits)
{
    if (bits < 5  ||  bits > 8)
        return -1;
    /*endif*/
    bits -= 5;
    s->near.data_format_code &= 0x9F; 
    s->near.data_format_code |= ((bits << 5) & 0x60); 
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_set_parity(v150_1_state_t *s, int mode)
{
    s->near.data_format_code &= 0xE3; 
    s->near.data_format_code |= ((mode << 2) & 0x1C); 
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_set_stop_bits(v150_1_state_t *s, int bits)
{
    if (bits < 1  ||  bits > 2)
        return -1;
    /*endif*/
    bits -= 1;
    s->near.data_format_code &= 0xFC; 
    s->near.data_format_code |= (bits & 0x03);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int status_report(v150_1_state_t *s, int reason)
{
    v150_1_status_t report;

    report.reason = reason;
    switch (reason)
    {
    case V150_1_STATUS_REASON_STATE_CHANGED:
        report.state_change.state = s->far.connection_state;
        report.state_change.cleardown_reason = s->far.cleardown_reason;
        break;
    case V150_1_STATUS_REASON_DATA_FORMAT_CHANGED:
        report.data_format_change.bits = 5 + ((s->far.data_format_code >> 5) & 0x03);
        report.data_format_change.parity_code = (s->far.data_format_code >> 2) & 0x07;
        report.data_format_change.stop_bits = 1 + (s->far.data_format_code & 0x03);
        break;
    case V150_1_STATUS_REASON_BREAK_RECEIVED:
        report.break_received.source = s->far.break_source;
        report.break_received.type = s->far.break_type;
        report.break_received.duration = s->far.break_duration*10;
        break;
    case V150_1_STATUS_REASON_RATE_RETRAIN_RECEIVED:
        break;
    case V150_1_STATUS_REASON_RATE_RENEGOTIATION_RECEIVED:
        break;
    case V150_1_STATUS_REASON_BUSY_CHANGED:
        report.busy_change.local_busy = s->near.busy;
        report.busy_change.far_busy = s->far.busy;
        break;
    case V150_1_STATUS_REASON_PHYSUP:
        report.physup_parameters.selmod = s->far.selmod;
        report.physup_parameters.tdsr = s->far.tdsr;
        report.physup_parameters.rdsr = s->far.rdsr;

        report.physup_parameters.txsen = s->far.txsen;
        report.physup_parameters.txsr = s->far.txsr;
        report.physup_parameters.rxsen = s->far.rxsen;
        report.physup_parameters.rxsr = s->far.rxsr;
        break;
    case V150_1_STATUS_REASON_CONNECTED:
        report.connect_parameters.selmod = s->far.selmod;
        report.connect_parameters.tdsr = s->far.tdsr;
        report.connect_parameters.rdsr = s->far.rdsr;

        report.connect_parameters.selected_compression_direction = s->far.selected_compression_direction;
        report.connect_parameters.selected_compression = s->far.selected_compression;
        report.connect_parameters.selected_error_correction = s->far.selected_error_correction;

        report.connect_parameters.compression_tx_dictionary_size = s->far.compression_tx_dictionary_size;
        report.connect_parameters.compression_rx_dictionary_size = s->far.compression_rx_dictionary_size;
        report.connect_parameters.compression_tx_string_length = s->far.compression_tx_string_length;
        report.connect_parameters.compression_rx_string_length = s->far.compression_rx_string_length;
        report.connect_parameters.compression_tx_history_size = s->far.compression_tx_history_size;
        report.connect_parameters.compression_rx_history_size = s->far.compression_rx_history_size;

        /* I_RAW-OCTET is always available. There is no selection flag for it. */
        report.connect_parameters.i_raw_octet_available = true;
        report.connect_parameters.i_raw_bit_available = s->far.i_raw_bit_available;
        report.connect_parameters.i_frame_available = s->far.i_frame_available;
        /* I_OCTET is an oddity, as you need to know in advance whether there will be a DLCI field
           present. So, functionally its really like 2 different types of message. */
        report.connect_parameters.i_octet_with_dlci_available = s->far.i_octet_with_dlci_available;
        report.connect_parameters.i_octet_without_dlci_available = s->far.i_octet_without_dlci_available;
        report.connect_parameters.i_char_stat_available = s->far.i_char_stat_available;
        report.connect_parameters.i_char_dyn_available = s->far.i_char_dyn_available;
        /* Unlike I_OCTET, I_OCTET-CS is only defined without a DLCI field. */
        report.connect_parameters.i_octet_cs_available = s->far.i_octet_cs_available;
        report.connect_parameters.i_char_stat_cs_available = s->far.i_char_stat_cs_available;
        report.connect_parameters.i_char_dyn_cs_available = s->far.i_char_dyn_cs_available;
        break;
    }
    /*endswitch*/
    if (s->rx_status_report_handler)
        s->rx_status_report_handler(s->rx_status_report_user_data, &report);
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_tx_null(v150_1_state_t *s)
{
    int res;
    uint8_t pkt[256];

    res = -1;
    /* This isn't a real message. Its marked as reserved by the ITU-T in V.150.1 */
    pkt[0] = V150_1_MSGID_NULL;
    if (s->tx_packet_handler)
        res = s->tx_packet_handler(s->tx_packet_user_data, SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, pkt, 1);
    /*endif*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_tx_init(v150_1_state_t *s)
{
    int res;
    uint8_t i;
    uint8_t pkt[256];

    res = -1;
    pkt[0] = V150_1_MSGID_INIT;
    /* At this stage we just tell the far end the things we support. */
    i = 0;
    if (s->near.necrxch_option)
        i |= 0x80;
    /*endif*/
    if (s->near.ecrxch_option)
        i |= 0x40;
    /*endif*/
    if (s->near.xid_profile_exchange_supported)
        i |= 0x20;
    /*endif*/
    if (s->near.asymmetric_data_types_supported)
        i |= 0x10;
    /*endif*/
    if (s->near.i_raw_bit_supported)
        i |= 0x08;
    /*endif*/
    if (s->near.i_frame_supported)
        i |= 0x04;
    /*endif*/
    if (s->near.i_char_stat_supported)
        i |= 0x02;
    /*endif*/
    if (s->near.i_char_dyn_supported)
        i |= 0x01;
    /*endif*/
    pkt[1] = i;
    i = 0;
    if (s->near.i_octet_cs_supported)
        i |= 0x80;
    /*endif*/
    if (s->near.i_char_stat_cs_supported)
        i |= 0x40;
    /*endif*/
    if (s->near.i_char_dyn_cs_supported)
        i |= 0x20;
    /*endif*/
    pkt[2] = i;
    if (s->tx_packet_handler)
        res = s->tx_packet_handler(s->tx_packet_user_data, SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, pkt, 3);
    /*endif*/
    if (res >= 0)
    {
        s->near.connection_state = V150_1_STATE_INITED;
        if (s->far.connection_state >= V150_1_STATE_INITED)
            s->joint_connection_state = V150_1_STATE_INITED;
        /*endif*/
    }
    /*endif*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_tx_xid_xchg(v150_1_state_t *s)
{
    int res;
    uint8_t i;
    uint8_t pkt[256];

    res = -1;
    if (!s->far.xid_profile_exchange_supported)
        return -1;
    /*endif*/
    pkt[0] = V150_1_MSGID_XID_XCHG;
    pkt[1] = s->near.ecp;
    i = 0;
    if (s->near.v42bis_supported)
        i |= 0x80;
    /*endif*/
    if (s->near.v44_supported)
        i |= 0x40;
    /*endif*/
    if (s->near.mnp5_supported)
        i |= 0x20;
    /*endif*/
    pkt[2] = i;
    if (s->near.v42bis_supported)
    {
        pkt[3] = s->near.v42bis_p0;
        put_net_unaligned_uint16(&pkt[4], s->near.v42bis_p1);
        pkt[6] = s->near.v42bis_p2;
    }
    else
    {
        memset(&pkt[3], 0, 4);
    }
    /*endif*/
    if (s->near.v44_supported)
    {
        pkt[7] = s->near.v44_c0;
        pkt[8] = s->near.v44_p0;
        put_net_unaligned_uint16(&pkt[9], s->near.v44_p1t);
        put_net_unaligned_uint16(&pkt[11], s->near.v44_p1r);
        pkt[13] = s->near.v44_p2t;
        pkt[14] = s->near.v44_p2r;
        put_net_unaligned_uint16(&pkt[15], s->near.v44_p3t);
        put_net_unaligned_uint16(&pkt[17], s->near.v44_p3r);
    }
    else
    {
        memset(&pkt[7], 0, 12);
    }
    /*endif*/
    if (s->tx_packet_handler)
        res = s->tx_packet_handler(s->tx_packet_user_data, SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, pkt, 19);
    /*endif*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_tx_jm_info(v150_1_state_t *s)
{
    int res;
    int i;
    int len;
    uint8_t pkt[256];

    res = -1;
    pkt[0] = V150_1_MSGID_JM_INFO;
    len = 1;
    for (i = 0;  i < 16;  i++)
    {
        if (s->near.jm_category_id_seen[i])
        {
            put_net_unaligned_uint16(&pkt[len], (i << 12) | (s->near.jm_category_info[i] & 0x0FFF));
            len += 2;
        }
        /*endif*/
    }
    if (s->tx_packet_handler)
        res = s->tx_packet_handler(s->tx_packet_user_data, SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, pkt, len);
    /*endif*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_tx_start_jm(v150_1_state_t *s)
{
    int res;
    uint8_t pkt[256];

    res = -1;
    pkt[0] = V150_1_MSGID_START_JM;
    if (s->tx_packet_handler)
        res = s->tx_packet_handler(s->tx_packet_user_data, SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, pkt, 1);
    /*endif*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_tx_connect(v150_1_state_t *s)
{
    int res;
    int available_data_types;
    int len;
    uint8_t pkt[256];

    res = -1;
    pkt[0] = V150_1_MSGID_CONNECT;
    pkt[1] = (s->near.selmod << 2) | s->near.selected_compression_direction;
    pkt[2] = (s->near.selected_compression << 4) | s->near.selected_error_correction;
    put_net_unaligned_uint16(&pkt[3], s->near.tdsr);
    put_net_unaligned_uint16(&pkt[5], s->near.rdsr);

    available_data_types = 0;
    if (s->near.i_octet_with_dlci_available)
        available_data_types |= 0x8000;
    /*endif*/
    if (s->near.i_octet_without_dlci_available)
        available_data_types |= 0x4000;
    /*endif*/
    if (s->near.i_raw_bit_available)
        available_data_types |= 0x2000;
    /*endif*/
    if (s->near.i_frame_available)
        available_data_types |= 0x1000;
    /*endif*/
    if (s->near.i_char_stat_available)
        available_data_types |= 0x0800;
    /*endif*/
    if (s->near.i_char_dyn_available)
        available_data_types |= 0x0400;
    /*endif*/
    if (s->near.i_octet_cs_available)
        available_data_types |= 0x0200;
    /*endif*/
    if (s->near.i_char_stat_cs_available)
        available_data_types |= 0x0100;
    /*endif*/
    if (s->near.i_char_dyn_cs_available)
        available_data_types |= 0x0080;
    /*endif*/
    put_net_unaligned_uint16(&pkt[7], available_data_types);
    len = 9;
    if (s->near.selected_compression == V150_1_COMPRESSION_V42BIS  ||  s->near.selected_compression == V150_1_COMPRESSION_V44)
    {
        /* This is only included if V.42bis or V.44 is selected. For no compression, or MNP5 this is omitted */
        put_net_unaligned_uint16(&pkt[9], s->near.compression_tx_dictionary_size);
        put_net_unaligned_uint16(&pkt[11], s->near.compression_rx_dictionary_size);
        pkt[13] = s->near.compression_tx_string_length;
        pkt[14] = s->near.compression_rx_string_length;
        len += 6;
    }
    /*endif*/
    if (s->near.selected_compression == V150_1_COMPRESSION_V44)
    {
        /* This is only included if V.44 is selected. For no compression, MNP5, or V.42bis this is omitted */
        put_net_unaligned_uint16(&pkt[15], s->near.compression_tx_history_size);
        put_net_unaligned_uint16(&pkt[15], s->near.compression_rx_history_size);
        len += 4;
    }
    /*endif*/
    if (s->tx_packet_handler)
        res = s->tx_packet_handler(s->tx_packet_user_data, SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, pkt, len);
    /*endif*/
    if (res >= 0)
    {
        s->near.connection_state = V150_1_STATE_CONNECTED;
        if (s->near.connection_state >= V150_1_STATE_CONNECTED)
            s->joint_connection_state = V150_1_STATE_CONNECTED;
        /*endif*/
    }
    /*endif*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_tx_break(v150_1_state_t *s, int source, int type, int duration)
{
    int res;
    uint8_t pkt[256];

    res = -1;
    pkt[0] = V150_1_MSGID_BREAK;
    pkt[1] = (source << 4) | type;
    pkt[2] = duration/10;
    if (s->tx_packet_handler)
        res = s->tx_packet_handler(s->tx_packet_user_data, SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, pkt, 3);
    /*endif*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_tx_break_ack(v150_1_state_t *s)
{
    int res;
    uint8_t pkt[256];

    res = -1;
    pkt[0] = V150_1_MSGID_BREAKACK;
    if (s->tx_packet_handler)
        res = s->tx_packet_handler(s->tx_packet_user_data, SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, pkt, 1);
    /*endif*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_tx_mr_event(v150_1_state_t *s, int event_id)
{
    int res;
    int len;
    uint8_t i;
    uint8_t pkt[256];

    res = -1;
    pkt[0] = V150_1_MSGID_MR_EVENT;
    pkt[1] = event_id;
    switch (event_id)
    {
    case V150_1_MR_EVENT_ID_RETRAIN:
        pkt[2] = V150_1_MR_EVENT_REASON_NULL;
        len = 3;
        s->near.connection_state = V150_1_STATE_RETRAIN;
        s->joint_connection_state = V150_1_STATE_RETRAIN;
        break;
    case V150_1_MR_EVENT_ID_RATE_RENEGOTIATION:
        pkt[2] = V150_1_MR_EVENT_REASON_NULL;
        len = 3;
        s->near.connection_state = V150_1_STATE_RATE_RENEGOTIATION;
        s->joint_connection_state = V150_1_STATE_RATE_RENEGOTIATION;
        break;
    case V150_1_MR_EVENT_ID_PHYSUP:
        pkt[2] = 0;
        i = (s->near.selmod << 2);
        if (s->near.txsen)
            i |= 0x02;
        /*endif*/
        if (s->near.rxsen)
            i |= 0x01;
        /*endif*/
        pkt[3] = i;
        put_net_unaligned_uint16(&pkt[4], s->near.tdsr);
        put_net_unaligned_uint16(&pkt[4], s->near.rdsr);
        pkt[8] = (s->near.txsen)  ?  s->near.txsr  :  V150_1_SYMBOL_RATE_NULL;
        pkt[9] = (s->near.rxsen)  ?  s->near.rxsr  :  V150_1_SYMBOL_RATE_NULL;
        len = 10;
        s->near.connection_state = V150_1_STATE_PHYSUP;
        if (s->far.connection_state >= V150_1_STATE_PHYSUP)
            s->joint_connection_state = V150_1_STATE_PHYSUP;
        /*endif*/
        break;
    case V150_1_MR_EVENT_ID_NULL:
    default:
        pkt[2] = 0;
        len = 3;
        break;
    }
    /*endswitch*/
    if (s->tx_packet_handler)
        res = s->tx_packet_handler(s->tx_packet_user_data, SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, pkt, len);
    /*endif*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_tx_cleardown(v150_1_state_t *s, int reason)
{
    int res;
    uint8_t pkt[256];

    res = -1;

    pkt[0] = V150_1_MSGID_CLEARDOWN;
    pkt[1] = reason;
    pkt[2] = 0; /* Vendor tag */
    pkt[3] = 0; /* Vendor info */
    if (s->tx_packet_handler)
        res = s->tx_packet_handler(s->tx_packet_user_data, SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, pkt, 4);
    /*endif*/
    if (res >= 0)
        s->near.connection_state = V150_1_STATE_IDLE;
    /*endif*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_tx_prof_xchg(v150_1_state_t *s)
{
    int res;
    uint8_t i;
    uint8_t pkt[256];

    res = -1;
    pkt[0] = V150_1_MSGID_PROF_XCHG;
    i = 0;
    if (s->near.v42_lapm_supported)
        i |= 0x40;
    /*endif*/
    if (s->near.v42_annex_a_supported)
        i |= 0x10;
    /*endif*/
    if (s->near.v44_supported)
        i |= 0x04;
    /*endif*/
    if (s->near.v42bis_supported)
        i |= 0x01;
    /*endif*/
    pkt[1] = i;
    i = 0;
    if (s->near.mnp5_supported)
        i |= 0x40;
    /*endif*/
    pkt[2] = i;
    if (s->near.v42bis_supported)
    {
        pkt[3] = s->near.v42bis_p0;
        put_net_unaligned_uint16(&pkt[4], s->near.v42bis_p1);
        pkt[6] = s->near.v42bis_p2;
    }
    else
    {
        memset(&pkt[3], 0, 4);
    }
    /*endif*/
    if (s->near.v44_supported)
    {
        pkt[7] = s->near.v44_c0;
        pkt[8] = s->near.v44_p0;
        put_net_unaligned_uint16(&pkt[9], s->near.v44_p1t);
        put_net_unaligned_uint16(&pkt[11], s->near.v44_p1r);
        pkt[13] = s->near.v44_p2t;
        pkt[14] = s->near.v44_p2r;
        put_net_unaligned_uint16(&pkt[15], s->near.v44_p3t);
        put_net_unaligned_uint16(&pkt[17], s->near.v44_p3r);
    }
    else
    {
        memset(&pkt[7], 0, 12);
    }
    /*endif*/
    if (s->tx_packet_handler)
        res = s->tx_packet_handler(s->tx_packet_user_data, SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, pkt, 19);
    /*endif*/
    return res;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_build_i_raw_octet(v150_1_state_t *s, uint8_t pkt[], int max_len, const uint8_t buf[], int len)
{
    if (len > max_len - 3)
        return -1;
    /*endif*/
    pkt[0] = V150_1_MSGID_I_RAW_OCTET;
    pkt[1] = 0x80 | 0x02;  /* L */
    pkt[2] = 0x02;  /* N */
    memcpy(&pkt[3], buf, len);
    len += 3;
    return len;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_build_i_raw_bit(v150_1_state_t *s, uint8_t pkt[], int max_len, const uint8_t buf[], int len)
{
    if (!s->far.i_raw_bit_available)
        return -1;
    /*endif*/
    if (len > max_len - 3)
        return -1;
    /*endif*/
    pkt[0] = V150_1_MSGID_I_RAW_BIT;
    pkt[1] = 0x80 | 0x02;  /* L */
    pkt[2] = 0x02;  /* N */
    memcpy(&pkt[3], buf, len);
    len += 3;
    return len;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_build_i_octet(v150_1_state_t *s, uint8_t pkt[], int max_len, const uint8_t buf[], int len)
{
    int header;

    if (!s->far.i_octet_without_dlci_available  &&  !s->far.i_octet_with_dlci_available)
        return -1;
    /*endif*/
    if (len > max_len - 3)
        return -1;
    /*endif*/
    pkt[0] = V150_1_MSGID_I_OCTET;
    if (s->far.i_octet_with_dlci_available)
    {
        /* The DLCI may be one or two octets long. */
        if ((s->near.dlci & 0x01) == 0)
        {
            pkt[1] = s->near.dlci & 0xFF;
            header = 2;
        }
        else
        {
            put_net_unaligned_uint16(&pkt[1], s->near.dlci);
            header = 3;
        }
        /*endif*/
    }
    else
    {
        header = 1;
    }
    /*endif*/
    memcpy(&pkt[header], buf, len);
    len += header;
    return len;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_build_i_char_stat(v150_1_state_t *s, uint8_t pkt[], int max_len, const uint8_t buf[], int len)
{
    if (!s->far.i_char_stat_available)
        return -1;
    /*endif*/
    if (len > max_len - 2)
        return -1;
    /*endif*/
    pkt[0] = V150_1_MSGID_I_CHAR_STAT;
    pkt[1] = s->near.data_format_code;
    memcpy(&pkt[2], buf, len);
    len += 2;
    return len;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_build_i_char_dyn(v150_1_state_t *s, uint8_t pkt[], int max_len, const uint8_t buf[], int len)
{
    if (!s->far.i_char_dyn_available)
        return -1;
    /*endif*/
    if (len > max_len - 2)
        return -1;
    /*endif*/
    pkt[0] = V150_1_MSGID_I_CHAR_DYN;
    pkt[1] = s->near.data_format_code;
    memcpy(&pkt[2], buf, len);
    len += 2;
    return len;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_build_i_frame(v150_1_state_t *s, uint8_t pkt[], int max_len, const uint8_t buf[], int len)
{
    int data_frame_state;

    data_frame_state = 0;

    if (!s->far.i_frame_available)
        return -1;
    /*endif*/
    if (len > max_len - 2)
        return -1;
    /*endif*/
    pkt[0] = V150_1_MSGID_I_FRAME;
    pkt[1] = data_frame_state & 0x03;
    memcpy(&pkt[2], buf, len);
    len += 2;
    return len;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_build_i_octet_cs(v150_1_state_t *s, uint8_t pkt[], int max_len, const uint8_t buf[], int len)
{
    if (!s->far.i_octet_cs_available)
        return -1;
    /*endif*/
    if (len > max_len - 3)
        return -1;
    /*endif*/
    pkt[0] = V150_1_MSGID_I_OCTET_CS;
    put_net_unaligned_uint16(&pkt[1], s->near.octet_cs_next_seq_no & 0xFFFF);
    memcpy(&pkt[3], buf, len);
    s->near.octet_cs_next_seq_no += len;
    len += 3;
    return len;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_build_i_char_stat_cs(v150_1_state_t *s, uint8_t pkt[], int max_len, const uint8_t buf[], int len)
{
    if (!s->far.i_char_stat_cs_available)
        return -1;
    /*endif*/
    if (len > max_len - 4)
        return -1;
    /*endif*/
    pkt[0] = V150_1_MSGID_I_CHAR_STAT_CS;
    pkt[1] = s->near.data_format_code;
    put_net_unaligned_uint16(&pkt[2], s->near.octet_cs_next_seq_no & 0xFFFF);
    memcpy(&pkt[4], buf, len);
    len += 4;
    s->near.octet_cs_next_seq_no += len;
    return len;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_build_i_char_dyn_cs(v150_1_state_t *s, uint8_t pkt[], int max_len, const uint8_t buf[], int len)
{
    if (!s->far.i_char_dyn_cs_available)
        return -1;
    /*endif*/
    if (len > max_len - 4)
        return -1;
    /*endif*/
    pkt[0] = V150_1_MSGID_I_CHAR_DYN_CS;
    pkt[1] = s->near.data_format_code;
    put_net_unaligned_uint16(&pkt[2], s->near.octet_cs_next_seq_no & 0xFFFF);
    memcpy(&pkt[4], buf, len);
    s->near.octet_cs_next_seq_no += len;
    len += 4;
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_tx_info_stream(v150_1_state_t *s, const uint8_t buf[], int len)
{
    uint8_t pkt[256];
    int max_len;
    int res;

    max_len = s->near.max_payload_bytes[s->near.info_stream_channel];
    switch (s->near.info_stream_msg_id)
    {
    case V150_1_MSGID_I_RAW_OCTET:
        res = v150_1_build_i_raw_octet(s, pkt, max_len, buf, len);
        break;
    case V150_1_MSGID_I_RAW_BIT:
        res = v150_1_build_i_raw_bit(s, pkt, max_len, buf, len);
        break;
    case V150_1_MSGID_I_OCTET:
        res = v150_1_build_i_octet(s, pkt, max_len, buf, len);
        break;
    case V150_1_MSGID_I_CHAR_STAT:
        res = v150_1_build_i_char_stat(s, pkt, max_len, buf, len);
        break;
    case V150_1_MSGID_I_CHAR_DYN:
        res = v150_1_build_i_char_dyn(s, pkt, max_len, buf, len);
        break;
    case V150_1_MSGID_I_FRAME:
        res = v150_1_build_i_frame(s, pkt, max_len, buf, len);
        break;
    case V150_1_MSGID_I_OCTET_CS:
        res = v150_1_build_i_octet_cs(s, pkt, max_len, buf, len);
        break;
    case V150_1_MSGID_I_CHAR_STAT_CS:
        res = v150_1_build_i_char_stat_cs(s, pkt, max_len, buf, len);
        break;
    case V150_1_MSGID_I_CHAR_DYN_CS:
        res = v150_1_build_i_char_dyn_cs(s, pkt, max_len, buf, len);
        break;
    default:
        res = -1;
        break;
    }
    /*endswitch*/
    if (res >= 0)
    {
        if (s->tx_packet_handler)
            res = s->tx_packet_handler(s->tx_packet_user_data, s->near.info_stream_channel, pkt, res);
        /*endif*/
    }
    else
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Bad message\n");
    }
    /*endif*/
    return res;
}
/*- End of function --------------------------------------------------------*/

static int select_info_msg_type(v150_1_state_t *s)
{
    int i;

    /* Select the first available information message type we find in the preferences list */
    for (i = 0;  i < 10  &&  s->near.info_msg_preferences[i] >= 0;  i++)
    {
        switch (s->near.info_msg_preferences[i])
        {
        case V150_1_MSGID_I_RAW_OCTET:
            /* This is always supported */
            s->near.info_stream_msg_id = s->near.info_msg_preferences[i];
            return 0;
        case V150_1_MSGID_I_RAW_BIT:
            if (s->near.i_raw_bit_available)
            {
                s->near.info_stream_msg_id = s->near.info_msg_preferences[i];
                return 0;
            }
            /*endif*/
            break;
        case V150_1_MSGID_I_OCTET:
            /* This is always supported */
            s->near.info_stream_msg_id = s->near.info_msg_preferences[i];
            return 0;
        case V150_1_MSGID_I_CHAR_STAT:
            if (s->near.i_char_stat_available)
            {
                s->near.info_stream_msg_id = s->near.info_msg_preferences[i];
                return 0;
            }
            /*endif*/
            break;
        case V150_1_MSGID_I_CHAR_DYN:
            if (s->near.i_char_dyn_available)
            {
                s->near.info_stream_msg_id = s->near.info_msg_preferences[i];
                return 0;
            }
            /*endif*/
            break;
        case V150_1_MSGID_I_FRAME:
            if (s->near.i_frame_available)
            {
                s->near.info_stream_msg_id = s->near.info_msg_preferences[i];
                return 0;
            }
            /*endif*/
            break;
        case V150_1_MSGID_I_OCTET_CS:
            if (s->near.i_octet_cs_available)
            {
                s->near.info_stream_msg_id = s->near.info_msg_preferences[i];
                return 0;
            }
            /*endif*/
            break;
        case V150_1_MSGID_I_CHAR_STAT_CS:
            if (s->near.i_char_stat_cs_available)
            {
                s->near.info_stream_msg_id = s->near.info_msg_preferences[i];
                return 0;
            }
            /*endif*/
            break;
        case V150_1_MSGID_I_CHAR_DYN_CS:
            if (s->near.i_char_dyn_cs_available)
            {
                s->near.info_stream_msg_id = s->near.info_msg_preferences[i];
                return 0;
            }
            /*endif*/
            break;
        default:
            s->near.info_stream_msg_id = -1;
            return -1;
        }
        /*endswitch*/
    }
    /*endfor*/
    s->near.info_stream_msg_id = -1;
    return -1;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_process_null(v150_1_state_t *s, const uint8_t buf[], int len)
{
    if (len != 1)
        return -1;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_process_init(v150_1_state_t *s, const uint8_t buf[], int len)
{
    if (len != 3)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "Invalid INIT message length %d\n", len);
        return -1;
    }
    /*endif*/
    /* Just capture what the far end says about its capabilities */
    s->far.necrxch_option = (buf[1] & 0x80) != 0;
    s->far.ecrxch_option = (buf[1] & 0x40) != 0;
    s->far.xid_profile_exchange_supported = (buf[1] & 0x20) != 0;
    s->far.asymmetric_data_types_supported = (buf[1] & 0x10) != 0;
    s->far.i_raw_bit_supported = (buf[1] & 0x08) != 0;
    s->far.i_frame_supported = (buf[1] & 0x04) != 0;
    s->far.i_char_stat_supported = (buf[1] & 0x02) != 0;
    s->far.i_char_dyn_supported = (buf[1] & 0x01) != 0;
    s->far.i_octet_cs_supported = (buf[2] & 0x80) != 0;
    s->far.i_char_stat_cs_supported = (buf[2] & 0x40) != 0;
    s->far.i_char_dyn_cs_supported = (buf[2] & 0x20) != 0;

    /* Now sift out what will be available, because both ends support the features */
    s->near.i_raw_bit_available  = s->near.i_raw_bit_supported  &&  s->far.i_raw_bit_supported;
    s->near.i_frame_available = s->near.i_frame_supported  &&  s->far.i_frame_supported;
    s->near.i_octet_with_dlci_available = s->near.dlci_supported;
    s->near.i_octet_without_dlci_available = !s->near.dlci_supported;
    s->near.i_char_stat_available = s->near.i_char_stat_supported  &&  s->far.i_char_stat_supported;
    s->near.i_char_dyn_available = s->near.i_char_dyn_supported  &&  s->far.i_char_dyn_supported;
    s->near.i_octet_cs_available = s->near.i_octet_cs_supported  &&  s->far.i_octet_cs_supported;
    s->near.i_char_stat_cs_available = s->near.i_char_stat_cs_supported  &&  s->far.i_char_stat_cs_supported;
    s->near.i_char_dyn_cs_available = s->near.i_char_dyn_cs_supported  &&  s->far.i_char_dyn_cs_supported;

    span_log(&s->logging, SPAN_LOG_FLOW, "    Preferred non-error controlled Rx channel: %s\n", (s->far.necrxch_option)  ?  "RSC"  :  "USC");
    span_log(&s->logging, SPAN_LOG_FLOW, "    Preferred error controlled Rx channel: %s\n", (s->far.necrxch_option)  ?  "USC"  :  "RSC");
    span_log(&s->logging, SPAN_LOG_FLOW, "    XID profile exchange  %ssupported\n", (s->far.xid_profile_exchange_supported)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    Asymmetric data types %ssupported\n", (s->far.asymmetric_data_types_supported)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_RAW-CHAR            supported\n");
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_RAW-BIT             %ssupported\n", (s->far.i_raw_bit_supported)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_FRAME               %ssupported\n", (s->far.i_frame_supported)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_OCTET %s     supported\n", (s->near.dlci_supported)  ?  "(DLCI)   "  :  "(no DLCI)");
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_CHAR-STAT           %ssupported\n", (s->far.i_char_stat_supported)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_CHAR-DYN            %ssupported\n", (s->far.i_char_dyn_supported)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_OCTET-CS            %ssupported\n", (s->far.i_octet_cs_supported)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_CHAR-STAT-CS        %ssupported\n", (s->far.i_char_stat_cs_supported)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_CHAR-DYN-CS         %ssupported\n", (s->far.i_char_dyn_cs_supported)  ?  ""  :  "not ");
    select_info_msg_type(s);

    s->far.connection_state = V150_1_STATE_INITED;
    if (s->near.connection_state >= V150_1_STATE_INITED)
        s->joint_connection_state = V150_1_STATE_INITED;
    /*endif*/
    status_report(s, V150_1_STATUS_REASON_STATE_CHANGED);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_process_xid_xchg(v150_1_state_t *s, const uint8_t buf[], int len)
{
    if (s->joint_connection_state < V150_1_STATE_INITED)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "XID_XCHG received before INIT. Ignored.\n");
        return -1;
    }
    /*endif*/
    if (len != 19)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "Invalid XID_XCHG message length %d\n", len);
        return -1;
    }
    /*endif*/
    s->far.ecp = buf[1];

    s->far.v42bis_supported = (buf[2] & 0x80) != 0;
    s->far.v44_supported = (buf[2] & 0x40) != 0;
    s->far.mnp5_supported = (buf[2] & 0x20) != 0;

    s->far.v42bis_p0 = buf[3];
    s->far.v42bis_p1 = get_net_unaligned_uint16(&buf[4]);
    s->far.v42bis_p2 = buf[6];
    s->far.v44_c0 = buf[7];
    s->far.v44_p0 = buf[8];
    s->far.v44_p1t = get_net_unaligned_uint16(&buf[9]);
    s->far.v44_p1r = get_net_unaligned_uint16(&buf[11]);
    s->far.v44_p2t = buf[13];
    s->far.v44_p2r = buf[14];
    s->far.v44_p3t = get_net_unaligned_uint16(&buf[15]);
    s->far.v44_p3r = get_net_unaligned_uint16(&buf[17]);

    span_log(&s->logging, SPAN_LOG_FLOW, "    V.42bis %ssupported\n", (s->far.v42bis_supported)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44    %ssupported\n", (s->far.v44_supported)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    MNP5    %ssupported\n", (s->far.mnp5_supported)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.42bis P0 %d\n", s->far.v42bis_p0);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.42bis P1 %d\n", s->far.v42bis_p1);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.42bis P2 %d\n", s->far.v42bis_p2);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 C0 %d\n", s->far.v44_c0);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 P1 %d\n", s->far.v44_p0);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 P1T %d\n", s->far.v44_p1t);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 P1R %d\n", s->far.v44_p1r);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 P2T %d\n", s->far.v44_p2t);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 P2R %d\n", s->far.v44_p2r);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 P3T %d\n", s->far.v44_p3t);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 P3R %d\n", s->far.v44_p3r);

    /* TODO: */
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_process_jm_info(v150_1_state_t *s, const uint8_t buf[], int len)
{
    int i;
    int id;

    if (s->joint_connection_state < V150_1_STATE_INITED)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "JM_INFO received before INIT. Ignored.\n");
        return -1;
    }
    /*endif*/
    /* The length must be even */
    if ((len & 1) != 1)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "Invalid JM_INFO message length %d\n", len);
        return -1;
    }
    /*endif*/
    for (i = 1;  i < len;  i += 2)
    {
        id = (buf[i] >> 4) & 0x0F;
        s->far.jm_category_id_seen[id] = true;
        s->far.jm_category_info[id] = get_net_unaligned_uint16(&buf[i]) & 0x0FFF;
    }
    /*endfor*/
    for (i = 1;  i < 16;  i++)
    {
        if (s->far.jm_category_id_seen[i])
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "    JM %d 0x%x\n", i, s->far.jm_category_info[i]);
        }
        /*endif*/
    }
    /*endfor*/

    /* TODO: */
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_process_start_jm(v150_1_state_t *s, const uint8_t buf[], int len)
{
    if (s->joint_connection_state < V150_1_STATE_INITED)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "START_JM received before INIT. Ignored.\n");
        return -1;
    }
    /*endif*/
    if (len > 1)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "Invalid START_JM message length %d\n", len);
        return -1;
    }
    /*endif*/

    /* TODO: */
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_process_connect(v150_1_state_t *s, const uint8_t buf[], int len)
{
    int available_data_types;

    if (s->joint_connection_state < V150_1_STATE_INITED)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "CONNECT received before INIT. Ignored.\n");
        return -1;
    }
    /*endif*/
    if (len < 9  ||  len > 19)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "Invalid CONNECT message length %d\n", len);
        return -1;
    }
    /*endif*/
    s->far.selmod = (buf[1] >> 2) & 0x3F;
    s->far.selected_compression_direction = buf[1] & 0x03;
    s->far.selected_compression = (buf[2] >> 4) & 0x0F;
    s->far.selected_error_correction = buf[2] & 0x0F;
    s->far.tdsr = get_net_unaligned_uint16(&buf[3]);
    s->far.rdsr = get_net_unaligned_uint16(&buf[5]);

    available_data_types = get_net_unaligned_uint16(&buf[7]);
    s->far.i_octet_with_dlci_available = (available_data_types & 0x8000) != 0;
    s->far.i_octet_without_dlci_available = (available_data_types & 0x4000) != 0;
    s->far.i_raw_bit_available = (available_data_types & 0x2000) != 0;
    s->far.i_frame_available = (available_data_types & 0x1000) != 0;
    s->far.i_char_stat_available = (available_data_types & 0x0800) != 0;
    s->far.i_char_dyn_available = (available_data_types & 0x0400) != 0;
    s->far.i_octet_cs_available = (available_data_types & 0x0200) != 0;
    s->far.i_char_stat_cs_available = (available_data_types & 0x0100) != 0;
    s->far.i_char_dyn_cs_available = (available_data_types & 0x0080) != 0;

    span_log(&s->logging, SPAN_LOG_FLOW, "    Modulation %s\n", v150_1_modulation_to_str(s->far.selmod));
    span_log(&s->logging, SPAN_LOG_FLOW, "    Compression direction %s\n", v150_1_compression_direction_to_str(s->far.selected_compression_direction));
    span_log(&s->logging, SPAN_LOG_FLOW, "    Compression %s\n", v150_1_compression_to_str(s->far.selected_compression));
    span_log(&s->logging, SPAN_LOG_FLOW, "    Error correction %s\n", v150_1_error_correction_to_str(s->far.selected_error_correction));
    span_log(&s->logging, SPAN_LOG_FLOW, "    Tx data rate %d\n", s->far.tdsr);
    span_log(&s->logging, SPAN_LOG_FLOW, "    Rx data rate %d\n", s->far.rdsr);

    span_log(&s->logging, SPAN_LOG_FLOW, "    I_RAW-CHAR            available\n");
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_RAW-BIT             %savailable\n", (s->far.i_raw_bit_available)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_FRAME               %savailable\n", (s->far.i_frame_available)  ?  ""  :  "not ");
    if (s->far.i_octet_without_dlci_available  ||  s->far.i_octet_without_dlci_available)
    {
        if (s->far.i_octet_without_dlci_available)
            span_log(&s->logging, SPAN_LOG_FLOW, "    I_OCTET (no DLCI)     available\n");
        /*endif*/
        if (s->far.i_octet_with_dlci_available)
            span_log(&s->logging, SPAN_LOG_FLOW, "    I_OCTET (DLCI)        available\n");
        /*endif*/
    }
    else
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "    I_OCTET               not available\n");
    }
    /*endif*/
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_CHAR-STAT           %savailable\n", (s->far.i_char_stat_available)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_CHAR-DYN            %savailable\n", (s->far.i_char_dyn_available)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_OCTET-CS            %savailable\n", (s->far.i_octet_cs_available)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_CHAR-STAT-CS        %savailable\n", (s->far.i_char_stat_cs_available)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_CHAR-DYN-CS         %savailable\n", (s->far.i_char_dyn_cs_available)  ?  ""  :  "not ");

    if (len >= 15
        &&
        (s->far.selected_compression == V150_1_COMPRESSION_V42BIS  ||  s->far.selected_compression == V150_1_COMPRESSION_V44))
    {
        /* Selected_compression should be V150_1_COMPRESSION_V42BIS or V150_1_COMPRESSION_V44 */
        s->far.compression_tx_dictionary_size = get_net_unaligned_uint16(&buf[9]);
        s->far.compression_rx_dictionary_size = get_net_unaligned_uint16(&buf[11]);
        s->far.compression_tx_string_length = buf[13];
        s->far.compression_rx_string_length = buf[14];

        span_log(&s->logging, SPAN_LOG_FLOW, "    Tx dictionary size %d\n", s->far.compression_tx_dictionary_size);
        span_log(&s->logging, SPAN_LOG_FLOW, "    Rx dictionary size %d\n", s->far.compression_rx_dictionary_size);
        span_log(&s->logging, SPAN_LOG_FLOW, "    Tx string length %d\n", s->far.compression_tx_string_length);
        span_log(&s->logging, SPAN_LOG_FLOW, "    Rx string length %d\n", s->far.compression_rx_string_length);
    }
    else
    {
        s->far.compression_tx_dictionary_size = 0;
        s->far.compression_rx_dictionary_size = 0;
        s->far.compression_tx_string_length = 0;
        s->far.compression_rx_string_length = 0;
    }
    /*endif*/

    if (len >= 19
        &&
        s->far.selected_compression == V150_1_COMPRESSION_V44)
    {
        /* Selected_compression should be V150_1_COMPRESSION_V44 */
        s->far.compression_tx_history_size = get_net_unaligned_uint16(&buf[15]);
        s->far.compression_rx_history_size = get_net_unaligned_uint16(&buf[17]);

        span_log(&s->logging, SPAN_LOG_FLOW, "   Tx history size %d\n", s->far.compression_tx_history_size);
        span_log(&s->logging, SPAN_LOG_FLOW, "   Rx history size %d\n", s->far.compression_rx_history_size);
    }
    else
    {
        s->far.compression_tx_history_size = 0;
        s->far.compression_rx_history_size = 0;
    }
    /*endif*/

    s->far.connection_state = V150_1_STATE_CONNECTED;
    if (s->near.connection_state >= V150_1_STATE_CONNECTED)
        s->joint_connection_state = V150_1_STATE_CONNECTED;
    /*endif*/
    status_report(s, V150_1_STATUS_REASON_STATE_CHANGED);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_process_break(v150_1_state_t *s, const uint8_t buf[], int len)
{
    if (s->joint_connection_state != V150_1_STATE_CONNECTED)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "BREAK received before CONNECT. Ignored.\n");
        return -1;
    }
    /*endif*/
    if (len != 3)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "Invalid BREAK message length %d\n", len);
        return -1;
    }
    /*endif*/

    s->far.break_source = (buf[1] >> 4) & 0x0F;
    s->far.break_type = buf[1] & 0x0F;
    s->far.break_duration = buf[2];
    span_log(&s->logging, SPAN_LOG_FLOW, "Break source %s\n", v150_1_break_source_to_str(s->far.break_source));
    span_log(&s->logging, SPAN_LOG_FLOW, "Break type %s\n", v150_1_break_type_to_str(s->far.break_type));
    span_log(&s->logging, SPAN_LOG_FLOW, "Break len %d ms\n", s->far.break_duration*10);
    status_report(s, V150_1_STATUS_REASON_BREAK_RECEIVED);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_process_break_ack(v150_1_state_t *s, const uint8_t buf[], int len)
{
    if (s->joint_connection_state != V150_1_STATE_CONNECTED)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "BREAKACK received before CONNECT. Ignored.\n");
        return -1;
    }
    /*endif*/
    if (len != 1)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "Invalid BREAKACK message length %d\n", len);
        return -1;
    }
    /*endif*/

    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_process_mr_event(v150_1_state_t *s, const uint8_t buf[], int len)
{
    int event;
    int reason;

    if (s->joint_connection_state < V150_1_STATE_INITED)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "MR-EVENT received before INIT. Ignored.\n");
        return -1;
    }
    /*endif*/
    if (len < 3)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "Invalid MR_EVENT message length %d\n", len);
        return -1;
    }
    /*endif*/

    event = buf[1];
    span_log(&s->logging, SPAN_LOG_FLOW, "MR_EVENT type %s (%d) received\n", v150_1_mr_event_type_to_str(event), event);
    switch (event)
    {
    case V150_1_MR_EVENT_ID_NULL:
        if (len != 3)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "Invalid MR_EVENT message length %d\n", len);
            return -1;
        }
        /*endif*/
        break;
    case V150_1_MR_EVENT_ID_RATE_RENEGOTIATION:
    case V150_1_MR_EVENT_ID_RETRAIN:
        if (len != 3)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "Invalid MR_EVENT message length %d\n", len);
            return -1;
        }
        /*endif*/
        reason = buf[2];
        span_log(&s->logging, SPAN_LOG_FLOW, "    Reason %d\n", reason);
        if (event == V150_1_MR_EVENT_ID_RETRAIN)
        {
            s->far.connection_state = V150_1_STATE_RETRAIN;
            s->joint_connection_state = V150_1_STATE_RETRAIN;
            status_report(s, V150_1_STATUS_REASON_RATE_RETRAIN_RECEIVED);
        }
        else
        {
            s->far.connection_state = V150_1_STATE_RATE_RENEGOTIATION;
            s->joint_connection_state = V150_1_STATE_RATE_RENEGOTIATION;
            status_report(s, V150_1_STATUS_REASON_RATE_RENEGOTIATION_RECEIVED);
        }
        /*endif*/
        break;
    case V150_1_MR_EVENT_ID_PHYSUP:
        if (len != 10)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "Invalid MR_EVENT message length %d\n", len);
            return -1;
        }
        /*endif*/
        s->far.selmod = (buf[3] >> 2) & 0x3F;
        s->far.txsen = (buf[3] & 0x02) != 0;
        s->far.rxsen = (buf[3] & 0x01) != 0;
        s->far.tdsr = get_net_unaligned_uint16(&buf[4]);
        s->far.rdsr = get_net_unaligned_uint16(&buf[6]);
        s->far.txsr = buf[8];
        s->far.rxsr = buf[9];

        span_log(&s->logging, SPAN_LOG_FLOW, "    Selected modulation %s\n", v150_1_modulation_to_str(s->far.selmod));
        span_log(&s->logging, SPAN_LOG_FLOW, "    Tx data signalling rate %d\n", s->far.tdsr);
        span_log(&s->logging, SPAN_LOG_FLOW, "    Rx data signalling rate %d\n", s->far.rdsr);
        if (s->far.txsen)
            span_log(&s->logging, SPAN_LOG_FLOW, "    Tx symbol rate %s\n", v150_1_symbol_rate_to_str(s->far.txsr));
        /*endif*/
        if (s->far.rxsen)
            span_log(&s->logging, SPAN_LOG_FLOW, "    Rx symbol rate %s\n", v150_1_symbol_rate_to_str(s->far.rxsr));
        /*endif*/
        
        /* TODO: report these parameters */
        
        s->far.connection_state = V150_1_STATE_PHYSUP;
        if (s->near.connection_state >= V150_1_STATE_PHYSUP)
            s->joint_connection_state = V150_1_STATE_PHYSUP;
        /*endif*/
        status_report(s, V150_1_STATUS_REASON_STATE_CHANGED);
        break;
    default:
        span_log(&s->logging, SPAN_LOG_WARNING, "Unknown MR_EVENT type %d received\n", event);
        break;
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_process_cleardown(v150_1_state_t *s, const uint8_t buf[], int len)
{
    if (s->joint_connection_state < V150_1_STATE_INITED)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "CLEARDOWN received before INIT. Ignored.\n");
        return -1;
    }
    /*endif*/
    if (len != 4)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "Invalid CLEARDOWN message length %d\n", len);
        return -1;
    }
    /*endif*/

    s->far.cleardown_reason = buf[1];
    span_log(&s->logging, SPAN_LOG_FLOW, "    Reason %s\n", v150_1_cleardown_reason_to_str(s->far.cleardown_reason));
    // vendor = buf[2];
    // vendor_info = buf[3];
    /* A cleardown moves everything back to square one. */
    s->far.connection_state = V150_1_STATE_IDLE;
    status_report(s, V150_1_STATUS_REASON_STATE_CHANGED);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_process_prof_xchg(v150_1_state_t *s, const uint8_t buf[], int len)
{
    if (s->joint_connection_state < V150_1_STATE_INITED)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "PROF_XCHG received before INIT. Ignored.\n");
        return -1;
    }
    /*endif*/
    if (len != 19)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "Invalid PROF_XCHG message length %d\n", len);
        return -1;
    }
    /*endif*/

    /* The following have 3 way options - no, yes and unknown */
    s->far.v42_lapm_supported = (buf[1] & 0xC0) == 0x40;
    s->far.v42_annex_a_supported = (buf[1] & 0x30) == 0x10;
    s->far.v44_supported = (buf[1] & 0x0C) == 0x04;
    s->far.v42bis_supported = (buf[1] & 0x03) == 0x01;
    s->far.mnp5_supported = (buf[2] & 0xC0) == 0x40;

    s->far.v42bis_p0 = buf[3];
    s->far.v42bis_p1 = get_net_unaligned_uint16(&buf[4]);
    s->far.v42bis_p2 = buf[6];
    s->far.v44_c0 = buf[7];
    s->far.v44_p0 = buf[8];
    s->far.v44_p1t = get_net_unaligned_uint16(&buf[9]);
    s->far.v44_p1r = get_net_unaligned_uint16(&buf[11]);
    s->far.v44_p2t = buf[13];
    s->far.v44_p2r = buf[14];
    s->far.v44_p3t = get_net_unaligned_uint16(&buf[15]);
    s->far.v44_p3r = get_net_unaligned_uint16(&buf[17]);

    span_log(&s->logging, SPAN_LOG_FLOW, "    V.42 LAPM    %ssupported\n", (s->far.v42_lapm_supported)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.42 Annex A %ssupported\n", (s->far.v42_annex_a_supported)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44         %ssupported\n", (s->far.v44_supported)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.42bis      %ssupported\n", (s->far.v42bis_supported)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    MNP5         %ssupported\n", (s->far.mnp5_supported)  ?  ""  :  "not ");

    span_log(&s->logging, SPAN_LOG_FLOW, "    V.42bis P0 %d\n", s->far.v42bis_p0);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.42bis P1 %d\n", s->far.v42bis_p1);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.42bis P2 %d\n", s->far.v42bis_p2);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 C0 %d\n", s->far.v44_c0);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 P1 %d\n", s->far.v44_p0);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 P1T %d\n", s->far.v44_p1t);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 P1R %d\n", s->far.v44_p1r);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 P2T %d\n", s->far.v44_p2t);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 P2R %d\n", s->far.v44_p2r);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 P3T %d\n", s->far.v44_p3t);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 P3R %d\n", s->far.v44_p3r);

    /* TODO: */
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_process_i_raw_octet(v150_1_state_t *s, const uint8_t buf[], int len)
{
    int i;
    int l;
    int n;
    int header;

    if (s->joint_connection_state != V150_1_STATE_CONNECTED)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "I_RAW-OCTET received before CONNECT. Ignored.\n");
        return -1;
    }
    /*endif*/
    if (len < 2)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "Invalid I_RAW-OCTET message length %d\n", len);
        return -1;
    }
    /*endif*/
    l = buf[1] & 0x7F;
    if ((buf[1] & 0x80) != 0)
    {
        n = 1;
        header = 1;
    }
    else
    {
        n = buf[1] + 2;
        header = 2;
    }
    /*endif*/
    if (len != l + header)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "Invalid I_RAW-OCTET message length %d\n", len);
        return -1;
    }
    /*endif*/
    for (i = 0;  i < n;  i++)
    {
        if (s->rx_octet_handler)
            s->rx_octet_handler(s->rx_octet_handler_user_data, &buf[header], len - header, -1);
        /*endif*/
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_process_i_raw_bit(v150_1_state_t *s, const uint8_t buf[], int len)
{
    int i;
    int l;
    int p;
    int n;
    int header;

    if (s->joint_connection_state != V150_1_STATE_CONNECTED)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "I_RAW-BIT received before CONNECT. Ignored.\n");
        return -1;
    }
    /*endif*/
    if (len < 2)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "Invalid I_RAW-BIT message length %d\n", len);
        return -1;
    }
    /*endif*/
    if ((buf[1] & 0x80) == 0)
    {
        if ((buf[1] & 0x40) == 0)
        {
            l = buf[1] & 0x3F;
            p = 0;
        }
        else
        {
            l = (buf[1] >> 3) & 0x07;
            p = buf[1] & 0x07;
        }
        /*endif*/
        n = 1;
        header = 1;
    }
    else
    {
        l = (buf[1] >> 3) & 0x0F;
        p = buf[1] & 0x07;
        n = buf[2] + 2;
        header = 2;
    }
    /*endif*/
    if (len != l + header)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "Invalid I_RAW-BIT message length %d\n", len);
        return -1;
    }
    /*endif*/
    for (i = 0;  i < n;  i++)
    {
        if (s->rx_octet_handler)
            s->rx_octet_handler(s->rx_octet_handler_user_data, &buf[header], len - header, -1);
        /*endif*/
    }
    /*endfor*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_process_i_octet(v150_1_state_t *s, const uint8_t buf[], int len)
{
    int header;

    if (s->joint_connection_state != V150_1_STATE_CONNECTED)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "I_OCTET received before CONNECT. Ignored.\n");
        return -1;
    }
    /*endif*/
    if (len < 2)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "Invalid I_OCTET message length %d\n", len);
        return -1;
    }
    /*endif*/
    if (s->far.i_octet_with_dlci_available)
    {
        /* DLCI is one or two bytes (usually just 1). The low bit of each byte is an extension
           bit, allowing for a variable number of bytes. */
        if (len < 2)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "I_OCTET with DLCI has no DLCI field\n"); 
        }
        else
        {
            if ((buf[1] & 0x01) == 0)
            {
                if ((buf[2] & 0x01) == 0)
                    span_log(&s->logging, SPAN_LOG_WARNING, "I_OCTET with DLCI has bad DLCI field\n"); 
                /*endif*/
                header = 3;
                s->far.dlci = get_net_unaligned_uint16(&buf[1]);
            }
            else
            {
                header = 2;
                s->far.dlci = buf[1];
            }
            /*endif*/
        }
        /*endif*/
    }
    else
    {
        header = 1;
    }
    /*endif*/
    if (len > header)
    {
        if (s->rx_octet_handler)
            s->rx_octet_handler(s->rx_octet_handler_user_data, &buf[header], len - header, -1);
        /*endif*/
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_process_i_char_stat(v150_1_state_t *s, const uint8_t buf[], int len)
{
    if (s->joint_connection_state != V150_1_STATE_CONNECTED)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "I_CHAR-STAT received before CONNECT. Ignored.\n");
        return -1;
    }
    /*endif*/
    if (len < 2)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "Invalid I_CHAR-STAT message length %d\n", len);
        return -1;
    }
    /*endif*/
    if (s->far.data_format_code != buf[1])
    {
        /* Every packet in a session should have the same data format code */
        s->far.data_format_code = buf[1];
        status_report(s, V150_1_STATUS_REASON_DATA_FORMAT_CHANGED);
    }
    /*endif*/
    if (len > 2)
    {
        if (s->rx_octet_handler)
            s->rx_octet_handler(s->rx_octet_handler_user_data, &buf[2], len - 2, -1);
        /*endif*/
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_process_i_char_dyn(v150_1_state_t *s, const uint8_t buf[], int len)
{
    if (s->joint_connection_state != V150_1_STATE_CONNECTED)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "I_CHAR-DYN received before CONNECT. Ignored.\n");
        return -1;
    }
    /*endif*/
    if (len < 2)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "Invalid I_CHAR-DYN message length %d\n", len);
        return -1;
    }
    /*endif*/
    if (s->far.data_format_code != buf[1])
    {
        s->far.data_format_code = buf[1];
        status_report(s, V150_1_STATUS_REASON_DATA_FORMAT_CHANGED);
    }
    /*endif*/
    if (len > 2)
    {
        if (s->rx_octet_handler)
            s->rx_octet_handler(s->rx_octet_handler_user_data, &buf[2], len - 2, -1);
        /*endif*/
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_process_i_frame(v150_1_state_t *s, const uint8_t buf[], int len)
{
    int res;
    int data_frame_state;

    if (s->joint_connection_state != V150_1_STATE_CONNECTED)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "I_FRAME received before CONNECT. Ignored.\n");
        return -1;
    }
    /*endif*/
    if (len < 2)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "Invalid I_FRAME message length %d\n", len);
        return -1;
    }
    /*endif*/
    res = (buf[1] >> 2) & 0x3F;
    if (res)
        span_log(&s->logging, SPAN_LOG_WARNING, "I_FRAME with non-zero 'res' field\n"); 
    /*endif*/
    data_frame_state = buf[1] & 0x03;
    if (len > 2)
    {
        if (s->rx_octet_handler)
            s->rx_octet_handler(s->rx_octet_handler_user_data, &buf[2], len - 2, -1);
        /*endif*/
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_process_i_octet_cs(v150_1_state_t *s, const uint8_t buf[], int len)
{
    int fill;
    int character_seq_no;

    if (s->joint_connection_state != V150_1_STATE_CONNECTED)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "I_OCTET-CS received before CONNECT. Ignored.\n");
        return -1;
    }
    /*endif*/
    if (len < 3)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "Invalid I_OCTET-CS message length %d\n", len);
        return -1;
    }
    /*endif*/
    character_seq_no = get_net_unaligned_uint16(&buf[1]);
    /* Check for a gap in the data */
    fill = (character_seq_no - s->far.octet_cs_next_seq_no) & 0xFFFF;
    if (s->rx_octet_handler)
        s->rx_octet_handler(s->rx_octet_handler_user_data, &buf[3], len - 3, fill);
    /*endif*/
    s->far.octet_cs_next_seq_no = (character_seq_no + len - 3) & 0xFFFF;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_process_i_char_stat_cs(v150_1_state_t *s, const uint8_t buf[], int len)
{
    int fill;
    int character_seq_no;

    if (s->joint_connection_state != V150_1_STATE_CONNECTED)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "I_CHAR-STAT-CS received before CONNECT. Ignored.\n");
        return -1;
    }
    /*endif*/
    if (len < 4)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "Invalid I_CHAR-STAT-CS message length %d\n", len);
        return -1;
    }
    /*endif*/
    if (s->far.data_format_code != buf[1])
    {
        /* Every packet in a session should have the same data format code */
        s->far.data_format_code = buf[1];
        status_report(s, V150_1_STATUS_REASON_DATA_FORMAT_CHANGED);
    }
    /*endif*/
    character_seq_no = get_net_unaligned_uint16(&buf[2]);
    /* Check for a gap in the data */
    fill = (character_seq_no - s->far.octet_cs_next_seq_no) & 0xFFFF;
    if (s->rx_octet_handler)
        s->rx_octet_handler(s->rx_octet_handler_user_data, &buf[4], len - 4, fill);
    /*endif*/
    s->far.octet_cs_next_seq_no = (character_seq_no + len - 4) & 0xFFFF;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_process_i_char_dyn_cs(v150_1_state_t *s, const uint8_t buf[], int len)
{
    int fill;
    int character_seq_no;

    if (s->joint_connection_state != V150_1_STATE_CONNECTED)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "I_CHAR-DYN-CS received before CONNECT. Ignored.\n");
        return -1;
    }
    /*endif*/
    if (len < 4)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "Invalid I_CHAR-DYN-CS message length %d\n", len);
        return -1;
    }
    /*endif*/
    if (s->far.data_format_code != buf[1])
    {
        s->far.data_format_code = buf[1];
        status_report(s, V150_1_STATUS_REASON_DATA_FORMAT_CHANGED);
    }
    /*endif*/
    character_seq_no = get_net_unaligned_uint16(&buf[2]);
    /* Check for a gap in the data */
    fill = (character_seq_no - s->far.octet_cs_next_seq_no) & 0xFFFF;
    if (s->rx_octet_handler)
        s->rx_octet_handler(s->rx_octet_handler_user_data, &buf[4], len - 4, fill);
    /*endif*/
    s->far.octet_cs_next_seq_no = (character_seq_no + len - 4) & 0xFFFF;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_process_rx_msg(v150_1_state_t *s, int chan, int seq_no, const uint8_t buf[], int len)
{
    int res;
    int msg_id;
    
    if (chan < SPRT_TCID_MIN  ||  chan > SPRT_TCID_MAX)
    {
        span_log(&s->logging, SPAN_LOG_ERROR, "Packet arrived on invalid channel %d\n", chan);
        return -1;
    }
    /*endif*/
    if ((buf[0] & 0x80))
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Don't know how to handle this\n");
        return -1;
    }
    msg_id = buf[0] & 0x7F;
    span_log(&s->logging, SPAN_LOG_FLOW, "Message %s received\n", v150_1_msg_id_to_str(msg_id));

    if (msg_id < sizeof(channel_check))
    {
        if ((channel_check[msg_id] & (1 << chan)) == 0)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Bad channel for message ID %d\n", msg_id);
            return -1;
        }
        /*endif*/
    }
    /*endif*/

    switch (msg_id)
    {
    case V150_1_MSGID_NULL:
        res = v150_1_process_null(s, buf, len);
        break;
    case V150_1_MSGID_INIT:
        res = v150_1_process_init(s, buf, len);
        break;
    case V150_1_MSGID_XID_XCHG:
        res = v150_1_process_xid_xchg(s, buf, len);
        break;
    case V150_1_MSGID_JM_INFO:
        res = v150_1_process_jm_info(s, buf, len);
        break;
    case V150_1_MSGID_START_JM:
        res = v150_1_process_start_jm(s, buf, len);
        break;
    case V150_1_MSGID_CONNECT:
        res = v150_1_process_connect(s, buf, len);
        break;
    case V150_1_MSGID_BREAK:
        res = v150_1_process_break(s, buf, len);
        break;
    case V150_1_MSGID_BREAKACK:
        res = v150_1_process_break_ack(s, buf, len);
        break;
    case V150_1_MSGID_MR_EVENT:
        res = v150_1_process_mr_event(s, buf, len);
        break;
    case V150_1_MSGID_CLEARDOWN:
        res = v150_1_process_cleardown(s, buf, len);
        break;
    case V150_1_MSGID_PROF_XCHG:
        res = v150_1_process_prof_xchg(s, buf, len);
        break;
    case V150_1_MSGID_I_RAW_OCTET:
        res = v150_1_process_i_raw_octet(s, buf, len);
        break;
    case V150_1_MSGID_I_RAW_BIT:
        res = v150_1_process_i_raw_bit(s, buf, len);
        break;
    case V150_1_MSGID_I_OCTET:
        res = v150_1_process_i_octet(s, buf, len);
        break;
    case V150_1_MSGID_I_CHAR_STAT:
        res = v150_1_process_i_char_stat(s, buf, len);
        break;
    case V150_1_MSGID_I_CHAR_DYN:
        res = v150_1_process_i_char_dyn(s, buf, len);
        break;
    case V150_1_MSGID_I_FRAME:
        res = v150_1_process_i_frame(s, buf, len);
        break;
    case V150_1_MSGID_I_OCTET_CS:
        res = v150_1_process_i_octet_cs(s, buf, len);
        break;
    case V150_1_MSGID_I_CHAR_STAT_CS:
        res = v150_1_process_i_char_stat_cs(s, buf, len);
        break;
    case V150_1_MSGID_I_CHAR_DYN_CS:
        res = v150_1_process_i_char_dyn_cs(s, buf, len);
        break;
    default:
        span_log(&s->logging, SPAN_LOG_FLOW, "Bad msg ID %d\n", msg_id);
        res = -1;
        break;
    }
    /*endswitch*/
    if (res < 0)
        span_log(&s->logging, SPAN_LOG_FLOW, "Bad message\n");
    /*endif*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_set_local_busy(v150_1_state_t *s, bool busy)
{
    bool previous_busy;

    previous_busy = s->near.busy;
    s->near.busy = busy;
    return previous_busy;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(bool) v150_1_get_far_busy_status(v150_1_state_t *s)
{
    return s->far.busy;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_set_local_tc_payload_bytes(v150_1_state_t *s, int channel, int max_len)
{
    if (channel < SPRT_TCID_MIN  ||  channel > SPRT_TCID_MAX)
        return -1;
    /*endif*/
    if (max_len < channel_parm_limits[channel].min_payload_bytes
        ||
        max_len > channel_parm_limits[channel].max_payload_bytes)
    {
        return -1;
    }
    /*endif*/
    s->near.max_payload_bytes[channel] = max_len;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_get_local_tc_payload_bytes(v150_1_state_t *s, int channel)
{
    if (channel < SPRT_TCID_MIN  ||  channel > SPRT_TCID_MAX)
        return -1;
    /*endif*/
    return s->near.max_payload_bytes[channel];
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_set_info_stream_tx_mode(v150_1_state_t *s, int channel, int msg_id)
{
    if (channel < SPRT_TCID_MIN  ||  channel > SPRT_TCID_MAX)
        return -1;
    /*endif*/
    switch (msg_id)
    {
    case V150_1_MSGID_I_RAW_OCTET:
    case V150_1_MSGID_I_RAW_BIT:
    case V150_1_MSGID_I_OCTET:
    case V150_1_MSGID_I_CHAR_STAT:
    case V150_1_MSGID_I_CHAR_DYN:
    case V150_1_MSGID_I_FRAME:
    case V150_1_MSGID_I_OCTET_CS:
    case V150_1_MSGID_I_CHAR_STAT_CS:
    case V150_1_MSGID_I_CHAR_DYN_CS:
        s->near.info_stream_channel = channel;
        s->near.info_stream_msg_id = msg_id;
        break;
    default:
        return -1;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_set_info_stream_msg_priorities(v150_1_state_t *s, int msg_ids[])
{
    int i;

    /* Check the list is valid */
    for (i = 0;  i < 10  &&  msg_ids[i] >= 0;  i++)
    {
        switch(msg_ids[i])
        {
        case V150_1_MSGID_I_RAW_OCTET:
        case V150_1_MSGID_I_RAW_BIT:
        case V150_1_MSGID_I_OCTET:
        case V150_1_MSGID_I_CHAR_STAT:
        case V150_1_MSGID_I_CHAR_DYN:
        case V150_1_MSGID_I_FRAME:
        case V150_1_MSGID_I_OCTET_CS:
        case V150_1_MSGID_I_CHAR_STAT_CS:
        case V150_1_MSGID_I_CHAR_DYN_CS:
            /* OK */
            break;
        default:
            return -1;
        }
        /*endswitch*/
    }
    /*endfor*/
    for (i = 0;  i < 10  &&  msg_ids[i] >= 0;  i++)
    {
        s->near.info_msg_preferences[i] = msg_ids[i];
    }
    /*endfor*/
    if (i < 10)
        s->near.info_msg_preferences[i] = -1;
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_set_modulation(v150_1_state_t *s, int modulation)
{
    s->near.selmod = modulation;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_set_compression_direction(v150_1_state_t *s, int compression_direction)
{
    s->near.selected_compression_direction = compression_direction;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_set_compression(v150_1_state_t *s, int compression)
{
    s->near.selected_compression = compression;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_set_compression_parameters(v150_1_state_t *s,
                                                    int tx_dictionary_size,
                                                    int rx_dictionary_size,
                                                    int tx_string_length,
                                                    int rx_string_length,
                                                    int tx_history_size,
                                                    int rx_history_size)
{
    s->near.compression_tx_dictionary_size = tx_dictionary_size;
    s->near.compression_rx_dictionary_size = rx_dictionary_size;
    s->near.compression_tx_string_length = tx_string_length;
    s->near.compression_rx_string_length = rx_string_length;
    /* These are only relevant for V.44 */
    s->near.compression_tx_history_size = tx_history_size;
    s->near.compression_rx_history_size = rx_history_size;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_set_error_correction(v150_1_state_t *s, int error_correction)
{
    s->near.selected_error_correction = error_correction;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_set_tx_symbol_rate(v150_1_state_t *s, bool enable, int rate)
{
    s->near.txsen = enable;
    s->near.txsr = (enable)  ?  rate  :  0;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_set_rx_symbol_rate(v150_1_state_t *s, bool enable, int rate)
{
    s->near.rxsen = enable;
    s->near.rxsr = (enable)  ?  rate  :  0;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_set_tx_data_signalling_rate(v150_1_state_t *s, int rate)
{
    s->near.tdsr = rate;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_set_rx_data_signalling_rate(v150_1_state_t *s, int rate)
{
    s->near.rdsr = rate;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) v150_1_get_logging_state(v150_1_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(v150_1_state_t *) v150_1_init(v150_1_state_t *s,
                                           v150_1_tx_packet_handler_t tx_packet_handler,
                                           void *tx_packet_user_data,
                                           v150_1_rx_packet_handler_t rx_packet_handler,
                                           void *rx_packet_user_data,
                                           v150_1_rx_octet_handler_t rx_octet_handler,
                                           void *rx_octet_handler_user_data,
                                           v150_1_rx_status_report_handler_t rx_status_report_handler,
                                           void *rx_status_report_user_data)
{
    if (tx_packet_handler == NULL  ||  rx_packet_handler == NULL  ||  rx_octet_handler == NULL  ||  rx_status_report_handler == NULL)
        return NULL;
    /*endif*/
    if (s == NULL)
    {
        if ((s = (v150_1_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
        /*endif*/
    }
    /*endif*/
    memset(s, 0, sizeof(*s));

    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "V.150.1");

    s->near.max_payload_bytes[SPRT_TCID_UNRELIABLE_UNSEQUENCED] = SPRT_DEFAULT_TC0_PAYLOAD_BYTES;
    s->near.max_payload_bytes[SPRT_TCID_RELIABLE_SEQUENCED] = SPRT_DEFAULT_TC1_PAYLOAD_BYTES;
    s->near.max_payload_bytes[SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED] = SPRT_DEFAULT_TC2_PAYLOAD_BYTES;
    s->near.max_payload_bytes[SPRT_TCID_UNRELIABLE_SEQUENCED] = SPRT_DEFAULT_TC3_PAYLOAD_BYTES;

    s->near.v42bis_p0 = 3;
    s->near.v42bis_p1 = 512;
    s->near.v42bis_p2 = 6;
    s->near.v44_c0 = 0;
    s->near.v44_p0 = 0;
    s->near.v44_p1t = 0;
    s->near.v44_p1r = 0;
    s->near.v44_p2t = 0;
    s->near.v44_p2r = 0;
    s->near.v44_p3t = 0;
    s->near.v44_p3r = 0;

    s->near.jm_category_id_seen[V150_1_JM_CATEGORY_ID_CALL_FUNCTION_1] = true;
    s->near.jm_category_info[V150_1_JM_CATEGORY_ID_CALL_FUNCTION_1] = V150_1_JM_CALL_FUNCTION_V_SERIES;
    s->near.jm_category_id_seen[V150_1_JM_CATEGORY_ID_MODULATION_MODES] = true;
    s->near.jm_category_info[V150_1_JM_CATEGORY_ID_MODULATION_MODES] =
          V150_1_JM_MODULATION_MODE_V34_AVAILABLE
        | V150_1_JM_MODULATION_MODE_V32_V32bis_AVAILABLE
        | V150_1_JM_MODULATION_MODE_V22_V22bis_AVAILABLE
        | V150_1_JM_MODULATION_MODE_V21_AVAILABLE;
    s->near.jm_category_id_seen[V150_1_JM_CATEGORY_ID_PROTOCOLS] = true;
    s->near.jm_category_info[V150_1_JM_CATEGORY_ID_PROTOCOLS] = V150_1_JM_PROTOCOL_V42_LAPM;
    s->near.jm_category_id_seen[V150_1_JM_CATEGORY_ID_PSTN_ACCESS] = true;
    s->near.jm_category_info[V150_1_JM_CATEGORY_ID_PSTN_ACCESS] = 0;
    s->near.jm_category_id_seen[V150_1_JM_CATEGORY_ID_PCM_MODEM_AVAILABILITY] = false;
    s->near.jm_category_info[V150_1_JM_CATEGORY_ID_PCM_MODEM_AVAILABILITY] = 0;
    s->near.jm_category_id_seen[V150_1_JM_CATEGORY_ID_EXTENSION] = false;
    s->near.jm_category_info[V150_1_JM_CATEGORY_ID_EXTENSION] = 0;

    s->near.selmod = V150_1_SELMOD_NULL;
    s->near.selected_compression_direction = V150_1_COMPRESS_NEITHER_WAY;
    s->near.selected_compression = V150_1_COMPRESSION_NONE;
    s->near.selected_error_correction = V150_1_ERROR_CORRECTION_NONE;
    s->near.tdsr = 0;
    s->near.rdsr = 0;
    s->near.txsen = false;
    s->near.txsr = V150_1_SYMBOL_RATE_NULL;
    s->near.rxsen = false;
    s->near.rxsr = V150_1_SYMBOL_RATE_NULL;

    /* Set default values that suit V.42bis */
    s->near.compression_tx_dictionary_size = 512;
    s->near.compression_rx_dictionary_size = 512;
    s->near.compression_tx_string_length = 6;
    s->near.compression_rx_string_length = 6;
    s->near.compression_tx_history_size = 0;
    s->near.compression_rx_history_size = 0;

    s->near.ecp = V150_1_ERROR_CORRECTION_V42_LAPM;
    s->near.v42_lapm_supported = true;
    s->near.v42_annex_a_supported = false;  /* This will never be supported, as it was removed from the V.42 spec in 2002. */
    s->near.v42bis_supported = true;
    s->near.v44_supported = false;
    s->near.mnp5_supported = false;

    s->near.necrxch_option = false;
    s->near.ecrxch_option = true;
    s->near.xid_profile_exchange_supported = false;
    s->near.asymmetric_data_types_supported = false;

    s->near.i_raw_bit_supported = false;
    s->near.i_frame_supported = false;
    s->near.i_char_stat_supported = false;
    s->near.i_char_dyn_supported = false;
    s->near.i_octet_cs_supported = true;
    s->near.i_char_stat_cs_supported = false;
    s->near.i_char_dyn_cs_supported = false;

    /* Set a default character format. */
    s->near.data_format_code = (V150_1_DATA_BITS_7 << 6)
                             | (V150_1_PARITY_EVEN << 3)
                             | V150_1_STOP_BITS_1;
    s->far.data_format_code = -1;

    s->tx_packet_handler = tx_packet_handler;
    s->tx_packet_user_data = tx_packet_user_data;
    s->rx_packet_handler = rx_packet_handler;
    s->rx_packet_user_data = rx_packet_user_data;
    s->rx_octet_handler = rx_octet_handler;
    s->rx_octet_handler_user_data = rx_octet_handler_user_data;
    s->rx_status_report_handler = rx_status_report_handler;
    s->rx_status_report_user_data = rx_status_report_user_data;

    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_release(v150_1_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_free(v150_1_state_t *s)
{
    int ret;

    ret = v150_1_release(s);
    span_free(s);
    return ret;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
