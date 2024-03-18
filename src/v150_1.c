/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v150_1.c - An implementation of V.150.1.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2022, 2023 Steve Underwood
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

#define SPANDSP_FULLY_DEFINE_SPRT_STATE_T

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/unaligned.h"
#include "spandsp/logging.h"
#include "spandsp/async.h"
#include "spandsp/sprt.h"
#include "spandsp/v150_1.h"
#include "spandsp/v150_1_sse.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/sprt.h"
#include "spandsp/private/v150_1_sse.h"
#include "spandsp/private/v150_1.h"

#include "v150_1_local.h"

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

    A U-MR needs to support V.92 digital, V.90 digital, V.34, V.32bis, V.32, V.22bis, V.22, V.23 and V.21

V-MR:   A V.8 Modem Relay
    A V-MR doesn't have to support any specific set of modulations. Instead, V.8 is used to negotiate a
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

txalgs=1 V.44       (V.42bis is always required, so is not listed in this tag)
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

    |<----------------------------------- Data compression ------------------------------------->|
    |<----------------------------------- Error correction ------------------------------------->|
    |<-------------------------------------- Modulation ---------------------------------------->|
    |                             |<---- Encapsulated G.711 ---->|                               |
    |                             |                              |                               |
    |<---------- PSTN ----------->|<-------Packet network ------>|<----------- PSTN ------------>|


The various modem relay error correction and compression scenarios:

MR1

    |<----------------------------------- Data compression --- --------------------------------->|
    |<---- Error correction ----->|                              |<----- Error correction ------>|
    |<------- Modulation -------->|                              |<-------- Modulation --------->|
    |                             |<---- Reliable transport ---->|                               |
    |                             |                              |                               |
    |<---------- PSTN ----------->|<-------Packet network ------>|<----------- PSTN ------------>|


MR2

    |<---- Data compression ----->|                              |<----- Data compression ------>|
    |<---- Error correction ----->|                              |<----- Error correction ------>|
    |<------- Modulation -------->|                              |<-------- Modulation --------->|
    |                             |<------- MR2a or MR2b ------->|                               |
    |                             |                              |                               |
    |<---------- PSTN ----------->|<-------Packet network ------>|<----------- PSTN ------------>|

MR2a: Reliable transport without data compression
MR2b: Reliable transport with data compression



MR3

    |<---- Data compression ----->|<-------------------- Data compression ---------------------->|
    |<------------------- Data compression --------------------->|<----- Data compression ------>|
    |<---- Error correction -0--->|                              |<----- Error correction ------>|
    |<------- Modulation -------->|                              |<-------- Modulation --------->|
    |                             |<---- Reliable transport ---->|                               |
    |                             |                              |                               |
    |<--------- PSTN ------------>|<------ Packet network ------>|<----------- PSTN ------------>|



MR4

    |<------------------- Data compression --------------------->|<----- Data compression ------>|
    |<---- Error correction ----->|                              |<----- Error correction ------>|
    |<------- Modulation -------->|                              |<-------- Modulation --------->|
    |                             |<---- Reliable transport ---->|                               |
    |                             |                              |                               |
    |<---------- PSTN ----------->|<-------Packet network ------>|<----------- PSTN ------------>|

*/

/* Example call flows

Establishing Modem Relay with V.32 Modem

    M1                            G1                             G2                              M2
    |                             |                              |                               |
    |                             |                              |<-------------ANS--------------|
    |                             |<--------RFC4733 ANS----------|                               |
    |<------------ANS-------------|                              |                               |
    |                             |                              |<------------/ANS--------------|
    |                             |<--------RFC4733 /ANS---------|                               |
    |<-----------/ANS-------------|                              |                               |
    |                             |                              |                               |
    |                             |                              |                               |
    |<<----- V.32 signals ------>>|                              |                               |
    |                             |-------SSE MR(m,a) AA-------->|                               |
    |                             |                              |<<------ V.32 signals ------->>|
    |                             |<------SSE MR(m,m) p'---------|                               |
    |<<----- V.32 signals ------>>|                              |                               |
    |                             |-----------SPRT:INIT--------->|<<------ V.32 signals ------->>|
    |                             |                              |                               |
    |                             |<----------SPRT:INIT----------|                               |
    |<<----- V.32 signals ------>>|                              |<<------ V.32 signals ------->>|
    |                             |<--SPRT:MR_EVENT(PHYSUPv32)---|                               |
    |                             |                              |                               |
    |                             |---SPRT:MR_EVENT(PHYSUPv32)-->|                               |
    |                             |                              |                               |
    |                             |<------SPRT:CONNECT(v32)------|                               |
    |                             |                              |                               |
    |                             |-------SPRT:CONNECT(v32)----->|                               |
    |                             |                              |                               |
    |<<------ V.32 data -------->>|<<-------- SPRT:data ------->>|<<-------- V.32 data -------->>|
    |                             |                              |                               |
    |                             |                              |                               |



Establishing Modem Relay with V.34 Modem

    M1                            G1                             G2                              M2
    |                             |                              |                               |
    |                             |                              |<-------------ANS--------------|
    |                             |<--------RFC4733 ANS----------|                               |
    |<-----------ANS--------------|                              |                               |
    |                             |                              |<------------/ANS--------------|
    |                             |<--------RFC4733 /ANS---------|                               |
    |<----------/ANS--------------|                              |                               |
    |                             |                              |                               |
    |------------CM-------------->|                              |                               |
    |                             |-----SSE MR(m,a) CM(v34)----->|                               |
    |                             |                              |--------------CM-------------->|
    |                             |<------SSE MR(m,m) p'---------|                               |
    |                             |                              |                               |
    |                             |-----------SPRT:INIT--------->|                               |
    |                             |                              |                               |
    |                             |<----------SPRT:INIT----------|                               |
    |                             |                              |<-------------JM---------------|
    |                             |<-----SPRT:JM_INFO(v34)-------|                               |
    |<-----------JM---------------|                              |                               |
    |                             |                              |                               |
    |<<----- V.34 signals ------>>|                              |<<------ V.34 signals ------->>|
    |                             |<--SPRT:MR_EVENT(PHYSUPv34)---|                               |
    |                             |                              |                               |
    |                             |---SPRT:MR_EVENT(PHYSUPv34)-->|                               |
    |                             |                              |                               |
    |                             |<------SPRT:CONNECT(v34)------|                               |
    |                             |                              |                               |
    |                             |-------SPRT:CONNECT(v34)----->|                               |
    |                             |                              |                               |
    |<<------ V.34 data -------->>|<<-------- SPRT:data ------->>|<<-------- V.34 data -------->>|
    |                             |                              |                               |
    |                             |                              |                               |



Establishing Modem Relay with ITU V.34 Modem with no JM_INFO Message Sent from G2 Gateway

    M1                            G1                             G2                              M2
    |                             |                              |                               |
    |                             |                              |<-------------ANS--------------|
    |                             |<--------RFC4733 ANS----------|                               |
    |<-----------ANS--------------|                              |                               |
    |                             |                              |<------------/ANS--------------|
    |                             |<--------RFC4733 /ANS---------|                               |
    |<----------/ANS--------------|                              |                               |
    |                             |                              |                               |
    |------------CM-------------->|                              |                               |
    |                             |-----SSE MR(m,a) CM(v34)----->|                               |
    |<-----------JM---------------|                              |--------------CM-------------->|
    |                             |<------SSE MR(m,m) p'---------|                               |
    |                             |                              |                               |
    |                             |-----------SPRT:INIT--------->|                               |
    |                             |                              |                               |
    |                             |<----------SPRT:INIT----------|                               |
    |                             |                              |<-------------JM---------------|
    |                             |                              |                               |
    |<<----- V.34 signals ------>>|                              |<<------ V.34 signals ------->>|
    |                             |<--SPRT:MR_EVENT(PHYSUPv34)---|                               |
    |                             |                              |                               |
    |                             |---SPRT:MR_EVENT(PHYSUPv34)-->|                               |
    |                             |                              |                               |
    |                             |<------SPRT:CONNECT(v34)------|                               |
    |                             |                              |                               |
    |                             |-------SPRT:CONNECT(v34)----->|                               |
    |                             |                              |                               |
    |<<------- V.34 data ------->>|<<-------- SPRT:data ------->>|<<-------- V.34 data -------->>|
    |                             |                              |                               |
    |                             |                              |                               |




Establishing Modem Relay with ITU V.90 Modem

    M1                            G1                             G2                              M2
    |                             |                              |                               |
    |                             |                              |<-------------ANS--------------|
    |                             |<--------RFC4733 ANS----------|                               |
    |<------------ANS-------------|                              |                               |
    |                             |                              |<------------/ANS--------------|
    |                             |<--------RFC4733 /ANS---------|                               |
    |<-----------/ANS-------------|                              |                               |
    |                             |                              |                               |
    |-------------CM------------->|                              |                               |
    |                             |--SSE MR(m,a) CM(v90 or v92)->|                               |
    |   `                         |                              |--------------CM-------------->|
    |                             |<------SSE MR(m,m) p'---------|                               |
    |                             |                              |                               |
    |                             |-----------SPRT:INIT--------->|                               |
    |                             |                              |                               |
    |                             |<----------SPRT:INIT----------|                               |
    |                             |                              |<-------------JM---------------|
    |                             |<--SPRT:JM_INFO (v90 or v92)--|                               |
    |<------------JM--------------|                              |                               |
    |                             |                              |                               |
    |<<----- V.90 signals ------>>|                              |<<------ V.90 signals ------->>|
    |                             |<--SPRT:MR_EVENT(PHYSUPv90)---|                               |
    |                             |                              |                               |
    |                             |<------SPRT:CONNECT(v90)------|                               |
    |                             |                              |                               |
    |                             |---SPRT:MR_EVENT(PHYSUPv90)-->|                               |
    |                             |                              |                               |
    |                             |-------SPRT:CONNECT(v90)----->|                               |
    |                             |                              |                               |
    |<<------- V.90 data ------->>|<<-------- SPRT:data ------->>|<<-------- V.90 data -------->>|
    |                             |                              |                               |
    |                             |                              |                               |




Establishing Modem Relay with ITU V.92 Modem

    M1                            G1                             G2                              M2
    |                             |                              |                               |
    |                             |                              |<--------------ANS-------------|
    |                             |<--------RFC4733 ANS----------|                               |
    |<------------ANS-------------|                              |                               |
    |                             |                              |<-------------/ANS-------------|
    |                             |<--------RFC4733 /ANS---------|                               |
    |<-----------/ANS-------------|                              |                               |
    |                             |                              |                               |
    |-------------CM------------->|                              |                               |
    |                             |--SSE MR(m,a) CM(v90 or v92)->|                               |
    |                             |                              |---------------CM------------->|
    |                             |<------SSE MR(m,m) p'---------|                               |
    |                             |                              |                               |
    |                             |-----------SPRT:INIT--------->|                               |
    |                             |                              |                               |
    |                             |<----------SPRT:INIT----------|                               |
    |                             |                              |<--------------JM--------------|
    |                             |<--SPRT:JM_INFO (v90 or v92)--|                               |
    |<------------JM--------------|                              |                               |
    |                             |                              |                               |
    |<<----- V.92 signals ------>>|                              |<<------- V.92 signals ------>>|
    |                             |<--SPRT:MR_EVENT(PHYSUPv92)---|                               |
    |                             |                              |                               |
    |                             |<------SPRT:CONNECT(v92)------|                               |
    |                             |                              |                               |
    |                             |---SPRT:MR_EVENT(PHYSUPv92)-->|                               |
    |                             |                              |                               |
    |                             |-------SPRT:CONNECT(v90)----->|                               |
    |                             |                              |                               |
    |<<------- V.92 data ------->>|<<-------- SPRT:data ------->>|<<-------- V.92 data -------->>|
    |                             |                              |                               |
    |                             |                              |                               |
*/

/*
               telephone network
                      ^
                      |
                      |
                      v
    +-----------------------------------+
    |                                   |
    |   Signal processing entity (SPE)  |
    |                                   |
    +-----------------------------------+
                |           ^
                |           |
  Signal list 1 |           | Signal list 2
                |           |
                v           |
    +-----------------------------------+   Signal list 5   +-----------------------------------+
    |                                   | ----------------->|                                   |
    |   SSE protocol state machine (P)  |                   |    Gateway state machine (s,s')   |
    |                                   |<------------------|                                   |
    +-----------------------------------+   Signal list 6   +-----------------------------------+
                |           ^
                |           |
  Signal list 3 |           | Signal list 4
                |           |
                v           |
    +-----------------------------------+
    |                                   |
    |       IP network processor        |
    |                                   |
    +-----------------------------------+
                      ^
                      |
                      |
                      v
                 IP network
*/

/*
    Table 31/V.150.1 - MoIP initial modes.
    
    <<<<<<<< Additional modes supported >>>>>>>>                    Starting mode
    FoIP (T.38) and/or ToIP (V.151)         VoIP
    -----------------------------------------------------------------------------
    No                                      No                          MoIP
    No                                      Yes                         VoIP
    Yes                                     No                          MoIP
    Yes                                     Yes                         VoIP
 */

/* Used to verify if a message type is compatible with the transmission
   control channel it arrives on */
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

static span_timestamp_t update_call_discrimination_timer(v150_1_state_t *s, span_timestamp_t timeout);

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

