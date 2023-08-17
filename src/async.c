/*
 * SpanDSP - a series of DSP components for telephony
 *
 * async.c - Asynchronous serial bit stream encoding and decoding
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003 Steve Underwood
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

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#else
#include "spandsp/stdbool.h"
#endif

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/bit_operations.h"
#include "spandsp/async.h"

#include "spandsp/private/async.h"

SPAN_DECLARE(const char *) signal_status_to_str(int status)
{
    switch (status)
    {
    case SIG_STATUS_CARRIER_DOWN:
        return "Carrier down";
    case SIG_STATUS_CARRIER_UP:
        return "Carrier up";
    case SIG_STATUS_TRAINING_IN_PROGRESS:
        return "Training in progress";
    case SIG_STATUS_TRAINING_SUCCEEDED:
        return "Training succeeded";
    case SIG_STATUS_TRAINING_FAILED:
        return "Training failed";
    case SIG_STATUS_FRAMING_OK:
        return "Framing OK";
    case SIG_STATUS_END_OF_DATA:
        return "End of data";
    case SIG_STATUS_ABORT:
        return "Abort";
    case SIG_STATUS_BREAK:
        return "Break";
    case SIG_STATUS_SHUTDOWN_COMPLETE:
        return "Shutdown complete";
    case SIG_STATUS_OCTET_REPORT:
        return "Octet report";
    case SIG_STATUS_POOR_SIGNAL_QUALITY:
        return "Poor signal quality";
    case SIG_STATUS_MODEM_RETRAIN_OCCURRED:
        return "Modem retrain occurred";
    case SIG_STATUS_LINK_CONNECTED:
        return "Link connected";
    case SIG_STATUS_LINK_DISCONNECTED:
        return "Link disconnected";
    case SIG_STATUS_LINK_ERROR:
        return "Link error";
    case SIG_STATUS_LINK_IDLE:
        return "Link idle";
    }
    /*endswitch*/
    return "???";
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) async_rx_put_bit(void *user_data, int bit)
{
    async_rx_state_t *s;
    int parity_bit_a;
    int parity_bit_b;

    s = (async_rx_state_t *) user_data;
    if (bit < 0)
    {
        /* Special conditions */
        switch (bit)
        {
        case SIG_STATUS_CARRIER_UP:
        case SIG_STATUS_CARRIER_DOWN:
        case SIG_STATUS_TRAINING_IN_PROGRESS:
        case SIG_STATUS_TRAINING_SUCCEEDED:
        case SIG_STATUS_TRAINING_FAILED:
        case SIG_STATUS_END_OF_DATA:
            s->put_byte(s->user_data, bit);
            s->bitpos = 0;
            s->frame_in_progress = 0;
            break;
        default:
            //printf("Eh!\n");
            break;
        }
        /*endswitch*/
    }
    else
    {
        if (s->bitpos == 0)
        {
            /* Search for the start bit */
            s->bitpos += (bit ^ 1);
            s->frame_in_progress = 0;
        }
        else if (s->bitpos <= s->total_data_bits)
        {
            s->frame_in_progress = (s->frame_in_progress >> 1) | (bit << 15);
            s->bitpos++;
        }
        else
        {
            /* We should be at the first stop bit */
            if (bit == 0  &&  !s->use_v14)
            {
                s->framing_errors++;
                s->bitpos = 0;
            }
            else
            {
                /* Check and remove any parity bit */
                if (s->parity != ASYNC_PARITY_NONE)
                {
                    parity_bit_a = (s->frame_in_progress >> 15) & 0x01;
                    /* Trim off the parity bit */
                    s->frame_in_progress &= 0x7FFF;
                    s->frame_in_progress >>= (16 - s->total_data_bits);
                    switch (s->parity)
                    {
                    case ASYNC_PARITY_ODD:
                        parity_bit_b = parity8(s->frame_in_progress) ^ 1;
                        break;
                    case ASYNC_PARITY_EVEN:
                        parity_bit_b = parity8(s->frame_in_progress);
                        break;
                    case ASYNC_PARITY_MARK:
                        parity_bit_b = 1;
                        break;
                    default:
                    case ASYNC_PARITY_SPACE:
                        parity_bit_b = 0;
                        break;
                    }
                    /*endswitch*/
                    if (parity_bit_a == parity_bit_b)
                        s->put_byte(s->user_data, s->frame_in_progress);
                    else
                        s->parity_errors++;
                    /*endif*/
                }
                else
                {
                    s->frame_in_progress >>= (16 - s->total_data_bits);
                    s->put_byte(s->user_data, s->frame_in_progress);
                }
                /*endif*/
                if (bit == 1)
                {
                    /* This is the first of any stop bits */
                    s->bitpos = 0;
                }
                else
                {
                    /* There might be a framing error, but we have to assume the stop
                       bit has been dropped by the rate adaption mechanism described in
                       V.14. */
                    s->bitpos = 1;
                    s->frame_in_progress = 0;
                }
                /*endif*/
            }
            /*endif*/
        }
        /*endif*/
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) async_rx_get_parity_errors(async_rx_state_t *s, bool reset)
{
    int errors;

    errors = s->parity_errors;
    if (reset)
        s->parity_errors = 0;
    /*endif*/
    return errors;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) async_rx_get_framing_errors(async_rx_state_t *s, bool reset)
{
    int errors;

    errors = s->framing_errors;
    if (reset)
        s->framing_errors = 0;
    /*endif*/
    return errors;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(async_rx_state_t *) async_rx_init(async_rx_state_t *s,
                                               int data_bits,
                                               int parity,
                                               int stop_bits,
                                               bool use_v14,
                                               span_put_byte_func_t put_byte,
                                               void *user_data)
{
    if (s == NULL)
    {
        if ((s = (async_rx_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
        /*endif*/
    }
    /*endif*/
    /* We don't record the stop bits, as they are currently only in the function
       call for completeness, and future compatibility. */
    s->data_bits = data_bits;
    s->parity = parity;
    s->total_data_bits = data_bits;
    if (parity != ASYNC_PARITY_NONE)
        s->total_data_bits++;
    /*endif*/
    s->use_v14 = use_v14;

    s->put_byte = put_byte;
    s->user_data = user_data;

    s->frame_in_progress = 0;
    s->bitpos = 0;

    s->parity_errors = 0;
    s->framing_errors = 0;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) async_rx_release(async_rx_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) async_rx_free(async_rx_state_t *s)
{
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) async_tx_get_bit(void *user_data)
{
    async_tx_state_t *s;
    int bit;
    int parity_bit;
    int32_t next_byte;

    s = (async_tx_state_t *) user_data;
    if (s->bitpos == 0)
    {
        if (s->presend_bits > 0)
        {
            s->presend_bits--;
            return 1;
        }
        /*endif*/
        if ((next_byte = s->get_byte(s->user_data)) < 0)
        {
            if (next_byte != SIG_STATUS_LINK_IDLE)
                return next_byte;
            /*endif*/
            /* Idle for a bit time. If the get byte call configured a presend
               time we might idle for longer. */
            return 1;
        }
        /*endif*/
        s->frame_in_progress = next_byte;
        /* Trim off any upper bits */
        s->frame_in_progress &= (0xFFFF >> (16 - s->data_bits));
        /* Now insert any parity bit */
        switch (s->parity)
        {
        case ASYNC_PARITY_MARK:
            s->frame_in_progress |= (1 << s->data_bits);
            break;
        case ASYNC_PARITY_EVEN:
            parity_bit = parity8(s->frame_in_progress);
            s->frame_in_progress |= (parity_bit << s->data_bits);
            break;
        case ASYNC_PARITY_ODD:
            parity_bit = parity8(s->frame_in_progress) ^ 1;
            s->frame_in_progress |= (parity_bit << s->data_bits);
            break;
        }
        /*endswitch*/
        /* Insert some stop bits above the data and parity ones */
        s->frame_in_progress |= (0xFFFF << s->total_data_bits);
        /* Start bit */
        bit = 0;
        s->bitpos++;
    }
    else
    {
        bit = s->frame_in_progress & 1;
        s->frame_in_progress >>= 1;
        if (++s->bitpos > s->total_bits)
            s->bitpos = 0;
        /*endif*/
    }
    /*endif*/
    return bit;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) async_tx_presend_bits(async_tx_state_t *s, int bits)
{
    s->presend_bits = bits;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(async_tx_state_t *) async_tx_init(async_tx_state_t *s,
                                               int data_bits,
                                               int parity,
                                               int stop_bits,
                                               bool use_v14,
                                               span_get_byte_func_t get_byte,
                                               void *user_data)
{
    if (s == NULL)
    {
        if ((s = (async_tx_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
        /*endif*/
    }
    /*endif*/
    /* We have a use_v14 parameter for completeness, but right now V.14 only
       applies to the receive side. We are unlikely to have an application where
       flow control does not exist, so V.14 stuffing is not needed. */
    s->data_bits = data_bits;
    s->parity = parity;
    s->total_data_bits = data_bits;
    if (parity != ASYNC_PARITY_NONE)
        s->total_data_bits++;
    /*endif*/
    s->total_bits = s->total_data_bits + stop_bits;
    s->get_byte = get_byte;
    s->user_data = user_data;

    s->frame_in_progress = 0;
    s->bitpos = 0;
    s->presend_bits = 0;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) async_tx_release(async_tx_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) async_tx_free(async_tx_state_t *s)
{
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
