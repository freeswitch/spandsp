/*
 * SpanDSP - a series of DSP components for telephony
 *
 * sprt_decode.c
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

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#if !defined(_WIN32)
#include <unistd.h>
#endif
#include <sys/time.h>
#include <arpa/inet.h>

#include "spandsp.h"
#include "spandsp-sim.h"

#include "fax_utils.h"
#include "pcap_parse.h"

#define INPUT_FILE_NAME         "sprt.pcap"

#define OUTPUT_WAVE_FILE_NAME   "sprt_decode.wav"

#define SAMPLES_PER_CHUNK       160

static struct timeval now;

static int64_t previous_fwd_time = 0;
static int64_t previous_rev_time = 0;
static int64_t current_time = 0;

static sprt_state_t *sprt;
static v150_1_state_t *v150_1;

static int sprt_pt = 120;

static span_timestamp_t sprt_timer_handler(void *user_data, span_timestamp_t timeout)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int sprt_tx_packet_handler(void *user_data, const uint8_t msg[], int len)
{
    int i;

    printf("Response_packet %5d >>> ", len);
    for (i = 0;  i < len;  i++)
        printf("%02X ", msg[i]);
    /*endfor*/
    printf("\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_status_report_handler(void *user_data, v150_1_status_t *report)
{
    printf("V.150.1 status report received\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_octet_stream_handler(void *user_data, const uint8_t msg[], int len, int fill)
{
    int i;

    if (fill > 0)
        printf("%d missing characters\n", fill);
    /*endif*/
    printf(">>>");
    for (i = 0;  i < len;  i++)
    {
        printf(" %02x", msg[i]);
    }
    /*endfor*/
    printf("<<<\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int timing_update(void *user_data, struct timeval *ts)
{
    current_time = ts->tv_sec*1000000LL + ts->tv_usec;
    if (previous_fwd_time == 0)
        previous_fwd_time = current_time;
    /*endif*/
    if (previous_rev_time == 0)
        previous_rev_time = current_time;
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int sprt_log(const uint8_t *pkt, int len)
{
    int i;
    int msgid;

    i = 0;
    if (len > 0)
    {
        msgid = pkt[i++];
        printf("MSG_ID=%s,", v150_1_msg_id_to_str(msgid));
        switch (msgid)
        {
        case V150_1_MSGID_NULL:
            if (len != 1)
                printf("Bad length ");
            /*endif*/
            for (  ;  i < len;  i++)
                printf(" %02X", pkt[i]);
            /*endfor*/
            break;
        case V150_1_MSGID_INIT:
            if (len != 2 + 1)
                printf("Bad length ");
            /*endif*/
            for (  ;  i < len;  i++)
                printf(" %02X", pkt[i]);
            /*endfor*/
            break;
        case V150_1_MSGID_XID_XCHG:
            if (len != 18 + 1)
                printf("Bad length ");
            /*endif*/
            for (  ;  i < len;  i++)
                printf(" %02X", pkt[i]);
            /*endfor*/
            break;
        case V150_1_MSGID_JM_INFO:
            if (len < 2 + 1)
                printf("Bad length ");
            /*endif*/
            for (  ;  i < len;  i++)
                printf(" %02X", pkt[i]);
            /*endfor*/
            break;
        case V150_1_MSGID_START_JM:
            if (len != 1)
                printf("Bad length ");
            /*endif*/
            for (  ;  i < len;  i++)
                printf(" %02X", pkt[i]);
            /*endfor*/
            break;
        case V150_1_MSGID_CONNECT:
            if (len < 8 + 1  ||  len > 18 + 1)
                printf("Bad length ");
            /*endif*/
            for (  ;  i < len;  i++)
                printf(" %02X", pkt[i]);
            /*endfor*/
            break;
        case V150_1_MSGID_BREAK:
            if (len != 2 + 1)
                printf("Bad length ");
            /*endif*/
            for (  ;  i < len;  i++)
                printf(" %02X", pkt[i]);
            /*endfor*/
            break;
        case V150_1_MSGID_BREAKACK:
            if (len != 1)
                printf("Bad length ");
            /*endif*/
            for (  ;  i < len;  i++)
                printf(" %02X", pkt[i]);
            /*endfor*/
            break;
        case V150_1_MSGID_MR_EVENT:
            if (len != 9 + 1)
                printf("Bad length ");
            /*endif*/
            for (  ;  i < len;  i++)
                printf(" %02X", pkt[i]);
            /*endfor*/
            break;
        case V150_1_MSGID_CLEARDOWN:
            if (len != 3 + 1)
                printf("Bad length ");
            /*endif*/
            for (  ;  i < len;  i++)
                printf(" %02X", pkt[i]);
            /*endfor*/
            break;
        case V150_1_MSGID_PROF_XCHG:
            if (len < 8 + 1  ||  len > 18 + 1)
                printf("Bad length ");
            /*endif*/
            for (  ;  i < len;  i++)
                printf(" %02X", pkt[i]);
            /*endfor*/
            break;
        case V150_1_MSGID_I_RAW_OCTET:
            if (len < 1 + 1)
                printf("Bad length ");
            /*endif*/
            for (  ;  i < len;  i++)
                printf(" %02X", pkt[i]);
            /*endfor*/
            break;
        case V150_1_MSGID_I_RAW_BIT:
            if (len < 1 + 1)
                printf("Bad length ");
            /*endif*/
            for (  ;  i < len;  i++)
                printf(" %02X", pkt[i]);
            /*endfor*/
            break;
        case V150_1_MSGID_I_OCTET:
            /* Minimum could be 1 or 2, depending on DLCI setting */
            if (len < 1 + 1)
                printf("Bad length ");
            /*endif*/
            for (  ;  i < len;  i++)
                printf(" %02X", pkt[i]);
            /*endfor*/
            break;
        case V150_1_MSGID_I_CHAR_STAT:
            if (len < 1 + 1)
                printf("Bad length ");
            /*endif*/
            for (  ;  i < len;  i++)
                printf(" %02X", pkt[i]);
            /*endfor*/
            break;
        case V150_1_MSGID_I_CHAR_DYN:
            if (len < 1 + 1)
                printf("Bad length ");
            /*endif*/
            for (  ;  i < len;  i++)
                printf(" %02X", pkt[i]);
            /*endfor*/
            break;
        case V150_1_MSGID_I_FRAME:
            if (len < 1 + 1)
                printf("Bad length ");
            /*endif*/
            for (  ;  i < len;  i++)
                printf(" %02X", pkt[i]);
            /*endfor*/
            break;
        case V150_1_MSGID_I_OCTET_CS:
            if (len < 1 + 2)
                printf("Bad length ");
            /*endif*/
            for (  ;  i < len;  i++)
                printf(" %02X", pkt[i]);
            /*endfor*/
            break;
        case V150_1_MSGID_I_CHAR_STAT_CS:
            if (len < 1 + 3)
                printf("Bad length ");
            /*endif*/
            for (  ;  i < len;  i++)
                printf(" %02X", pkt[i]);
            /*endfor*/
            break;
        case V150_1_MSGID_I_CHAR_DYN_CS:
            if (len != 1 + 3)
                printf("Bad length ");
            /*endif*/
            for (  ;  i < len;  i++)
                printf(" %02X", pkt[i]);
            /*endfor*/
            break;
        default:
            printf("Unknown MSGID ");
            for (  ;  i < len;  i++)
                printf(" %02X", pkt[i]);
            /*endfor*/
            break;
        }
        /*endswitch*/
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int process_packet(void *user_data, const uint8_t *pkt, int len, bool forward)
{
    int64_t diff;
    float fdiff;
    int noa;
    int i;
    int j;
    int pt;

    if (sprt == NULL)
    {
        sprt = sprt_init(NULL,
                         0,
                         120,
                         120,
                         NULL,
                         sprt_tx_packet_handler,
                         NULL,
                         NULL,
                         NULL,
                         sprt_timer_handler,
                         NULL,
                         NULL,
                         NULL);
    }
    /*endif*/
    if (v150_1 == NULL)
    {
        v150_1 = v150_1_init(NULL,
                             sprt_tx_packet_handler,
                             NULL,
                             120,
                             120,
                             NULL,
                             NULL,
                             NULL,
                             NULL,
                             v150_1_octet_stream_handler,
                             NULL,
                             v150_1_status_report_handler,
                             NULL,
                             NULL,
                             NULL);
    }
    /*endif*/

    if (forward)
    {
        diff = current_time - previous_fwd_time;
        fdiff = (float) diff/1000000.0;
        previous_fwd_time = current_time;
    }
    else
    {
        diff = current_time - previous_rev_time;
        fdiff = (float) diff/1000000.0;
        previous_rev_time = current_time;
    }
    /*endif*/
    if ((pkt[0] & 0x80) == 0)
    {
        pt = pkt[1] & 0x7F;
        if (sprt_pt == 0  ||  pt == sprt_pt)
        {
            noa = (pkt[4] >> 6) & 0x03;
            printf("%s %fs %3d >>> ", (forward)  ?  "FWD"  :  "REV", fdiff, len);
            printf("SSID=%d, PT=%d, TC=%d, SQN=%d, BSN=%d",
                   pkt[0] & 0x7F,
                   pt,
                   (pkt[2] >> 6) & 0x03,
                   ((pkt[2] << 8) | pkt[3]) & 0x3FFF,
                   ((pkt[4] << 8) | pkt[5]) & 0x3FFF);
            i = 6;
            if (noa > 0)
            {
                printf(", (ACKS ");
                for (j = 0;  j < noa;  j++)
                {
                    if (j > 0)
                        printf(", ");
                    printf("TC=%d SQN=%d", (pkt[i] >> 6) & 0x03, ((pkt[i] << 8) | pkt[i + 1]) & 0x3FFF);
                    i += 2;
                }
                /*endfor*/
                printf(")");
            }
            /*endif*/
            if (len > i)
            {
                printf(" - ");
                sprt_log(&pkt[i], len - i);
            }
            /*endif*/
            printf("\n");
        }
        /*endif*/
    }
    /*endif*/
#if 0
    if (forward)
    {
        printf("FWD packet arrived after %fs\n", fdiff);
        sprt_rx_packet(sprt, pkt, len);
    }
    else
    {
        printf("REV packet arrived after %fs\n", fdiff);
        /* ??? */
    }
    /*endif*/
#endif
    return 0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    const char *input_file_name;
    int opt;
    uint8_t src_addr[INET6_ADDRSTRLEN];
    uint16_t src_port;
    uint8_t dest_addr[INET6_ADDRSTRLEN];
    uint16_t dest_port;

    input_file_name = INPUT_FILE_NAME;
    memset(src_addr, 0, sizeof(src_addr));
    src_port = 0;
    memset(dest_addr, 0, sizeof(dest_addr));
    dest_port = 0;
    sprt_pt = 120;
    while ((opt = getopt(argc, argv, "D:d:i:m:op:S:s:")) != -1)
    {
        switch (opt)
        {
        case 'D':
            if (inet_pton(AF_INET, optarg, dest_addr) <= 0)
            {
                fprintf(stderr, "Bad destination address\n");
                return -1;
            }
            /*endif*/
            break;
        case 'd':
            dest_port = atoi(optarg);
            break;
        case 'i':
            input_file_name = optarg;
            break;
        case 'S':
            if (inet_pton(AF_INET, optarg, src_addr) <= 0)
            {
                fprintf(stderr, "Bad destination address\n");
                return -1;
            }
            /*endif*/
            break;
        case 'p':
            sprt_pt = atoi(optarg);
            break;
        case 's':
            src_port = atoi(optarg);
            break;
        default:
            //usage();
            exit(2);
            break;
        }
        /*endswitch*/
    }
    /*endwhile*/

    if (pcap_scan_pkts(input_file_name, src_addr, src_port, dest_addr, dest_port, true, timing_update, process_packet, NULL))
        exit(2);
    /*endif*/
    /* Push the time along, to flush out any remaining activity from the application. */
    now.tv_sec += 60;
    timing_update(NULL, &now);
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