SPAN_DECLARE(const char *) v150_1_status_reason_to_str(int status)
{
    const char *res;

    res = "unknown";
    switch (status)
    {
    case V150_1_STATUS_REASON_NULL:
        res = "NULL";
        break;
    case V150_1_STATUS_REASON_MEDIA_STATE_CHANGED:
        res = "media state changed";
        break;
    case V150_1_STATUS_REASON_CONNECTION_STATE_CHANGED:
        res = "connection state changed";
        break;
    case V150_1_STATUS_REASON_DATA_FORMAT_CHANGED:
        res = "format changed";
        break;
    case V150_1_STATUS_REASON_BREAK_RECEIVED:
        res = "break received";
        break;
    case V150_1_STATUS_REASON_RATE_RETRAIN_RECEIVED:
        res = "retrain request received";
        break;
    case V150_1_STATUS_REASON_RATE_RENEGOTIATION_RECEIVED:
        res = "rate renegotiation received";
        break;
    case V150_1_STATUS_REASON_BUSY_CHANGED:
        res = "busy changed";
        break;
    case V150_1_STATUS_REASON_CONNECTION_STATE_PHYSUP:
        res = "physically up";
        break;
    case V150_1_STATUS_REASON_CONNECTION_STATE_CONNECTED:
        res = "connected";
        break;
    }
    /*endswitch*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) v150_1_jm_category_to_str(int category)
{
    const char *res;

    res = "unknown";
    switch (category)
    {
    case V150_1_JM_CATEGORY_ID_PROTOCOLS:
        res = "protocols";
        break;
    case V150_1_JM_CATEGORY_ID_CALL_FUNCTION_1:
        res = "call function 1";
        break;
    case V150_1_JM_CATEGORY_ID_MODULATION_MODES:
        res = "modulation modes";
        break;
    case V150_1_JM_CATEGORY_ID_PSTN_ACCESS:
        res = "PSTN access";
        break;
    case V150_1_JM_CATEGORY_ID_PCM_MODEM_AVAILABILITY:
        res = "PCM modem availability";
        break;
    case V150_1_JM_CATEGORY_ID_EXTENSION:
        res = "extension";
        break;
    }
    /*endswitch*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) v150_1_jm_info_modulation_to_str(int modulation)
{
    const char *res;

    res = "unknown";
    switch (modulation)
    {
    case V150_1_JM_MODULATION_MODE_V34_AVAILABLE:
        res = "V.34";
        break;
    case V150_1_JM_MODULATION_MODE_V34_HALF_DUPLEX_AVAILABLE:
        res = "V.34 half-duplex";
        break;
    case V150_1_JM_MODULATION_MODE_V32_V32bis_AVAILABLE:
        res = "V.32bis/V,32";
        break;
    case V150_1_JM_MODULATION_MODE_V22_V22bis_AVAILABLE:
        res = "V.22bis/V.22";
        break;
    case V150_1_JM_MODULATION_MODE_V17_AVAILABLE:
        res = "V.17";
        break;
    case V150_1_JM_MODULATION_MODE_V29_AVAILABLE:
        res = "V.29";
        break;
    case V150_1_JM_MODULATION_MODE_V27ter_AVAILABLE:
        res = "V.27ter";
        break;
    case V150_1_JM_MODULATION_MODE_V26ter_AVAILABLE:
        res = "V.26ter";
        break;
    case V150_1_JM_MODULATION_MODE_V26bis_AVAILABLE:
        res = "V.26bis";
        break;
    case V150_1_JM_MODULATION_MODE_V23_AVAILABLE:
        res = "V.23";
        break;
    case V150_1_JM_MODULATION_MODE_V23_HALF_DUPLEX_AVAILABLE:
        res = "V.23 half-duplex";
        break;
    case V150_1_JM_MODULATION_MODE_V21_AVAILABLE:
        res = "V.21";
        break;
    }
    /*endswitch*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) v150_1_signal_to_str(int modulation)
{
    const char *res;

    res = "unknown";
    switch (modulation)
    {
    case V150_1_SIGNAL_TONE_2100HZ:
        res = "2100Hz detected";
        break;
    case V150_1_SIGNAL_TONE_2225HZ:
        res = "2225Hz detected";
        break;
    case V150_1_SIGNAL_ANS:
        res = "V.25 ANS detected";
        break;
    case V150_1_SIGNAL_ANS_PR:
        res = "V.25 ANS reversal detected";
        break;
    case V150_1_SIGNAL_ANSAM:
        res = "V.8 ANSam detected";
        break;
    case V150_1_SIGNAL_ANSAM_PR:
        res = "V.8 ANSam reversal detected";
        break;
    case V150_1_SIGNAL_CI:
        res = "V.8 CI detected";
        break;
    case V150_1_SIGNAL_CM:
        res = "V.8 CM detected";
        break;
    case V150_1_SIGNAL_JM:
        res = "V.8 JM detected";
        break;
    case V150_1_SIGNAL_V21_LOW:
        res = "V.21 low channel detected";
        break;
    case V150_1_SIGNAL_V21_HIGH:
        res = "V.21 high channel detected";
        break;
    case V150_1_SIGNAL_V23_LOW:
        res = "V.23 low channel detected";
        break;
    case V150_1_SIGNAL_V23_HIGH:
        res = "V.23 high channel detected";
        break;
    case V150_1_SIGNAL_SB1:
        res = "V.22bis scrambled ones detected";
        break;
    case V150_1_SIGNAL_USB1:
        res = "V.22bis unscrambled ones detected";
        break;
    case V150_1_SIGNAL_S1:
        res = "V.22bis S1 detected";
        break;
    case V150_1_SIGNAL_AA:
        res = "V.32/V.32bis AA detected";
        break;
    case V150_1_SIGNAL_AC:
        res = "V.32/V.32bis AC detected";
        break;
    case V150_1_SIGNAL_CALL_DISCRIMINATION_TIMEOUT:
        res = "Call discrimination time-out";
        break;
    case V150_1_SIGNAL_UNKNOWN:
        res = "unrecognised signal detected";
        break;
    case V150_1_SIGNAL_SILENCE:
        res = "silence detected";
        break;
    case V150_1_SIGNAL_ABORT:
        res = "SPE has initiated an abort request";
        break;

    case V150_1_SIGNAL_ANS_GEN:
        res = "Generate V.25 ANS";
        break;
    case V150_1_SIGNAL_ANS_PR_GEN:
        res = "Generate V.25 ANS reversal";
        break;
    case V150_1_SIGNAL_ANSAM_GEN:
        res = "Generate V.8 ANSam";
        break;
    case V150_1_SIGNAL_ANSAM_PR_GEN:
        res = "Generate V.8 ANSam reversal";
        break;
    case V150_1_SIGNAL_2225HZ_GEN:
        res = "Generate 2225Hz";
        break;
    case V150_1_SIGNAL_CONCEAL_MODEM:
        res = "Block modem signal";
        break;
    case V150_1_SIGNAL_BLOCK_2100HZ_TONE:
        res = "Block 2100Hz";
        break;
    case V150_1_SIGNAL_AUTOMODE_ENABLE:
        res = "Enable automode";
        break;

    case V150_1_SIGNAL_AUDIO_GEN:
        res = "Send audio state";
        break;
    case V150_1_SIGNAL_FAX_RELAY_GEN:
        res = "Send fax relay state";
        break;
    case V150_1_SIGNAL_INDETERMINATE_GEN:
        res = "Send indeterminate state";
        break;
    case V150_1_SIGNAL_MODEM_RELAY_GEN:
        res = "Send modem relay state";
        break;
    case V150_1_SIGNAL_TEXT_RELAY_GEN:
        res = "Send text relay state";
        break;
    case V150_1_SIGNAL_VBD_GEN:
        res = "Send VBD state";
        break;
    case V150_1_SIGNAL_RFC4733_ANS_GEN:
        res = "Send RFC4733 ANS";
        break;
    case V150_1_SIGNAL_RFC4733_ANS_PR_GEN:
        res = "Send RFC4733 ANS reversal";
        break;
    case V150_1_SIGNAL_RFC4733_ANSAM_GEN:
        res = "Send RFC4733 ANSam";
        break;
    case V150_1_SIGNAL_RFC4733_ANSAM_PR_GEN:
        res = "Send RFC4733 ANSam reversal";
        break;
    case V150_1_SIGNAL_RFC4733_TONE_GEN:
        res = "Send RFC4733 tone";
        break;

    case V150_1_SIGNAL_AUDIO:
        res = "Audio state detected";
        break;
    case V150_1_SIGNAL_FAX_RELAY:
        res = "Facsimile relay state detected";
        break;
    case V150_1_SIGNAL_INDETERMINATE:
        res = "Indeterminate state detected";
        break;
    case V150_1_SIGNAL_MODEM_RELAY:
        res = "Modem relay state detected";
        break;
    case V150_1_SIGNAL_TEXT_RELAY:
        res = "Text relay state detected";
        break;
    case V150_1_SIGNAL_VBD:
        res = "VBD state detected";
        break;
    case V150_1_SIGNAL_RFC4733_ANS:
        res = "RFC4733 ANS event detected";
        break;
    case V150_1_SIGNAL_RFC4733_ANS_PR:
        res = "RFC4733 ANS reversal detected";
        break;
    case V150_1_SIGNAL_RFC4733_ANSAM:
        res = "RFC4733 ANSam detected";
        break;
    case V150_1_SIGNAL_RFC4733_ANSAM_PR:
        res = "RFC4733 ANSam reversal detected";
        break;
    case V150_1_SIGNAL_RFC4733_TONE:
        res = "RFC4733 tone detected";
        break;

    case V150_1_SIGNAL_AUDIO_STATE:
        res = "Audio";
        break;
    case V150_1_SIGNAL_FAX_RELAY_STATE:
        res = "Fax relay";
        break;
    case V150_1_SIGNAL_INDETERMINATE_STATE:
        res = "Indeterminate";
        break;
    case V150_1_SIGNAL_MODEM_RELAY_STATE:
        res = "Modem relay";
        break;
    case V150_1_SIGNAL_TEXT_RELAY_STATE:
        res = "Text relay";
        break;
    case V150_1_SIGNAL_VBD_STATE:
        res = "VBD";
        break;

    case V150_1_SIGNAL_CALL_DISCRIMINATION_TIMER_EXPIRED:
        res = "Call discrimination timer exposed";
        break;
    }
    /*endswitch*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) v150_1_media_state_to_str(int state)
{
    const char *res;

    res = "unknown";
    switch (state)
    {
    case V150_1_MEDIA_STATE_INITIAL_AUDIO:
        res = "Initial Audio";
        break;
    case V150_1_MEDIA_STATE_VOICE_BAND_DATA:
        res = "Voice Band Data (VBD)";
        break;
    case V150_1_MEDIA_STATE_MODEM_RELAY:
        res = "Modem Relay";
        break;
    case V150_1_MEDIA_STATE_FAX_RELAY:
        res = "Fax Relay";
        break;
    case V150_1_MEDIA_STATE_TEXT_RELAY:
        res = "Text Relay";
        break;
    case V150_1_MEDIA_STATE_TEXT_PROBE:
        res = "Text Probe";
        break;
    case V150_1_MEDIA_STATE_INDETERMINATE:
        res = "Indeterminate";
        break;
    }
    /*endswitch*/
    return res;
}
/*- End of function --------------------------------------------------------*/

static int status_report(v150_1_state_t *s, int reason)
{
    v150_1_status_t report;

    report.reason = reason;
    switch (reason)
    {
    case V150_1_STATUS_REASON_MEDIA_STATE_CHANGED:
        report.types.media_state_change.local_state = s->local_media_state;
        report.types.media_state_change.remote_state = s->remote_media_state;
        break;
    case V150_1_STATUS_REASON_CONNECTION_STATE_CHANGED:
        report.types.connection_state_change.state = s->far.parms.connection_state;
        report.types.connection_state_change.cleardown_reason = s->far.parms.cleardown_reason;
        break;
    case V150_1_STATUS_REASON_CONNECTION_STATE_PHYSUP:
        report.types.physup_parameters.selmod = s->far.parms.selmod;
        report.types.physup_parameters.tdsr = s->far.parms.tdsr;
        report.types.physup_parameters.rdsr = s->far.parms.rdsr;

        report.types.physup_parameters.txsen = s->far.parms.txsen;
        report.types.physup_parameters.txsr = s->far.parms.txsr;
        report.types.physup_parameters.rxsen = s->far.parms.rxsen;
        report.types.physup_parameters.rxsr = s->far.parms.rxsr;
        break;
    case V150_1_STATUS_REASON_CONNECTION_STATE_CONNECTED:
        report.types.connect_parameters.selmod = s->far.parms.selmod;
        report.types.connect_parameters.tdsr = s->far.parms.tdsr;
        report.types.connect_parameters.rdsr = s->far.parms.rdsr;

        report.types.connect_parameters.selected_compression_direction = s->far.parms.selected_compression_direction;
        report.types.connect_parameters.selected_compression = s->far.parms.selected_compression;
        report.types.connect_parameters.selected_error_correction = s->far.parms.selected_error_correction;

        report.types.connect_parameters.compression_tx_dictionary_size = s->far.parms.compression_tx_dictionary_size;
        report.types.connect_parameters.compression_rx_dictionary_size = s->far.parms.compression_rx_dictionary_size;
        report.types.connect_parameters.compression_tx_string_length = s->far.parms.compression_tx_string_length;
        report.types.connect_parameters.compression_rx_string_length = s->far.parms.compression_rx_string_length;
        report.types.connect_parameters.compression_tx_history_size = s->far.parms.compression_tx_history_size;
        report.types.connect_parameters.compression_rx_history_size = s->far.parms.compression_rx_history_size;

        /* I_RAW-OCTET is always available. There is no selection flag for it. */
        report.types.connect_parameters.i_raw_octet_available = true;
        report.types.connect_parameters.i_raw_bit_available = s->far.parms.i_raw_bit_available;
        report.types.connect_parameters.i_frame_available = s->far.parms.i_frame_available;
        /* I_OCTET is an oddity, as you need to know in advance whether there will be a DLCI field
           present. So, functionally its really like 2 different types of message. */
        report.types.connect_parameters.i_octet_with_dlci_available = s->far.parms.i_octet_with_dlci_available;
        report.types.connect_parameters.i_octet_without_dlci_available = s->far.parms.i_octet_without_dlci_available;
        report.types.connect_parameters.i_char_stat_available = s->far.parms.i_char_stat_available;
        report.types.connect_parameters.i_char_dyn_available = s->far.parms.i_char_dyn_available;
        /* Unlike I_OCTET, I_OCTET-CS is only defined without a DLCI field. */
        report.types.connect_parameters.i_octet_cs_available = s->far.parms.i_octet_cs_available;
        report.types.connect_parameters.i_char_stat_cs_available = s->far.parms.i_char_stat_cs_available;
        report.types.connect_parameters.i_char_dyn_cs_available = s->far.parms.i_char_dyn_cs_available;
        break;
    case V150_1_STATUS_REASON_DATA_FORMAT_CHANGED:
        report.types.data_format_change.bits = 5 + ((s->far.parms.data_format_code >> 5) & 0x03);
        report.types.data_format_change.parity_code = (s->far.parms.data_format_code >> 2) & 0x07;
        report.types.data_format_change.stop_bits = 1 + (s->far.parms.data_format_code & 0x03);
        break;
    case V150_1_STATUS_REASON_BREAK_RECEIVED:
        report.types.break_received.source = s->far.break_source;
        report.types.break_received.type = s->far.break_type;
        report.types.break_received.duration = s->far.break_duration*10;
        break;
    case V150_1_STATUS_REASON_RATE_RETRAIN_RECEIVED:
        break;
    case V150_1_STATUS_REASON_RATE_RENEGOTIATION_RECEIVED:
        break;
    case V150_1_STATUS_REASON_BUSY_CHANGED:
        report.types.busy_change.local_busy = s->near.parms.busy;
        report.types.busy_change.far_busy = s->far.parms.busy;
        break;
    }
    /*endswitch*/
    if (s->rx_status_report_handler)
        s->rx_status_report_handler(s->rx_status_report_user_data, &report);
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int send_spe_signal(v150_1_state_t *s, int signal)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Signal to SPE %s\n", v150_1_signal_to_str(signal));
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int send_ip_signal(v150_1_state_t *s, int signal)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Signal to IP %s\n", v150_1_signal_to_str(signal));
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int stop_timer(v150_1_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Stop timer\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int generic_macro(v150_1_state_t *s, int signal, int ric)
{
    span_timestamp_t now;

    /* Figure 25/V.150.1 */
    span_log(&s->logging,
             SPAN_LOG_FLOW,
             "IP signal %s(%s, %s)\n",
             v150_1_media_state_to_str(s->local_media_state),
             v150_1_signal_to_str(signal),
             v150_1_sse_moip_ric_to_str(ric));
    if (s->local_media_state == s->remote_media_state)
    {
        /* Stop the call discrimination timer */
        s->call_discrimination_timer = 0;
        update_call_discrimination_timer(s, s->call_discrimination_timer);
    }
    else
    {
        /* Start the call discrimination timer */
        if (s->call_discrimination_timer == 0)
        {
            now = update_call_discrimination_timer(s, ~0);
            s->call_discrimination_timer = now + s->call_discrimination_timeout;
            update_call_discrimination_timer(s, s->call_discrimination_timer);
        }
        /*endif*/
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void update_media_states(v150_1_state_t *s, int local, int remote)
{
    if (local != s->local_media_state  ||  remote != s->remote_media_state)
    {
        s->remote_media_state = remote;
        s->local_media_state = local;
        status_report(s, V150_1_STATUS_REASON_MEDIA_STATE_CHANGED);
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static int v150_1_figures_26_to_31(v150_1_state_t *s, int signal, const uint8_t *msg, int len)
{
    /* Figure 26/V.150.1 to 31/V.150.1 */
    switch (signal)
    {
    case V150_1_SIGNAL_TONE_2100HZ:
        if (s->cdscselect == V150_1_CDSCSELECT_VBD_PREFERRED
            ||
            s->cdscselect == V150_1_CDSCSELECT_MIXED)
        {
            update_media_states(s, V150_1_MEDIA_STATE_VOICE_BAND_DATA, s->remote_media_state);
            //send ANS or ANSam
            generic_macro(s, V150_1_SIGNAL_ANS, 0);
        }
        else
        {
            send_spe_signal(s, V150_1_SIGNAL_BLOCK_2100HZ_TONE);
        }
        /*endif*/
        break;
    case V150_1_SIGNAL_ANS:
        if (s->cdscselect == V150_1_CDSCSELECT_VBD_PREFERRED
            ||
            s->cdscselect == V150_1_CDSCSELECT_MIXED)
        {
            update_media_states(s, V150_1_MEDIA_STATE_VOICE_BAND_DATA, s->remote_media_state);
            generic_macro(s, V150_1_SIGNAL_ANS, 0);
        }
        else
        {
            generic_macro(s, V150_1_SIGNAL_RFC4733_ANS_GEN, 0);
            send_spe_signal(s, V150_1_SIGNAL_CONCEAL_MODEM);
        }
        /*endif*/
        break;
    case V150_1_SIGNAL_ANSAM:
        if (s->cdscselect == V150_1_CDSCSELECT_VBD_PREFERRED
            ||
            s->cdscselect == V150_1_CDSCSELECT_MIXED)
        {
            update_media_states(s, V150_1_MEDIA_STATE_VOICE_BAND_DATA, s->remote_media_state);
            generic_macro(s, V150_1_SIGNAL_ANSAM, 0);
        }
        else
        {
            generic_macro(s, V150_1_SIGNAL_RFC4733_ANSAM_GEN, 0);
            send_spe_signal(s, V150_1_SIGNAL_CONCEAL_MODEM);
        }
        /*endif*/
        break;
    case V150_1_SIGNAL_RFC4733_ANS:
        send_spe_signal(s, V150_1_SIGNAL_ANS_GEN);
        send_spe_signal(s, V150_1_SIGNAL_CONCEAL_MODEM);
        break;
    case V150_1_SIGNAL_RFC4733_ANSAM:
        send_spe_signal(s, V150_1_SIGNAL_ANSAM_GEN);
        send_spe_signal(s, V150_1_SIGNAL_CONCEAL_MODEM);
        break;
    case V150_1_SIGNAL_RFC4733_ANS_PR:
        send_spe_signal(s, V150_1_SIGNAL_ANS_PR_GEN);
        send_spe_signal(s, V150_1_SIGNAL_CONCEAL_MODEM);
        break;
    case V150_1_SIGNAL_RFC4733_ANSAM_PR:
        send_spe_signal(s, V150_1_SIGNAL_ANSAM_PR_GEN);
        send_spe_signal(s, V150_1_SIGNAL_CONCEAL_MODEM);
        break;
    case V150_1_SIGNAL_ANS_PR:
        break;
    case V150_1_SIGNAL_ANSAM_PR:
        break;
    case V150_1_SIGNAL_UNKNOWN:
    case V150_1_SIGNAL_CALL_DISCRIMINATION_TIMEOUT:
        if (s->cdscselect == V150_1_CDSCSELECT_VBD_PREFERRED
            ||
            s->cdscselect == V150_1_CDSCSELECT_MIXED)
        {
            update_media_states(s, V150_1_MEDIA_STATE_VOICE_BAND_DATA, s->remote_media_state);
            generic_macro(s, signal, 0);
        }
        /*endif*/
        break;
    case V150_1_SIGNAL_VBD:
        if (s->cdscselect == V150_1_CDSCSELECT_VBD_PREFERRED
            ||
            s->cdscselect == V150_1_CDSCSELECT_MIXED)
        {
            update_media_states(s, V150_1_MEDIA_STATE_VOICE_BAND_DATA, V150_1_MEDIA_STATE_VOICE_BAND_DATA);
            generic_macro(s, signal, 0);
        }
        else
        {
            update_media_states(s, V150_1_MEDIA_STATE_VOICE_BAND_DATA, s->remote_media_state);
            generic_macro(s, signal, 0);
        }
        /*endif*/
        break;
    case V150_1_SIGNAL_MODEM_RELAY:
        span_log(&s->logging, SPAN_LOG_FLOW, "Modem relay signal %s\n", v150_1_signal_to_str(signal));
        break;
    case V150_1_SIGNAL_CM:
        span_log(&s->logging, SPAN_LOG_FLOW, "SPE signal %s\n", v150_1_signal_to_str(signal));
        if (s->cdscselect == V150_1_CDSCSELECT_VBD_PREFERRED
            ||
            s->cdscselect == V150_1_CDSCSELECT_MIXED)
        {
            update_media_states(s, V150_1_MEDIA_STATE_VOICE_BAND_DATA, V150_1_MEDIA_STATE_MODEM_RELAY);
            generic_macro(s, V150_1_SIGNAL_MODEM_RELAY_GEN, V150_1_SSE_MOIP_RIC_V8_CM);
        }
        else
        {
            update_media_states(s, V150_1_MEDIA_STATE_MODEM_RELAY, V150_1_MEDIA_STATE_MODEM_RELAY);
            generic_macro(s, V150_1_SIGNAL_MODEM_RELAY_GEN, V150_1_SSE_MOIP_RIC_V8_CM);
        }
        /*endif*/
        break;
    /*supported modulations */
    default:
        span_log(&s->logging, SPAN_LOG_FLOW, "Unexpected signal %s\n", v150_1_signal_to_str(signal));
        break;
    }
    /*endswitch*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_figure_32(v150_1_state_t *s, int signal, const uint8_t *msg, int len)
{
    /* Figure 32/V.150.1 */
    switch (signal)
    {
    case V150_1_SIGNAL_AUDIO:
        //send SSE p'
        generic_macro(s, signal, 0);
        break;
    default:
        span_log(&s->logging, SPAN_LOG_FLOW, "Unexpected signal %s\n", v150_1_signal_to_str(signal));
        break;
    }
    /*endswitch*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_figure_33(v150_1_state_t *s, int signal, const uint8_t *msg, int len)
{
    /* Figure 33/V.150.1 */
    switch (signal)
    {
    case V150_1_SIGNAL_AUDIO:
        //send SSE p'
        generic_macro(s, signal, 0);
        break;
    default:
        span_log(&s->logging, SPAN_LOG_FLOW, "Unexpected signal %s\n", v150_1_signal_to_str(signal));
        break;
    }
    /*endswitch*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_figure_34(v150_1_state_t *s, int signal, const uint8_t *msg, int len)
{
    /* Figure 34/V.150.1 */
    switch (signal)
    {
    case V150_1_SIGNAL_AUDIO:
        //send SSE p'
        generic_macro(s, signal, 0);
        break;
    case V150_1_SIGNAL_MODEM_RELAY:
        stop_timer(s);
        break;
    case V150_1_SIGNAL_VBD:
        //send SSE RC
        generic_macro(s, signal, 0);
        break;
    default:
        span_log(&s->logging, SPAN_LOG_FLOW, "Unexpected signal %s\n", v150_1_signal_to_str(signal));
        break;
    }
    /*endswitch*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_figure_35(v150_1_state_t *s, int signal, const uint8_t *msg, int len)
{
    /* Figure 35/V.150.1 */
    switch (signal)
    {
    case V150_1_SIGNAL_JM:
        if (s->cdscselect == V150_1_CDSCSELECT_VBD_PREFERRED
            ||
            s->cdscselect == V150_1_CDSCSELECT_MIXED)
        {
        }
        else
        {
        }
        /*endif*/
        break;
    case V150_1_SIGNAL_VBD:
        update_media_states(s, s->local_media_state, V150_1_MEDIA_STATE_VOICE_BAND_DATA);
        break;
    default:
        span_log(&s->logging, SPAN_LOG_FLOW, "Unexpected signal %s\n", v150_1_signal_to_str(signal));
        break;
    }
    /*endswitch*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_figure_36(v150_1_state_t *s, int signal, const uint8_t *msg, int len)
{
    /* Figure 36/V.150.1 */
    switch (signal)
    {
    case V150_1_SIGNAL_AUDIO:
        update_media_states(s, V150_1_MEDIA_STATE_INITIAL_AUDIO, V150_1_MEDIA_STATE_VOICE_BAND_DATA);
        break;
    case V150_1_SIGNAL_MODEM_RELAY:
        stop_timer(s);
        break;
    case V150_1_SIGNAL_VBD:
        stop_timer(s);
        update_media_states(s, V150_1_MEDIA_STATE_INITIAL_AUDIO, V150_1_MEDIA_STATE_VOICE_BAND_DATA);
        // send sse p'
        generic_macro(s, signal, 0);
        break;
    default:
        span_log(&s->logging, SPAN_LOG_FLOW, "Unexpected signal %s\n", v150_1_signal_to_str(signal));
        break;
    }
    /*endswitch*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_figure_37(v150_1_state_t *s, int signal, const uint8_t *msg, int len)
{
    /* Figure 37/V.150.1 */
    switch (signal)
    {
    case V150_1_SIGNAL_AUDIO:
        update_media_states(s, V150_1_MEDIA_STATE_INITIAL_AUDIO, V150_1_MEDIA_STATE_INITIAL_AUDIO);
        break;
    case V150_1_SIGNAL_MODEM_RELAY:
        stop_timer(s);
        break;
    case V150_1_SIGNAL_VBD:
        stop_timer(s);
        break;
    default:
        span_log(&s->logging, SPAN_LOG_FLOW, "Unexpected signal %s\n", v150_1_signal_to_str(signal));
        break;
    }
    /*endswitch*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_figure_38(v150_1_state_t *s, int signal, const uint8_t *msg, int len)
{
    /* Figure 38/V.150.1 */
    switch (signal)
    {
    case V150_1_SIGNAL_AUDIO:
        update_media_states(s, V150_1_MEDIA_STATE_INITIAL_AUDIO, V150_1_MEDIA_STATE_INITIAL_AUDIO);
        break;
    case V150_1_SIGNAL_VBD:
        stop_timer(s);
        break;
    default:
        span_log(&s->logging, SPAN_LOG_FLOW, "Unexpected signal %s\n", v150_1_signal_to_str(signal));
        break;
    }
    /*endswitch*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_figure_39(v150_1_state_t *s, int signal, const uint8_t *msg, int len)
{
    /* Figure 39/V.150.1 */
    switch (signal)
    {
    case V150_1_SIGNAL_MODEM_RELAY:
        break;
    case V150_1_SIGNAL_CM:
        break;
    case V150_1_SIGNAL_RFC4733_ANS:
        send_spe_signal(s, V150_1_SIGNAL_ANS_GEN);
        break;
    case V150_1_SIGNAL_RFC4733_ANSAM:
        send_spe_signal(s, V150_1_SIGNAL_ANSAM_GEN);
        break;
    case V150_1_SIGNAL_RFC4733_ANS_PR:
        send_spe_signal(s, V150_1_SIGNAL_ANS_GEN);
        break;
    case V150_1_SIGNAL_RFC4733_ANSAM_PR:
        send_spe_signal(s, V150_1_SIGNAL_ANSAM_GEN);
        break;
    case V150_1_SIGNAL_ANS:
        if (s->rfc4733_preferred)
        {
            generic_macro(s, V150_1_SIGNAL_RFC4733_ANS_GEN, 0);
        }
        else
        {
            /* Pass the audio through */
        }
        /*endif*/
        break;
    case V150_1_SIGNAL_ANSAM:
        if (s->rfc4733_preferred)
        {
            generic_macro(s, V150_1_SIGNAL_RFC4733_ANSAM_GEN, 0);
        }
        else
        {
            /* Pass the audio through */
        }
        /*endif*/
        break;
    case V150_1_SIGNAL_ANS_PR:
        if (s->rfc4733_preferred)
        {
            generic_macro(s, V150_1_SIGNAL_RFC4733_ANS_PR_GEN, 0);
        }
        else
        {
            /* Pass the audio through */
        }
        /*endif*/
        break;
    case V150_1_SIGNAL_ANSAM_PR:
        if (s->rfc4733_preferred)
        {
            generic_macro(s, V150_1_SIGNAL_RFC4733_ANSAM_PR_GEN, 0);
        }
        else
        {
            /* Pass the audio through */
        }
        /*endif*/
        break;
    default:
        span_log(&s->logging, SPAN_LOG_FLOW, "Unexpected signal %s\n", v150_1_signal_to_str(signal));
        break;
    }
    /*endswitch*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_state_machine(v150_1_state_t *s, int signal, const uint8_t *msg, int len)
{
    span_log(&s->logging,
             SPAN_LOG_FLOW,
             "State machine - %s   %s   %s\n",
             v150_1_media_state_to_str(s->local_media_state),
             v150_1_media_state_to_str(s->remote_media_state),
             v150_1_signal_to_str(signal));
    /* Figure 40/V.150.1, leading out to the other SDL figures */
    switch (signal)
    {
    case V150_1_SIGNAL_SILENCE:
        if (s->local_media_state != V150_1_MEDIA_STATE_INITIAL_AUDIO
            ||
            s->remote_media_state != V150_1_MEDIA_STATE_INITIAL_AUDIO)
        {
            s->remote_media_state = V150_1_MEDIA_STATE_INDETERMINATE;
            s->local_media_state = V150_1_MEDIA_STATE_INITIAL_AUDIO;
            status_report(s, V150_1_STATUS_REASON_MEDIA_STATE_CHANGED);
            generic_macro(s, signal, 0);
        }
        /*endif*/
        break;
    case V150_1_SIGNAL_ABORT:
        s->remote_media_state = V150_1_MEDIA_STATE_INDETERMINATE;
        s->local_media_state = V150_1_MEDIA_STATE_INITIAL_AUDIO;
        status_report(s, V150_1_STATUS_REASON_MEDIA_STATE_CHANGED);
        generic_macro(s, signal, 0);
        break;
    case V150_1_SIGNAL_CALL_DISCRIMINATION_TIMER_EXPIRED:
        /* Time to give up with negotiation, and go with the flow */
        s->remote_media_state = V150_1_MEDIA_STATE_INDETERMINATE;
        s->local_media_state = V150_1_MEDIA_STATE_INITIAL_AUDIO;
        status_report(s, V150_1_STATUS_REASON_MEDIA_STATE_CHANGED);
        break;
    default:
        switch (s->local_media_state)
        {
        case V150_1_MEDIA_STATE_INDETERMINATE:
            switch (s->remote_media_state)
            {
            case V150_1_MEDIA_STATE_INDETERMINATE:
                break;
            case V150_1_MEDIA_STATE_INITIAL_AUDIO:
                break;
            case V150_1_MEDIA_STATE_VOICE_BAND_DATA:
                break;
            case V150_1_MEDIA_STATE_FAX_RELAY:
                break;
            case V150_1_MEDIA_STATE_MODEM_RELAY:
                break;
            case V150_1_MEDIA_STATE_TEXT_RELAY:
                break;
            }
            /*endswitch*/
            break;
        case V150_1_MEDIA_STATE_INITIAL_AUDIO:
            switch (s->remote_media_state)
            {
            case V150_1_MEDIA_STATE_INDETERMINATE:
                break;
            case V150_1_MEDIA_STATE_INITIAL_AUDIO:
                v150_1_figures_26_to_31(s, signal, msg, len);
                break;
            case V150_1_MEDIA_STATE_VOICE_BAND_DATA:
                v150_1_figure_33(s, signal, msg, len);
                break;
            case V150_1_MEDIA_STATE_FAX_RELAY:
                break;
            case V150_1_MEDIA_STATE_MODEM_RELAY:
                v150_1_figure_32(s, signal, msg, len);
                break;
            case V150_1_MEDIA_STATE_TEXT_RELAY:
                break;
            }
            /*endswitch*/
            break;
        case V150_1_MEDIA_STATE_VOICE_BAND_DATA:
            switch (s->remote_media_state)
            {
            case V150_1_MEDIA_STATE_INDETERMINATE:
                break;
            case V150_1_MEDIA_STATE_INITIAL_AUDIO:
                v150_1_figure_37(s, signal, msg, len);
                break;
            case V150_1_MEDIA_STATE_VOICE_BAND_DATA:
                v150_1_figure_39(s, signal, msg, len);
                break;
            case V150_1_MEDIA_STATE_FAX_RELAY:
                break;
            case V150_1_MEDIA_STATE_MODEM_RELAY:
                v150_1_figure_38(s, signal, msg, len);
                break;
            case V150_1_MEDIA_STATE_TEXT_RELAY:
                break;
            }
            /*endswitch*/
            break;
        case V150_1_MEDIA_STATE_FAX_RELAY:
            switch (s->remote_media_state)
            {
            case V150_1_MEDIA_STATE_INDETERMINATE:
                break;
            case V150_1_MEDIA_STATE_INITIAL_AUDIO:
                break;
            case V150_1_MEDIA_STATE_VOICE_BAND_DATA:
                break;
            case V150_1_MEDIA_STATE_FAX_RELAY:
                break;
            case V150_1_MEDIA_STATE_MODEM_RELAY:
                break;
            case V150_1_MEDIA_STATE_TEXT_RELAY:
                break;
            }
            /*endswitch*/
            break;
        case V150_1_MEDIA_STATE_MODEM_RELAY:
            switch (s->remote_media_state)
            {
            case V150_1_MEDIA_STATE_INDETERMINATE:
                break;
            case V150_1_MEDIA_STATE_INITIAL_AUDIO:
                v150_1_figure_34(s, signal, msg, len);
                break;
            case V150_1_MEDIA_STATE_VOICE_BAND_DATA:
                v150_1_figure_36(s, signal, msg, len);
                break;
            case V150_1_MEDIA_STATE_FAX_RELAY:
                break;
            case V150_1_MEDIA_STATE_MODEM_RELAY:
                v150_1_figure_35(s, signal, msg, len);
                break;
            case V150_1_MEDIA_STATE_TEXT_RELAY:
                break;
            }
            /*endswitch*/
            break;
        case V150_1_MEDIA_STATE_TEXT_RELAY:
            switch (s->remote_media_state)
            {
            case V150_1_MEDIA_STATE_INDETERMINATE:
                break;
            case V150_1_MEDIA_STATE_INITIAL_AUDIO:
                break;
            case V150_1_MEDIA_STATE_VOICE_BAND_DATA:
                break;
            case V150_1_MEDIA_STATE_FAX_RELAY:
                break;
            case V150_1_MEDIA_STATE_MODEM_RELAY:
                break;
            case V150_1_MEDIA_STATE_TEXT_RELAY:
                break;
            }
            /*endswitch*/
            break;
        }
        /*endswitch*/
    }
    /*endswitch*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_set_bits_per_character(v150_1_state_t *s, int bits)
{
    if (bits < 5  ||  bits > 8)
        return -1;
    /*endif*/
    bits -= 5;
    s->near.parms.data_format_code &= 0x9F; 
    s->near.parms.data_format_code |= ((bits << 5) & 0x60); 
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_set_parity(v150_1_state_t *s, int mode)
{
    s->near.parms.data_format_code &= 0xE3; 
    s->near.parms.data_format_code |= ((mode << 2) & 0x1C); 
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_set_stop_bits(v150_1_state_t *s, int bits)
{
    if (bits < 1  ||  bits > 2)
        return -1;
    /*endif*/
    bits -= 1;
    s->near.parms.data_format_code &= 0xFC; 
    s->near.parms.data_format_code |= (bits & 0x03);
    return 0;
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
            if (s->near.parms.i_raw_bit_available)
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
            if (s->near.parms.i_char_stat_available)
            {
                s->near.info_stream_msg_id = s->near.info_msg_preferences[i];
                return 0;
            }
            /*endif*/
            break;
        case V150_1_MSGID_I_CHAR_DYN:
            if (s->near.parms.i_char_dyn_available)
            {
                s->near.info_stream_msg_id = s->near.info_msg_preferences[i];
                return 0;
            }
            /*endif*/
            break;
        case V150_1_MSGID_I_FRAME:
            if (s->near.parms.i_frame_available)
            {
                s->near.info_stream_msg_id = s->near.info_msg_preferences[i];
                return 0;
            }
            /*endif*/
            break;
        case V150_1_MSGID_I_OCTET_CS:
            if (s->near.parms.i_octet_cs_available)
            {
                s->near.info_stream_msg_id = s->near.info_msg_preferences[i];
                return 0;
            }
            /*endif*/
            break;
        case V150_1_MSGID_I_CHAR_STAT_CS:
            if (s->near.parms.i_char_stat_cs_available)
            {
                s->near.info_stream_msg_id = s->near.info_msg_preferences[i];
                return 0;
            }
            /*endif*/
            break;
        case V150_1_MSGID_I_CHAR_DYN_CS:
            if (s->near.parms.i_char_dyn_cs_available)
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

static void log_init(v150_1_state_t *s, v150_1_near_far_t *parms)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "    Preferred non-error controlled Rx channel: %s\n", (parms->necrxch_option)  ?  "RSC"  :  "USC");
    span_log(&s->logging, SPAN_LOG_FLOW, "    Preferred error controlled Rx channel: %s\n", (parms->ecrxch_option)  ?  "USC"  :  "RSC");
    span_log(&s->logging, SPAN_LOG_FLOW, "    XID profile exchange  %ssupported\n", (parms->xid_profile_exchange_supported)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    Asymmetric data types %ssupported\n", (parms->asymmetric_data_types_supported)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_RAW-CHAR            supported\n");
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_RAW-BIT             %ssupported\n", (parms->i_raw_bit_supported)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_FRAME               %ssupported\n", (parms->i_frame_supported)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_OCTET %s     supported\n", (parms->dlci_supported)  ?  "(DLCI)   "  :  "(no DLCI)");
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_CHAR-STAT           %ssupported\n", (parms->i_char_stat_supported)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_CHAR-DYN            %ssupported\n", (parms->i_char_dyn_supported)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_OCTET-CS            %ssupported\n", (parms->i_octet_cs_supported)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_CHAR-STAT-CS        %ssupported\n", (parms->i_char_stat_cs_supported)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_CHAR-DYN-CS         %ssupported\n", (parms->i_char_dyn_cs_supported)  ?  ""  :  "not ");
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_tx_null(v150_1_state_t *s)
{
    int res;
    uint8_t pkt[256];

    res = -1;
    /* This isn't a real message. Its marked as reserved by the ITU-T in V.150.1 */
    pkt[0] = V150_1_MSGID_NULL;
    res = sprt_tx(&s->sprt, SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, pkt, 1);
    span_log(&s->logging, SPAN_LOG_FLOW, "NULL sent\n");
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
    if (s->near.parms.necrxch_option)
        i |= 0x80;
    /*endif*/
    if (s->near.parms.ecrxch_option)
        i |= 0x40;
    /*endif*/
    if (s->near.parms.xid_profile_exchange_supported)
        i |= 0x20;
    /*endif*/
    if (s->near.parms.asymmetric_data_types_supported)
        i |= 0x10;
    /*endif*/
    if (s->near.parms.i_raw_bit_supported)
        i |= 0x08;
    /*endif*/
    if (s->near.parms.i_frame_supported)
        i |= 0x04;
    /*endif*/
    if (s->near.parms.i_char_stat_supported)
        i |= 0x02;
    /*endif*/
    if (s->near.parms.i_char_dyn_supported)
        i |= 0x01;
    /*endif*/
    pkt[1] = i;
    i = 0;
    if (s->near.parms.i_octet_cs_supported)
        i |= 0x80;
    /*endif*/
    if (s->near.parms.i_char_stat_cs_supported)
        i |= 0x40;
    /*endif*/
    if (s->near.parms.i_char_dyn_cs_supported)
        i |= 0x20;
    /*endif*/
    pkt[2] = i;
    span_log(&s->logging, SPAN_LOG_FLOW, "Sending INIT\n");
    log_init(s, &s->near.parms);
    res = sprt_tx(&s->sprt, SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, pkt, 3);
    if (res >= 0)
    {
        s->near.parms.connection_state = V150_1_STATE_INITED;
        if (s->far.parms.connection_state >= V150_1_STATE_INITED)
        {
            select_info_msg_type(s);
            s->joint_connection_state = V150_1_STATE_INITED;
        }
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
    if (!s->far.parms.xid_profile_exchange_supported)
        return -1;
    /*endif*/
    pkt[0] = V150_1_MSGID_XID_XCHG;
    pkt[1] = s->near.parms.ecp;
    i = 0;
    if (s->near.parms.v42bis_supported)
        i |= 0x80;
    /*endif*/
    if (s->near.parms.v44_supported)
        i |= 0x40;
    /*endif*/
    if (s->near.parms.mnp5_supported)
        i |= 0x20;
    /*endif*/
    pkt[2] = i;
    if (s->near.parms.v42bis_supported)
    {
        pkt[3] = s->near.parms.v42bis_p0;
        put_net_unaligned_uint16(&pkt[4], s->near.parms.v42bis_p1);
        pkt[6] = s->near.parms.v42bis_p2;
    }
    else
    {
        memset(&pkt[3], 0, 4);
    }
    /*endif*/
    if (s->near.parms.v44_supported)
    {
        pkt[7] = s->near.parms.v44_c0;
        pkt[8] = s->near.parms.v44_p0;
        put_net_unaligned_uint16(&pkt[9], s->near.parms.v44_p1t);
        put_net_unaligned_uint16(&pkt[11], s->near.parms.v44_p1r);
        pkt[13] = s->near.parms.v44_p2t;
        pkt[14] = s->near.parms.v44_p2r;
        put_net_unaligned_uint16(&pkt[15], s->near.parms.v44_p3t);
        put_net_unaligned_uint16(&pkt[17], s->near.parms.v44_p3r);
    }
    else
    {
        memset(&pkt[7], 0, 12);
    }
    /*endif*/
    res = sprt_tx(&s->sprt, SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, pkt, 19);
    span_log(&s->logging, SPAN_LOG_FLOW, "XID xchg sent\n");
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_tx_jm_info(v150_1_state_t *s)
{
    int res;
    int i;
    int len;
    int bit;
    uint8_t pkt[256];

    for (i = 0;  i < 16;  i++)
    {
        if (s->near.parms.jm_category_id_seen[i])
        {
            span_log(&s->logging,
                     SPAN_LOG_FLOW,
                     "    JM %s 0x%x\n",
                     v150_1_jm_category_to_str(i),
                     s->near.parms.jm_category_info[i]);
        }
        /*endif*/
    }
    /*endfor*/
    if (s->near.parms.jm_category_id_seen[V150_1_JM_CATEGORY_ID_MODULATION_MODES])
    {
        for (i = 0;  i < 16;  i++)
        {
            bit = s->near.parms.jm_category_info[V150_1_JM_CATEGORY_ID_MODULATION_MODES] & (0x8000 >> i);
            if (bit)
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "    JM     %s\n", v150_1_jm_info_modulation_to_str(bit));
            }
            /*endif*/
        }
        /*endfor*/
    }
    /*endif*/
    res = -1;
    pkt[0] = V150_1_MSGID_JM_INFO;
    len = 1;
    for (i = 0;  i < 16;  i++)
    {
        if (s->near.parms.jm_category_id_seen[i])
        {
            put_net_unaligned_uint16(&pkt[len], (i << 12) | (s->near.parms.jm_category_info[i] & 0x0FFF));
            len += 2;
        }
        /*endif*/
    }
    res = sprt_tx(&s->sprt, SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, pkt, len);
    span_log(&s->logging, SPAN_LOG_FLOW, "JM info sent\n");
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_tx_start_jm(v150_1_state_t *s)
{
    int res;
    uint8_t pkt[256];

    res = -1;
    if (s->near.parms.connection_state != V150_1_STATE_IDLE)
    {
        pkt[0] = V150_1_MSGID_START_JM;
        res = sprt_tx(&s->sprt, SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, pkt, 1);
        span_log(&s->logging, SPAN_LOG_FLOW, "Start JM sent\n");
    }
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
    pkt[1] = (s->near.parms.selmod << 2) | s->near.parms.selected_compression_direction;
    pkt[2] = (s->near.parms.selected_compression << 4) | s->near.parms.selected_error_correction;
    put_net_unaligned_uint16(&pkt[3], s->near.parms.tdsr);
    put_net_unaligned_uint16(&pkt[5], s->near.parms.rdsr);

    available_data_types = 0;
    if (s->near.parms.i_octet_with_dlci_available)
        available_data_types |= 0x8000;
    /*endif*/
    if (s->near.parms.i_octet_without_dlci_available)
        available_data_types |= 0x4000;
    /*endif*/
    if (s->near.parms.i_raw_bit_available)
        available_data_types |= 0x2000;
    /*endif*/
    if (s->near.parms.i_frame_available)
        available_data_types |= 0x1000;
    /*endif*/
    if (s->near.parms.i_char_stat_available)
        available_data_types |= 0x0800;
    /*endif*/
    if (s->near.parms.i_char_dyn_available)
        available_data_types |= 0x0400;
    /*endif*/
    if (s->near.parms.i_octet_cs_available)
        available_data_types |= 0x0200;
    /*endif*/
    if (s->near.parms.i_char_stat_cs_available)
        available_data_types |= 0x0100;
    /*endif*/
    if (s->near.parms.i_char_dyn_cs_available)
        available_data_types |= 0x0080;
    /*endif*/
    put_net_unaligned_uint16(&pkt[7], available_data_types);
    len = 9;
    if (s->near.parms.selected_compression == V150_1_COMPRESSION_V42BIS  ||  s->near.parms.selected_compression == V150_1_COMPRESSION_V44)
    {
        /* This is only included if V.42bis or V.44 is selected. For no compression, or MNP5 this is omitted */
        put_net_unaligned_uint16(&pkt[9], s->near.parms.compression_tx_dictionary_size);
        put_net_unaligned_uint16(&pkt[11], s->near.parms.compression_rx_dictionary_size);
        pkt[13] = s->near.parms.compression_tx_string_length;
        pkt[14] = s->near.parms.compression_rx_string_length;
        len += 6;
    }
    /*endif*/
    if (s->near.parms.selected_compression == V150_1_COMPRESSION_V44)
    {
        /* This is only included if V.44 is selected. For no compression, MNP5, or V.42bis this is omitted */
        put_net_unaligned_uint16(&pkt[15], s->near.parms.compression_tx_history_size);
        put_net_unaligned_uint16(&pkt[15], s->near.parms.compression_rx_history_size);
        len += 4;
    }
    /*endif*/
    res = sprt_tx(&s->sprt, SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, pkt, len);
    if (res >= 0)
    {
        s->near.parms.connection_state = V150_1_STATE_CONNECTED;
        if (s->near.parms.connection_state >= V150_1_STATE_CONNECTED)
            s->joint_connection_state = V150_1_STATE_CONNECTED;
        /*endif*/
        span_log(&s->logging, SPAN_LOG_FLOW, "Connect sent\n");
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
    if (s->near.parms.connection_state != V150_1_STATE_IDLE)
    {
        pkt[0] = V150_1_MSGID_BREAK;
        pkt[1] = (source << 4) | type;
        pkt[2] = duration/10;
        res = sprt_tx(&s->sprt, SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, pkt, 3);
        if (res >= 0)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Break sent\n");
        }
        /*endif*/
    }
    /*endif*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_tx_break_ack(v150_1_state_t *s)
{
    int res;
    uint8_t pkt[256];

    res = -1;
    if (s->near.parms.connection_state != V150_1_STATE_IDLE)
    {
        pkt[0] = V150_1_MSGID_BREAKACK;
        res = sprt_tx(&s->sprt, SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, pkt, 1);
        if (res >= 0)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Break ACK sent\n");
        }
        /*endif*/
    }
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
        s->near.parms.connection_state = V150_1_STATE_RETRAIN;
        s->joint_connection_state = V150_1_STATE_RETRAIN;
        break;
    case V150_1_MR_EVENT_ID_RATE_RENEGOTIATION:
        pkt[2] = V150_1_MR_EVENT_REASON_NULL;
        len = 3;
        s->near.parms.connection_state = V150_1_STATE_RATE_RENEGOTIATION;
        s->joint_connection_state = V150_1_STATE_RATE_RENEGOTIATION;
        break;
    case V150_1_MR_EVENT_ID_PHYSUP:
        pkt[2] = 0;
        i = (s->near.parms.selmod << 2);
        if (s->near.parms.txsen)
            i |= 0x02;
        /*endif*/
        if (s->near.parms.rxsen)
            i |= 0x01;
        /*endif*/
        pkt[3] = i;
        put_net_unaligned_uint16(&pkt[4], s->near.parms.tdsr);
        put_net_unaligned_uint16(&pkt[4], s->near.parms.rdsr);
        pkt[8] = (s->near.parms.txsen)  ?  s->near.parms.txsr  :  V150_1_SYMBOL_RATE_NULL;
        pkt[9] = (s->near.parms.rxsen)  ?  s->near.parms.rxsr  :  V150_1_SYMBOL_RATE_NULL;
        len = 10;
        s->near.parms.connection_state = V150_1_STATE_PHYSUP;
        if (s->far.parms.connection_state >= V150_1_STATE_PHYSUP)
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
    res = sprt_tx(&s->sprt, SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, pkt, len);
    if (res >= 0)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "MR-event %s (%d) sent\n", v150_1_mr_event_type_to_str(event_id), event_id);
    }
    /*endif*/
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_tx_cleardown(v150_1_state_t *s, int reason)
{
    int res;
    uint8_t pkt[256];

    res = -1;

    if (s->near.parms.connection_state != V150_1_STATE_IDLE)
    {
        pkt[0] = V150_1_MSGID_CLEARDOWN;
        pkt[1] = reason;
        pkt[2] = 0; /* Vendor tag */
        pkt[3] = 0; /* Vendor info */
        res = sprt_tx(&s->sprt, SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, pkt, 4);
        if (res >= 0)
        {
            s->near.parms.connection_state = V150_1_STATE_IDLE;
            span_log(&s->logging, SPAN_LOG_FLOW, "Cleardown sent\n");
        }
        /*endif*/
    }
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
    if (s->near.parms.v42_lapm_supported)
        i |= 0x40;
    /*endif*/
    if (s->near.parms.v42_annex_a_supported)
        i |= 0x10;
    /*endif*/
    if (s->near.parms.v44_supported)
        i |= 0x04;
    /*endif*/
    if (s->near.parms.v42bis_supported)
        i |= 0x01;
    /*endif*/
    pkt[1] = i;
    i = 0;
    if (s->near.parms.mnp5_supported)
        i |= 0x40;
    /*endif*/
    pkt[2] = i;
    if (s->near.parms.v42bis_supported)
    {
        pkt[3] = s->near.parms.v42bis_p0;
        put_net_unaligned_uint16(&pkt[4], s->near.parms.v42bis_p1);
        pkt[6] = s->near.parms.v42bis_p2;
    }
    else
    {
        memset(&pkt[3], 0, 4);
    }
    /*endif*/
    if (s->near.parms.v44_supported)
    {
        pkt[7] = s->near.parms.v44_c0;
        pkt[8] = s->near.parms.v44_p0;
        put_net_unaligned_uint16(&pkt[9], s->near.parms.v44_p1t);
        put_net_unaligned_uint16(&pkt[11], s->near.parms.v44_p1r);
        pkt[13] = s->near.parms.v44_p2t;
        pkt[14] = s->near.parms.v44_p2r;
        put_net_unaligned_uint16(&pkt[15], s->near.parms.v44_p3t);
        put_net_unaligned_uint16(&pkt[17], s->near.parms.v44_p3r);
    }
    else
    {
        memset(&pkt[7], 0, 12);
    }
    /*endif*/
    res = sprt_tx(&s->sprt, SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED, pkt, 19);
    span_log(&s->logging, SPAN_LOG_FLOW, "Prof xchg sent\n");
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
    if (!s->far.parms.i_raw_bit_available)
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

    if (!s->far.parms.i_octet_without_dlci_available  &&  !s->far.parms.i_octet_with_dlci_available)
        return -1;
    /*endif*/
    if (len > max_len - 3)
        return -1;
    /*endif*/
    pkt[0] = V150_1_MSGID_I_OCTET;
    if (s->far.parms.i_octet_with_dlci_available)
    {
        /* The DLCI may be one or two octets long. */
        if ((s->near.parms.dlci & 0x01) == 0)
        {
            pkt[1] = s->near.parms.dlci & 0xFF;
            header = 2;
        }
        else
        {
            put_net_unaligned_uint16(&pkt[1], s->near.parms.dlci);
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
    if (!s->far.parms.i_char_stat_available)
        return -1;
    /*endif*/
    if (len > max_len - 2)
        return -1;
    /*endif*/
    pkt[0] = V150_1_MSGID_I_CHAR_STAT;
    pkt[1] = s->near.parms.data_format_code;
    memcpy(&pkt[2], buf, len);
    len += 2;
    return len;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_build_i_char_dyn(v150_1_state_t *s, uint8_t pkt[], int max_len, const uint8_t buf[], int len)
{
    if (!s->far.parms.i_char_dyn_available)
        return -1;
    /*endif*/
    if (len > max_len - 2)
        return -1;
    /*endif*/
    pkt[0] = V150_1_MSGID_I_CHAR_DYN;
    pkt[1] = s->near.parms.data_format_code;
    memcpy(&pkt[2], buf, len);
    len += 2;
    return len;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_build_i_frame(v150_1_state_t *s, uint8_t pkt[], int max_len, const uint8_t buf[], int len)
{
    int data_frame_state;

    data_frame_state = 0;

    if (!s->far.parms.i_frame_available)
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
    if (!s->far.parms.i_octet_cs_available)
        return -1;
    /*endif*/
    if (len > max_len - 3)
        return -1;
    /*endif*/
    pkt[0] = V150_1_MSGID_I_OCTET_CS;
    put_net_unaligned_uint16(&pkt[1], s->near.parms.octet_cs_next_seq_no & 0xFFFF);
    memcpy(&pkt[3], buf, len);
    s->near.parms.octet_cs_next_seq_no += len;
    len += 3;
    return len;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_build_i_char_stat_cs(v150_1_state_t *s, uint8_t pkt[], int max_len, const uint8_t buf[], int len)
{
    if (!s->far.parms.i_char_stat_cs_available)
        return -1;
    /*endif*/
    if (len > max_len - 4)
        return -1;
    /*endif*/
    pkt[0] = V150_1_MSGID_I_CHAR_STAT_CS;
    pkt[1] = s->near.parms.data_format_code;
    put_net_unaligned_uint16(&pkt[2], s->near.parms.octet_cs_next_seq_no & 0xFFFF);
    memcpy(&pkt[4], buf, len);
    len += 4;
    s->near.parms.octet_cs_next_seq_no += len;
    return len;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_build_i_char_dyn_cs(v150_1_state_t *s, uint8_t pkt[], int max_len, const uint8_t buf[], int len)
{
    if (!s->far.parms.i_char_dyn_cs_available)
        return -1;
    /*endif*/
    if (len > max_len - 4)
        return -1;
    /*endif*/
    pkt[0] = V150_1_MSGID_I_CHAR_DYN_CS;
    pkt[1] = s->near.parms.data_format_code;
    put_net_unaligned_uint16(&pkt[2], s->near.parms.octet_cs_next_seq_no & 0xFFFF);
    memcpy(&pkt[4], buf, len);
    s->near.parms.octet_cs_next_seq_no += len;
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
        res = sprt_tx(&s->sprt, s->near.info_stream_channel, pkt, res);
    }
    else
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Bad message\n");
    }
    /*endif*/
    return res;
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
    s->far.parms.necrxch_option = (buf[1] & 0x80) != 0;
    s->far.parms.ecrxch_option = (buf[1] & 0x40) != 0;
    s->far.parms.xid_profile_exchange_supported = (buf[1] & 0x20) != 0;
    s->far.parms.asymmetric_data_types_supported = (buf[1] & 0x10) != 0;
    s->far.parms.i_raw_bit_supported = (buf[1] & 0x08) != 0;
    s->far.parms.i_frame_supported = (buf[1] & 0x04) != 0;
    s->far.parms.i_char_stat_supported = (buf[1] & 0x02) != 0;
    s->far.parms.i_char_dyn_supported = (buf[1] & 0x01) != 0;
    s->far.parms.i_octet_cs_supported = (buf[2] & 0x80) != 0;
    s->far.parms.i_char_stat_cs_supported = (buf[2] & 0x40) != 0;
    s->far.parms.i_char_dyn_cs_supported = (buf[2] & 0x20) != 0;

    /* Now sift out what will be available, because both ends support the features */
    s->near.parms.i_raw_bit_available  = s->near.parms.i_raw_bit_supported  &&  s->far.parms.i_raw_bit_supported;
    s->near.parms.i_frame_available = s->near.parms.i_frame_supported  &&  s->far.parms.i_frame_supported;
    s->near.parms.i_octet_with_dlci_available = s->near.parms.dlci_supported;
    s->near.parms.i_octet_without_dlci_available = !s->near.parms.dlci_supported;
    s->near.parms.i_char_stat_available = s->near.parms.i_char_stat_supported  &&  s->far.parms.i_char_stat_supported;
    s->near.parms.i_char_dyn_available = s->near.parms.i_char_dyn_supported  &&  s->far.parms.i_char_dyn_supported;
    s->near.parms.i_octet_cs_available = s->near.parms.i_octet_cs_supported  &&  s->far.parms.i_octet_cs_supported;
    s->near.parms.i_char_stat_cs_available = s->near.parms.i_char_stat_cs_supported  &&  s->far.parms.i_char_stat_cs_supported;
    s->near.parms.i_char_dyn_cs_available = s->near.parms.i_char_dyn_cs_supported  &&  s->far.parms.i_char_dyn_cs_supported;

    if (s->far.parms.connection_state >= V150_1_STATE_INITED)
        select_info_msg_type(s);
    /*endif*/
    span_log(&s->logging, SPAN_LOG_FLOW, "Received INIT\n");
    log_init(s, &s->far.parms);

    s->far.parms.connection_state = V150_1_STATE_INITED;
    if (s->near.parms.connection_state >= V150_1_STATE_INITED)
        s->joint_connection_state = V150_1_STATE_INITED;
    /*endif*/
    status_report(s, V150_1_STATUS_REASON_CONNECTION_STATE_CHANGED);
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
    s->far.parms.ecp = buf[1];

    s->far.parms.v42bis_supported = (buf[2] & 0x80) != 0;
    s->far.parms.v44_supported = (buf[2] & 0x40) != 0;
    s->far.parms.mnp5_supported = (buf[2] & 0x20) != 0;

    s->far.parms.v42bis_p0 = buf[3];
    s->far.parms.v42bis_p1 = get_net_unaligned_uint16(&buf[4]);
    s->far.parms.v42bis_p2 = buf[6];
    s->far.parms.v44_c0 = buf[7];
    s->far.parms.v44_p0 = buf[8];
    s->far.parms.v44_p1t = get_net_unaligned_uint16(&buf[9]);
    s->far.parms.v44_p1r = get_net_unaligned_uint16(&buf[11]);
    s->far.parms.v44_p2t = buf[13];
    s->far.parms.v44_p2r = buf[14];
    s->far.parms.v44_p3t = get_net_unaligned_uint16(&buf[15]);
    s->far.parms.v44_p3r = get_net_unaligned_uint16(&buf[17]);

    span_log(&s->logging, SPAN_LOG_FLOW, "    V.42bis %ssupported\n", (s->far.parms.v42bis_supported)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44    %ssupported\n", (s->far.parms.v44_supported)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    MNP5    %ssupported\n", (s->far.parms.mnp5_supported)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.42bis P0 %d\n", s->far.parms.v42bis_p0);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.42bis P1 %d\n", s->far.parms.v42bis_p1);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.42bis P2 %d\n", s->far.parms.v42bis_p2);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 C0 %d\n", s->far.parms.v44_c0);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 P1 %d\n", s->far.parms.v44_p0);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 P1T %d\n", s->far.parms.v44_p1t);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 P1R %d\n", s->far.parms.v44_p1r);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 P2T %d\n", s->far.parms.v44_p2t);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 P2R %d\n", s->far.parms.v44_p2r);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 P3T %d\n", s->far.parms.v44_p3t);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 P3R %d\n", s->far.parms.v44_p3r);

    /* TODO: */
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_process_jm_info(v150_1_state_t *s, const uint8_t buf[], int len)
{
    int i;
    int id;
    int bit;

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
        s->far.parms.jm_category_id_seen[id] = true;
        s->far.parms.jm_category_info[id] = get_net_unaligned_uint16(&buf[i]) & 0x0FFF;
    }
    /*endfor*/
    for (i = 0;  i < 16;  i++)
    {
        if (s->far.parms.jm_category_id_seen[i])
        {
            span_log(&s->logging,
                     SPAN_LOG_WARNING,
                     "    JM %s 0x%x\n",
                     v150_1_jm_category_to_str(i),
                     s->far.parms.jm_category_info[i]);
        }
        /*endif*/
    }
    /*endfor*/
    if (s->far.parms.jm_category_id_seen[V150_1_JM_CATEGORY_ID_MODULATION_MODES])
    {
        for (i = 0;  i < 16;  i++)
        {
            bit = s->far.parms.jm_category_info[V150_1_JM_CATEGORY_ID_MODULATION_MODES] & (0x8000 >> i);
            if (bit)
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "    JM     %s\n", v150_1_jm_info_modulation_to_str(bit));
            }
            /*endif*/
        }
        /*endfor*/
    }
    /*endif*/

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
    s->far.parms.selmod = (buf[1] >> 2) & 0x3F;
    s->far.parms.selected_compression_direction = buf[1] & 0x03;
    s->far.parms.selected_compression = (buf[2] >> 4) & 0x0F;
    s->far.parms.selected_error_correction = buf[2] & 0x0F;
    s->far.parms.tdsr = get_net_unaligned_uint16(&buf[3]);
    s->far.parms.rdsr = get_net_unaligned_uint16(&buf[5]);

    available_data_types = get_net_unaligned_uint16(&buf[7]);
    s->far.parms.i_octet_with_dlci_available = (available_data_types & 0x8000) != 0;
    s->far.parms.i_octet_without_dlci_available = (available_data_types & 0x4000) != 0;
    s->far.parms.i_raw_bit_available = (available_data_types & 0x2000) != 0;
    s->far.parms.i_frame_available = (available_data_types & 0x1000) != 0;
    s->far.parms.i_char_stat_available = (available_data_types & 0x0800) != 0;
    s->far.parms.i_char_dyn_available = (available_data_types & 0x0400) != 0;
    s->far.parms.i_octet_cs_available = (available_data_types & 0x0200) != 0;
    s->far.parms.i_char_stat_cs_available = (available_data_types & 0x0100) != 0;
    s->far.parms.i_char_dyn_cs_available = (available_data_types & 0x0080) != 0;

    span_log(&s->logging, SPAN_LOG_FLOW, "    Modulation %s\n", v150_1_modulation_to_str(s->far.parms.selmod));
    span_log(&s->logging, SPAN_LOG_FLOW, "    Compression direction %s\n", v150_1_compression_direction_to_str(s->far.parms.selected_compression_direction));
    span_log(&s->logging, SPAN_LOG_FLOW, "    Compression %s\n", v150_1_compression_to_str(s->far.parms.selected_compression));
    span_log(&s->logging, SPAN_LOG_FLOW, "    Error correction %s\n", v150_1_error_correction_to_str(s->far.parms.selected_error_correction));
    span_log(&s->logging, SPAN_LOG_FLOW, "    Tx data rate %d\n", s->far.parms.tdsr);
    span_log(&s->logging, SPAN_LOG_FLOW, "    Rx data rate %d\n", s->far.parms.rdsr);

    span_log(&s->logging, SPAN_LOG_FLOW, "    I_RAW-CHAR            available\n");
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_RAW-BIT             %savailable\n", (s->far.parms.i_raw_bit_available)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_FRAME               %savailable\n", (s->far.parms.i_frame_available)  ?  ""  :  "not ");
    if (s->far.parms.i_octet_without_dlci_available  ||  s->far.parms.i_octet_without_dlci_available)
    {
        if (s->far.parms.i_octet_without_dlci_available)
            span_log(&s->logging, SPAN_LOG_FLOW, "    I_OCTET (no DLCI)     available\n");
        /*endif*/
        if (s->far.parms.i_octet_with_dlci_available)
            span_log(&s->logging, SPAN_LOG_FLOW, "    I_OCTET (DLCI)        available\n");
        /*endif*/
    }
    else
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "    I_OCTET               not available\n");
    }
    /*endif*/
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_CHAR-STAT           %savailable\n", (s->far.parms.i_char_stat_available)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_CHAR-DYN            %savailable\n", (s->far.parms.i_char_dyn_available)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_OCTET-CS            %savailable\n", (s->far.parms.i_octet_cs_available)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_CHAR-STAT-CS        %savailable\n", (s->far.parms.i_char_stat_cs_available)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    I_CHAR-DYN-CS         %savailable\n", (s->far.parms.i_char_dyn_cs_available)  ?  ""  :  "not ");

    if (len >= 15
        &&
        (s->far.parms.selected_compression == V150_1_COMPRESSION_V42BIS  ||  s->far.parms.selected_compression == V150_1_COMPRESSION_V44))
    {
        /* Selected_compression should be V150_1_COMPRESSION_V42BIS or V150_1_COMPRESSION_V44 */
        s->far.parms.compression_tx_dictionary_size = get_net_unaligned_uint16(&buf[9]);
        s->far.parms.compression_rx_dictionary_size = get_net_unaligned_uint16(&buf[11]);
        s->far.parms.compression_tx_string_length = buf[13];
        s->far.parms.compression_rx_string_length = buf[14];

        span_log(&s->logging, SPAN_LOG_FLOW, "    Tx dictionary size %d\n", s->far.parms.compression_tx_dictionary_size);
        span_log(&s->logging, SPAN_LOG_FLOW, "    Rx dictionary size %d\n", s->far.parms.compression_rx_dictionary_size);
        span_log(&s->logging, SPAN_LOG_FLOW, "    Tx string length %d\n", s->far.parms.compression_tx_string_length);
        span_log(&s->logging, SPAN_LOG_FLOW, "    Rx string length %d\n", s->far.parms.compression_rx_string_length);
    }
    else
    {
        s->far.parms.compression_tx_dictionary_size = 0;
        s->far.parms.compression_rx_dictionary_size = 0;
        s->far.parms.compression_tx_string_length = 0;
        s->far.parms.compression_rx_string_length = 0;
    }
    /*endif*/

    if (len >= 19
        &&
        s->far.parms.selected_compression == V150_1_COMPRESSION_V44)
    {
        /* Selected_compression should be V150_1_COMPRESSION_V44 */
        s->far.parms.compression_tx_history_size = get_net_unaligned_uint16(&buf[15]);
        s->far.parms.compression_rx_history_size = get_net_unaligned_uint16(&buf[17]);

        span_log(&s->logging, SPAN_LOG_FLOW, "   Tx history size %d\n", s->far.parms.compression_tx_history_size);
        span_log(&s->logging, SPAN_LOG_FLOW, "   Rx history size %d\n", s->far.parms.compression_rx_history_size);
    }
    else
    {
        s->far.parms.compression_tx_history_size = 0;
        s->far.parms.compression_rx_history_size = 0;
    }
    /*endif*/

    s->far.parms.connection_state = V150_1_STATE_CONNECTED;
    if (s->near.parms.connection_state >= V150_1_STATE_CONNECTED)
        s->joint_connection_state = V150_1_STATE_CONNECTED;
    /*endif*/
    status_report(s, V150_1_STATUS_REASON_CONNECTION_STATE_CONNECTED);
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
    span_log(&s->logging, SPAN_LOG_FLOW, "MR-event %s (%d) received\n", v150_1_mr_event_type_to_str(event), event);
    switch (event)
    {
    case V150_1_MR_EVENT_ID_NULL:
        if (len != 3)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "Invalid MR-event message length %d\n", len);
            return -1;
        }
        /*endif*/
        break;
    case V150_1_MR_EVENT_ID_RATE_RENEGOTIATION:
    case V150_1_MR_EVENT_ID_RETRAIN:
        if (len != 3)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "Invalid MR-event message length %d\n", len);
            return -1;
        }
        /*endif*/
        reason = buf[2];
        span_log(&s->logging, SPAN_LOG_FLOW, "    Reason %d\n", reason);
        if (event == V150_1_MR_EVENT_ID_RETRAIN)
        {
            s->far.parms.connection_state = V150_1_STATE_RETRAIN;
            s->joint_connection_state = V150_1_STATE_RETRAIN;
            status_report(s, V150_1_STATUS_REASON_RATE_RETRAIN_RECEIVED);
        }
        else
        {
            s->far.parms.connection_state = V150_1_STATE_RATE_RENEGOTIATION;
            s->joint_connection_state = V150_1_STATE_RATE_RENEGOTIATION;
            status_report(s, V150_1_STATUS_REASON_RATE_RENEGOTIATION_RECEIVED);
        }
        /*endif*/
        break;
    case V150_1_MR_EVENT_ID_PHYSUP:
        if (len != 10)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "Invalid MR-event message length %d\n", len);
            return -1;
        }
        /*endif*/
        s->far.parms.selmod = (buf[3] >> 2) & 0x3F;
        s->far.parms.txsen = (buf[3] & 0x02) != 0;
        s->far.parms.rxsen = (buf[3] & 0x01) != 0;
        s->far.parms.tdsr = get_net_unaligned_uint16(&buf[4]);
        s->far.parms.rdsr = get_net_unaligned_uint16(&buf[6]);
        s->far.parms.txsr = buf[8];
        s->far.parms.rxsr = buf[9];

        span_log(&s->logging, SPAN_LOG_FLOW, "    Selected modulation %s\n", v150_1_modulation_to_str(s->far.parms.selmod));
        span_log(&s->logging, SPAN_LOG_FLOW, "    Tx data signalling rate %d\n", s->far.parms.tdsr);
        span_log(&s->logging, SPAN_LOG_FLOW, "    Rx data signalling rate %d\n", s->far.parms.rdsr);
        if (s->far.parms.txsen)
            span_log(&s->logging, SPAN_LOG_FLOW, "    Tx symbol rate %s\n", v150_1_symbol_rate_to_str(s->far.parms.txsr));
        /*endif*/
        if (s->far.parms.rxsen)
            span_log(&s->logging, SPAN_LOG_FLOW, "    Rx symbol rate %s\n", v150_1_symbol_rate_to_str(s->far.parms.rxsr));
        /*endif*/
        
        /* TODO: report these parameters */
        
        s->far.parms.connection_state = V150_1_STATE_PHYSUP;
        if (s->near.parms.connection_state >= V150_1_STATE_PHYSUP)
            s->joint_connection_state = V150_1_STATE_PHYSUP;
        /*endif*/
        status_report(s, V150_1_STATUS_REASON_CONNECTION_STATE_PHYSUP);
        break;
    default:
        span_log(&s->logging, SPAN_LOG_WARNING, "Unknown MR-event type %d received\n", event);
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

    s->far.parms.cleardown_reason = buf[1];
    span_log(&s->logging,
             SPAN_LOG_FLOW,
             "    Reason %s\n",
             v150_1_cleardown_reason_to_str(s->far.parms.cleardown_reason));
    // vendor = buf[2];
    // vendor_info = buf[3];
    /* A cleardown moves everything back to square one. */
    s->far.parms.connection_state = V150_1_STATE_IDLE;
    status_report(s, V150_1_STATUS_REASON_CONNECTION_STATE_CHANGED);
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
    s->far.parms.v42_lapm_supported = (buf[1] & 0xC0) == 0x40;
    s->far.parms.v42_annex_a_supported = (buf[1] & 0x30) == 0x10;
    s->far.parms.v44_supported = (buf[1] & 0x0C) == 0x04;
    s->far.parms.v42bis_supported = (buf[1] & 0x03) == 0x01;
    s->far.parms.mnp5_supported = (buf[2] & 0xC0) == 0x40;

    s->far.parms.v42bis_p0 = buf[3];
    s->far.parms.v42bis_p1 = get_net_unaligned_uint16(&buf[4]);
    s->far.parms.v42bis_p2 = buf[6];
    s->far.parms.v44_c0 = buf[7];
    s->far.parms.v44_p0 = buf[8];
    s->far.parms.v44_p1t = get_net_unaligned_uint16(&buf[9]);
    s->far.parms.v44_p1r = get_net_unaligned_uint16(&buf[11]);
    s->far.parms.v44_p2t = buf[13];
    s->far.parms.v44_p2r = buf[14];
    s->far.parms.v44_p3t = get_net_unaligned_uint16(&buf[15]);
    s->far.parms.v44_p3r = get_net_unaligned_uint16(&buf[17]);

    span_log(&s->logging, SPAN_LOG_FLOW, "    V.42 LAPM    %ssupported\n", (s->far.parms.v42_lapm_supported)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.42 Annex A %ssupported\n", (s->far.parms.v42_annex_a_supported)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44         %ssupported\n", (s->far.parms.v44_supported)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.42bis      %ssupported\n", (s->far.parms.v42bis_supported)  ?  ""  :  "not ");
    span_log(&s->logging, SPAN_LOG_FLOW, "    MNP5         %ssupported\n", (s->far.parms.mnp5_supported)  ?  ""  :  "not ");

    span_log(&s->logging, SPAN_LOG_FLOW, "    V.42bis P0 %d\n", s->far.parms.v42bis_p0);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.42bis P1 %d\n", s->far.parms.v42bis_p1);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.42bis P2 %d\n", s->far.parms.v42bis_p2);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 C0 %d\n", s->far.parms.v44_c0);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 P1 %d\n", s->far.parms.v44_p0);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 P1T %d\n", s->far.parms.v44_p1t);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 P1R %d\n", s->far.parms.v44_p1r);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 P2T %d\n", s->far.parms.v44_p2t);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 P2R %d\n", s->far.parms.v44_p2r);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 P3T %d\n", s->far.parms.v44_p3t);
    span_log(&s->logging, SPAN_LOG_FLOW, "    V.44 P3R %d\n", s->far.parms.v44_p3r);

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
        if (s->rx_data_handler)
            s->rx_data_handler(s->rx_data_handler_user_data, &buf[header], len - header, -1);
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
        if (s->rx_data_handler)
            s->rx_data_handler(s->rx_data_handler_user_data, &buf[header], len - header, -1);
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
    if (s->far.parms.i_octet_with_dlci_available)
    {
        /* DLCI is one or two bytes (usually just 1). The low bit of each byte is an extension
           bit, allowing for a variable number of bytes. */
        if (len < 2)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "I_OCTET with DLCI has no DLCI field\n");
            header = 1000;
        }
        else
        {
            if ((buf[1] & 0x01) == 0)
            {
                if ((buf[2] & 0x01) == 0)
                    span_log(&s->logging, SPAN_LOG_WARNING, "I_OCTET with DLCI has bad DLCI field\n"); 
                /*endif*/
                header = 3;
                s->far.parms.dlci = get_net_unaligned_uint16(&buf[1]);
            }
            else
            {
                header = 2;
                s->far.parms.dlci = buf[1];
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
        if (s->rx_data_handler)
            s->rx_data_handler(s->rx_data_handler_user_data, &buf[header], len - header, -1);
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
    if (s->far.parms.data_format_code != buf[1])
    {
        /* Every packet in a session should have the same data format code */
        s->far.parms.data_format_code = buf[1];
        status_report(s, V150_1_STATUS_REASON_DATA_FORMAT_CHANGED);
    }
    /*endif*/
    if (len > 2)
    {
        if (s->rx_data_handler)
            s->rx_data_handler(s->rx_data_handler_user_data, &buf[2], len - 2, -1);
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
    if (s->far.parms.data_format_code != buf[1])
    {
        s->far.parms.data_format_code = buf[1];
        status_report(s, V150_1_STATUS_REASON_DATA_FORMAT_CHANGED);
    }
    /*endif*/
    if (len > 2)
    {
        if (s->rx_data_handler)
            s->rx_data_handler(s->rx_data_handler_user_data, &buf[2], len - 2, -1);
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
        if (s->rx_data_handler)
            s->rx_data_handler(s->rx_data_handler_user_data, &buf[2], len - 2, -1);
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
    fill = (character_seq_no - s->far.parms.octet_cs_next_seq_no) & 0xFFFF;
    if (s->rx_data_handler)
        s->rx_data_handler(s->rx_data_handler_user_data, &buf[3], len - 3, fill);
    /*endif*/
    s->far.parms.octet_cs_next_seq_no = (character_seq_no + len - 3) & 0xFFFF;
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
    if (s->far.parms.data_format_code != buf[1])
    {
        /* Every packet in a session should have the same data format code */
        s->far.parms.data_format_code = buf[1];
        status_report(s, V150_1_STATUS_REASON_DATA_FORMAT_CHANGED);
    }
    /*endif*/
    character_seq_no = get_net_unaligned_uint16(&buf[2]);
    /* Check for a gap in the data */
    fill = (character_seq_no - s->far.parms.octet_cs_next_seq_no) & 0xFFFF;
    if (s->rx_data_handler)
        s->rx_data_handler(s->rx_data_handler_user_data, &buf[4], len - 4, fill);
    /*endif*/
    s->far.parms.octet_cs_next_seq_no = (character_seq_no + len - 4) & 0xFFFF;
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
    if (s->far.parms.data_format_code != buf[1])
    {
        s->far.parms.data_format_code = buf[1];
        status_report(s, V150_1_STATUS_REASON_DATA_FORMAT_CHANGED);
    }
    /*endif*/
    character_seq_no = get_net_unaligned_uint16(&buf[2]);
    /* Check for a gap in the data */
    fill = (character_seq_no - s->far.parms.octet_cs_next_seq_no) & 0xFFFF;
    if (s->rx_data_handler)
        s->rx_data_handler(s->rx_data_handler_user_data, &buf[4], len - 4, fill);
    /*endif*/
    s->far.parms.octet_cs_next_seq_no = (character_seq_no + len - 4) & 0xFFFF;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int process_rx_sprt_msg(void *user_data, int chan, int seq_no, const uint8_t buf[], int len)
{
    int res;
    int msg_id;
    v150_1_state_t *s;

    s  = (v150_1_state_t *) user_data;

    span_log(&s->logging, SPAN_LOG_FLOW, "%s (%d) seq %d\n", sprt_transmission_channel_to_str(chan), chan, seq_no);
    span_log_buf(&s->logging, SPAN_LOG_FLOW, "", buf, len);

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
    /*endif*/
    msg_id = buf[0] & 0x7F;
    span_log(&s->logging, SPAN_LOG_FLOW, "Message %s received on channel %d, seq no %d\n", v150_1_msg_id_to_str(msg_id), chan, seq_no);

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

SPAN_DECLARE(int) v150_1_test_rx_sprt_msg(v150_1_state_t *s, int chan, int seq_no, const uint8_t buf[], int len)
{
    process_rx_sprt_msg((void *) s, chan, seq_no, buf, len);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_set_local_busy(v150_1_state_t *s, bool busy)
{
    bool previous_busy;

    previous_busy = s->near.parms.busy;
    s->near.parms.busy = busy;
    return previous_busy;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(bool) v150_1_get_far_busy_status(v150_1_state_t *s)
{
    return s->far.parms.busy;
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
    s->near.parms.selmod = modulation;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_set_compression_direction(v150_1_state_t *s, int compression_direction)
{
    s->near.parms.selected_compression_direction = compression_direction;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_set_compression(v150_1_state_t *s, int compression)
{
    s->near.parms.selected_compression = compression;
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
    s->near.parms.compression_tx_dictionary_size = tx_dictionary_size;
    s->near.parms.compression_rx_dictionary_size = rx_dictionary_size;
    s->near.parms.compression_tx_string_length = tx_string_length;
    s->near.parms.compression_rx_string_length = rx_string_length;
    /* These are only relevant for V.44 */
    s->near.parms.compression_tx_history_size = tx_history_size;
    s->near.parms.compression_rx_history_size = rx_history_size;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_set_error_correction(v150_1_state_t *s, int error_correction)
{
    s->near.parms.selected_error_correction = error_correction;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_set_tx_symbol_rate(v150_1_state_t *s, bool enable, int rate)
{
    s->near.parms.txsen = enable;
    s->near.parms.txsr = (enable)  ?  rate  :  0;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_set_rx_symbol_rate(v150_1_state_t *s, bool enable, int rate)
{
    s->near.parms.rxsen = enable;
    s->near.parms.rxsr = (enable)  ?  rate  :  0;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_set_tx_data_signalling_rate(v150_1_state_t *s, int rate)
{
    s->near.parms.tdsr = rate;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_set_rx_data_signalling_rate(v150_1_state_t *s, int rate)
{
    s->near.parms.rdsr = rate;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void set_joint_cdscselect(v150_1_state_t *s)
{
    /* See Table 32/V.150.1 */
    if (s->near.parms.cdscselect == V150_1_CDSCSELECT_INDETERMINATE
        ||
        s->far.parms.cdscselect == V150_1_CDSCSELECT_INDETERMINATE)
    {
        s->cdscselect = V150_1_CDSCSELECT_INDETERMINATE;
    }
    else if (s->near.parms.cdscselect == V150_1_CDSCSELECT_AUDIO_RFC4733
        ||
        s->far.parms.cdscselect == V150_1_CDSCSELECT_AUDIO_RFC4733)
    {
        s->cdscselect = V150_1_CDSCSELECT_AUDIO_RFC4733;
    }
    else if (s->near.parms.cdscselect == V150_1_CDSCSELECT_VBD_PREFERRED
             ||
             s->far.parms.cdscselect == V150_1_CDSCSELECT_VBD_PREFERRED)
    {
        s->cdscselect = V150_1_CDSCSELECT_VBD_PREFERRED;
    }
    else
    {
        s->cdscselect = V150_1_CDSCSELECT_MIXED;
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v150_1_set_near_cdscselect(v150_1_state_t *s, v150_1_cdscselect_t select)
{
    s->near.parms.cdscselect = select;
    set_joint_cdscselect(s);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v150_1_set_far_cdscselect(v150_1_state_t *s, v150_1_cdscselect_t select)
{
    s->far.parms.cdscselect = select;
    set_joint_cdscselect(s);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v150_1_set_call_discrimination_timeout(v150_1_state_t *s, int timeout)
{
    s->call_discrimination_timeout = timeout;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) v150_1_get_logging_state(v150_1_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

int sse_status_handler(v150_1_state_t *s, int status)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "SSE status event %s\n", v150_1_sse_status_to_str(status));
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void sprt_status_handler(void *user_data, int status)
{
    v150_1_state_t *s;

    s = (v150_1_state_t *) user_data;
    span_log(&s->logging, SPAN_LOG_FLOW, "SPRT status event %d\n", status);
}
/*- End of function --------------------------------------------------------*/

static void call_discrimination_timer_expired(v150_1_state_t *s, span_timestamp_t now)
{
    v150_1_state_machine(s, V150_1_SIGNAL_CALL_DISCRIMINATION_TIMER_EXPIRED, NULL, 0);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v150_1_timer_expired(v150_1_state_t *s, span_timestamp_t now)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "V.150.1 timer expired at %lu\n", now);

    if (now < s->latest_timer)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "V.150.1 timer returned %luus early\n", s->latest_timer - now);
        /* Request the same timeout point again. */
        if (s->timer_handler)
            s->timer_handler(s->timer_user_data, s->latest_timer);
        /*endif*/
        return 0;
    }
    /*endif*/

    if (s->call_discrimination_timer != 0  &&  s->call_discrimination_timer <= now)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Call discrimination timer expired\n");
        call_discrimination_timer_expired(s, now);
    }
    /*endif*/
    if (s->sse_timer != 0  &&  s->sse_timer <= now)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "SSE timer expired\n");
        v150_1_sse_timer_expired(s, now);
    }
    /*endif*/
    if (s->sprt_timer != 0  &&  s->sprt_timer <= now)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "SPRT timer expired\n");
        sprt_timer_expired(&s->sprt, now);
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static span_timestamp_t select_timer(v150_1_state_t *s)
{
    span_timestamp_t shortest;
    int shortest_is;

    /* Find the earliest expiring of the active timers, and set the timeout to that. */
    shortest = ~0;
    shortest_is = 0;
    if (s->sprt_timer  &&  s->sprt_timer < shortest)
    {
        shortest = s->sprt_timer;
        shortest_is = 0;
    }
    /*endif*/
    if (s->sse_timer  &&  s->sse_timer < shortest)
    {
        shortest = s->sse_timer;
        shortest_is = 1;
    }
    /*endif*/
    if (s->call_discrimination_timer  &&  s->call_discrimination_timer < shortest)
    {
        shortest = s->call_discrimination_timer;
        shortest_is = 2;
    }
    /*endif*/

    /* If we haven't shrunk shortest from maximum, we have no timer to set, so we stop the timer,
       if its set. */
    if (shortest == ~0)
        shortest = 0;
    /*endif*/
    span_log(&s->logging, SPAN_LOG_FLOW, "Update timer to %lu (%d)\n", shortest, shortest_is);
    s->latest_timer = shortest;
    return shortest;
}
/*- End of function --------------------------------------------------------*/

static span_timestamp_t update_call_discrimination_timer(v150_1_state_t *s, span_timestamp_t timeout)
{
    span_timestamp_t res;

    if (timeout != ~0)
    {
        s->call_discrimination_timer = timeout;
        timeout = select_timer(s);
    }
    /*endif*/
    res = 0;
    if (s->timer_handler)
        res = s->timer_handler(s->timer_user_data, timeout);
    /*endif*/
    return res;
}
/*- End of function --------------------------------------------------------*/

span_timestamp_t update_sse_timer(void *user_data, span_timestamp_t timeout)
{
    v150_1_state_t *s;
    span_timestamp_t res;

    s = (v150_1_state_t *) user_data;
    if (timeout != ~0)
    {
        s->sse_timer = timeout;
        timeout = select_timer(s);
    }
    /*endif*/
    res = 0;
    if (s->timer_handler)
        res = s->timer_handler(s->timer_user_data, timeout);
    /*endif*/
    return res;
}
/*- End of function --------------------------------------------------------*/

static span_timestamp_t update_sprt_timer(void *user_data, span_timestamp_t timeout)
{
    v150_1_state_t *s;
    span_timestamp_t res;

    s = (v150_1_state_t *) user_data;
    if (timeout != ~0)
    {
        s->sprt_timer = timeout;
        timeout = select_timer(s);
    }
    /*endif*/
    res = 0;
    if (s->timer_handler)
        res = s->timer_handler(s->timer_user_data, timeout);
    /*endif*/
    return res;
}
/*- End of function --------------------------------------------------------*/

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
                                           void *spe_signal_handler_user_data)
{
    if (sprt_tx_packet_handler == NULL  ||  rx_data_handler == NULL  ||  rx_status_report_handler == NULL)
        return NULL;
    /*endif*/
    if (s == NULL)
    {
        if ((s = (v150_1_state_t *) span_alloc(sizeof(*s))) == NULL)
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

    s->near.parms.v42bis_p0 = 3;
    s->near.parms.v42bis_p1 = 512;
    s->near.parms.v42bis_p2 = 6;
    s->near.parms.v44_c0 = 0;
    s->near.parms.v44_p0 = 0;
    s->near.parms.v44_p1t = 0;
    s->near.parms.v44_p1r = 0;
    s->near.parms.v44_p2t = 0;
    s->near.parms.v44_p2r = 0;
    s->near.parms.v44_p3t = 0;
    s->near.parms.v44_p3r = 0;

    s->near.parms.jm_category_id_seen[V150_1_JM_CATEGORY_ID_CALL_FUNCTION_1] = true;
    s->near.parms.jm_category_info[V150_1_JM_CATEGORY_ID_CALL_FUNCTION_1] = V150_1_JM_CALL_FUNCTION_V_SERIES;
    s->near.parms.jm_category_id_seen[V150_1_JM_CATEGORY_ID_MODULATION_MODES] = true;
    s->near.parms.jm_category_info[V150_1_JM_CATEGORY_ID_MODULATION_MODES] =
          V150_1_JM_MODULATION_MODE_V34_AVAILABLE
        | V150_1_JM_MODULATION_MODE_V32_V32bis_AVAILABLE
        | V150_1_JM_MODULATION_MODE_V22_V22bis_AVAILABLE
        | V150_1_JM_MODULATION_MODE_V21_AVAILABLE;
    s->near.parms.jm_category_id_seen[V150_1_JM_CATEGORY_ID_PROTOCOLS] = true;
    s->near.parms.jm_category_info[V150_1_JM_CATEGORY_ID_PROTOCOLS] = V150_1_JM_PROTOCOL_V42_LAPM;
    s->near.parms.jm_category_id_seen[V150_1_JM_CATEGORY_ID_PSTN_ACCESS] = true;
    s->near.parms.jm_category_info[V150_1_JM_CATEGORY_ID_PSTN_ACCESS] = 0;
    s->near.parms.jm_category_id_seen[V150_1_JM_CATEGORY_ID_PCM_MODEM_AVAILABILITY] = false;
    s->near.parms.jm_category_info[V150_1_JM_CATEGORY_ID_PCM_MODEM_AVAILABILITY] = 0;
    s->near.parms.jm_category_id_seen[V150_1_JM_CATEGORY_ID_EXTENSION] = false;
    s->near.parms.jm_category_info[V150_1_JM_CATEGORY_ID_EXTENSION] = 0;

    s->near.parms.selmod = V150_1_SELMOD_NULL;
    s->near.parms.selected_compression_direction = V150_1_COMPRESS_NEITHER_WAY;
    s->near.parms.selected_compression = V150_1_COMPRESSION_NONE;
    s->near.parms.selected_error_correction = V150_1_ERROR_CORRECTION_NONE;
    s->near.parms.tdsr = 0;
    s->near.parms.rdsr = 0;
    s->near.parms.txsen = false;
    s->near.parms.txsr = V150_1_SYMBOL_RATE_NULL;
    s->near.parms.rxsen = false;
    s->near.parms.rxsr = V150_1_SYMBOL_RATE_NULL;

    /* Set default values that suit V.42bis */
    s->near.parms.compression_tx_dictionary_size = 512;
    s->near.parms.compression_rx_dictionary_size = 512;
    s->near.parms.compression_tx_string_length = 6;
    s->near.parms.compression_rx_string_length = 6;
    s->near.parms.compression_tx_history_size = 0;
    s->near.parms.compression_rx_history_size = 0;

    s->near.parms.ecp = V150_1_ERROR_CORRECTION_V42_LAPM;
    s->near.parms.v42_lapm_supported = true;
    s->near.parms.v42_annex_a_supported = false;  /* This will never be supported, as it was removed from the V.42 spec in 2002. */
    s->near.parms.v42bis_supported = true;
    s->near.parms.v44_supported = false;
    s->near.parms.mnp5_supported = false;

    s->near.parms.necrxch_option = false;
    s->near.parms.ecrxch_option = true;
    s->near.parms.xid_profile_exchange_supported = false;
    s->near.parms.asymmetric_data_types_supported = false;

    s->near.parms.i_raw_bit_supported = false;
    s->near.parms.i_frame_supported = false;
    s->near.parms.i_char_stat_supported = false;
    s->near.parms.i_char_dyn_supported = false;
    s->near.parms.i_octet_cs_supported = true;
    s->near.parms.i_char_stat_cs_supported = false;
    s->near.parms.i_char_dyn_cs_supported = false;

    /* Set a default character format. */
    s->near.parms.data_format_code = (V150_1_DATA_BITS_7 << 6)
                                   | (V150_1_PARITY_EVEN << 3)
                                   | V150_1_STOP_BITS_1;
    s->far.parms.data_format_code = -1;

    s->remote_media_state = V150_1_MEDIA_STATE_INITIAL_AUDIO;
    s->local_media_state = V150_1_MEDIA_STATE_INITIAL_AUDIO;

    s->call_discrimination_timeout = V150_1_CALL_DISCRIMINATION_DEFAULT_TIMEOUT;

    s->near.parms.sprt_subsession_id = 0;
    s->near.parms.sprt_payload_type = sprt_tx_payload_type;
    s->far.parms.sprt_payload_type = sprt_rx_payload_type;

    s->rx_data_handler = rx_data_handler;
    s->rx_data_handler_user_data = rx_data_handler_user_data;
    s->rx_status_report_handler = rx_status_report_handler;
    s->rx_status_report_user_data = rx_status_report_user_data;

    s->timer_handler = v150_1_timer_handler;
    s->timer_user_data = v150_1_timer_user_data;

    v150_1_sse_init(s, 
                    sse_tx_packet_handler,
                    sse_tx_packet_user_data);

    sprt_init(&s->sprt,
              s->near.parms.sprt_subsession_id,
              s->near.parms.sprt_payload_type,
              s->far.parms.sprt_payload_type,
              NULL /* Use default params */,
              sprt_tx_packet_handler,
              sprt_tx_packet_handler_user_data,
              process_rx_sprt_msg,
              s,
              update_sprt_timer,
              s,
              sprt_status_handler,
              s);

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
