/*
 * SpanDSP - a series of DSP components for telephony
 *
 * sprt.c - An implementation of the SPRT protocol defined in V.150.1
 *          Annex B, less the packet exchange part
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

#define SPANDSP_FULLY_DEFINE_SPRT_STATE_T

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/unaligned.h"
#include "spandsp/logging.h"
#include "spandsp/async.h"
#include "spandsp/sprt.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/sprt.h"

/* V.150.1 consists of
    V.150.1 (01/03)
        The main spec
    V.150.1 (2003) Corrigendum 1 (07/03)
        This was merged into the spec, and so is irrelevant
    V.150.1 (2003) Corrigendum 2 (03/04)
        Fixes Table 15, Annex E.1, Annex E.1.4, E.1.5, E.2.3
    V.150.1 (2003) Amendment 1 (01/05)
        Additions to Table 12 for VBD and ToIP
    V.150.1 (2003) Amendment 2 (05/06)
        These are mostly ToIP and VBD changes.
        Additions/changes  to 2, 3.2, 10, 15.3, 15.4, Table 16, 15.4.1,
        15.4.5, 15.4.11.8, 15.4.11.9, 15.4.11.10, 17, 18, 19, C.2.5,
        C.2.6, C.3, C.5.2, C.5.3, C.5.5, Annex D, Appendix IV
*/

static struct
{
    uint16_t min_payload_bytes;
    uint16_t max_payload_bytes;
    uint16_t min_window_size;
    uint16_t max_window_size;
} channel_parm_limits[SPRT_CHANNELS] =
{
    {
        SPRT_MIN_TC0_PAYLOAD_BYTES,
        SPRT_MAX_TC0_PAYLOAD_BYTES,
        1,
        1
    },
    {
        SPRT_MIN_TC1_PAYLOAD_BYTES,
        SPRT_MAX_TC1_PAYLOAD_BYTES,
        SPRT_MIN_TC1_WINDOWS_SIZE,
        SPRT_MAX_TC1_WINDOWS_SIZE
    },
    {
        SPRT_MIN_TC2_PAYLOAD_BYTES,
        SPRT_MAX_TC2_PAYLOAD_BYTES,
        SPRT_MIN_TC2_WINDOWS_SIZE,
        SPRT_MAX_TC2_WINDOWS_SIZE
    },
    {
        SPRT_MIN_TC3_PAYLOAD_BYTES,
        SPRT_MAX_TC3_PAYLOAD_BYTES,
        1,
        1
    }
};

static channel_parms_t default_channel_parms[SPRT_CHANNELS] =
{
    {
        SPRT_DEFAULT_TC0_PAYLOAD_BYTES,
        1,
        -1,
        -1,
        -1
    },
    {
        SPRT_DEFAULT_TC1_PAYLOAD_BYTES,
        SPRT_DEFAULT_TC1_WINDOWS_SIZE,
        SPRT_DEFAULT_TIMER_TC1_TA01,
        SPRT_DEFAULT_TIMER_TC1_TA02,
        SPRT_DEFAULT_TIMER_TC1_TR03
    },
    {
        SPRT_DEFAULT_TC2_PAYLOAD_BYTES,
        SPRT_DEFAULT_TC2_WINDOWS_SIZE,
        SPRT_DEFAULT_TIMER_TC2_TA01,
        SPRT_DEFAULT_TIMER_TC2_TA02,
        SPRT_DEFAULT_TIMER_TC2_TR03
    },
    {
        SPRT_DEFAULT_TC3_PAYLOAD_BYTES,
        1,
        -1,
        -1,
        -1
    }
};

SPAN_DECLARE(const char *) sprt_transmission_channel_to_str(int channel)
{
    const char *res;

    res = "unknown";
    switch (channel)
    {
    case SPRT_TCID_UNRELIABLE_UNSEQUENCED:
        res = "unreliable unsequenced";
        break;
    case SPRT_TCID_RELIABLE_SEQUENCED:
        res = "reliable sequenced";
        break;
    case SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED:
        res = "expedited reliable sequenced";
        break;
    case SPRT_TCID_UNRELIABLE_SEQUENCED:
        res = "unreliable sequenced";
        break;
    }
    /*endswitch*/
    return res;
}
/*- End of function --------------------------------------------------------*/

static int update_timer(sprt_state_t *s)
{
    span_timestamp_t shortest;
    uint8_t first;
    int i;
    int shortest_is;

    if (s->tx.immediate_timer)
    {
        shortest = 1;
        shortest_is = 4;
    }
    else
    {
        /* Find the earliest expiring of the active timers, and set the timeout to that. */
        shortest = ~0;
        shortest_is = 0;
        /* There's a single ACK holdoff timer */
        if (s->tx.ta01_timer != 0  &&  s->tx.ta01_timer < shortest)
        {
            shortest = s->tx.ta01_timer;
            shortest_is = 1;
        }
        /*endif*/
        for (i = SPRT_TCID_MIN_RELIABLE;  i <= SPRT_TCID_MAX_RELIABLE;  i++)
        {
            /* There's a keepalive timer for each reliable channel. These are only active
               after the channel is used for the first time, and stay active until shutdown. */
            if (s->tx.chan[i].ta02_timer != 0  &&  s->tx.chan[i].ta02_timer < shortest)
            {
                shortest = s->tx.chan[i].ta02_timer;
                shortest_is = 2 + 10*i;
            }
            /*endif*/
            /* There are per slot timers for all the buffer slots for a reliable channel, but they are
               sorted, so we already know which is the sortest one. */
            if ((first = s->tx.chan[i].first_in_time) != TR03_QUEUE_FREE_SLOT_TAG)
            {
                if (s->tx.chan[i].tr03_timer[first] != 0  &&  s->tx.chan[i].tr03_timer[first] < shortest)
                {
                    shortest = s->tx.chan[i].tr03_timer[first];
                    shortest_is = 3 + 10*i;
                }
                /*endif*/
            }
            /*endif*/
        }
        /*endfor*/
        /* If we haven't shrunk shortest from maximum, we have no timer to set, so we stop the timer,
           if its set. */
        if (shortest == ~0)
            shortest = 0;
        /*endif*/
    }
    /*endif*/
    span_log(&s->logging, SPAN_LOG_FLOW, "Update timer to %lu (%d)\n", shortest, shortest_is);
    s->latest_timer = shortest;
    if (s->timer_handler)
        s->timer_handler(s->timer_user_data, s->latest_timer);
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void delete_timer_queue_entry(sprt_state_t *s, int channel, int slot)
{
    if (s->tx.chan[channel].first_in_time == TR03_QUEUE_FREE_SLOT_TAG  ||  slot == TR03_QUEUE_FREE_SLOT_TAG)
        return;
    /*endif*/

    if (s->tx.chan[channel].first_in_time == slot)
    {
        /* Delete from the head of the list */
        s->tx.chan[channel].first_in_time = s->tx.chan[channel].next_in_time[slot];
    }
    else
    {
        s->tx.chan[channel].next_in_time[s->tx.chan[channel].prev_in_time[slot]] = s->tx.chan[channel].next_in_time[slot];
    }
    /*endif*/

    if (s->tx.chan[channel].last_in_time == slot)
    {
        /* Delete from the end of the list */
        s->tx.chan[channel].last_in_time = s->tx.chan[channel].prev_in_time[slot];
    }
    else
    {
        s->tx.chan[channel].prev_in_time[s->tx.chan[channel].next_in_time[slot]] = s->tx.chan[channel].prev_in_time[slot];
    }
    /*endif*/

    s->tx.chan[channel].prev_in_time[slot] = TR03_QUEUE_FREE_SLOT_TAG;
    s->tx.chan[channel].next_in_time[slot] = TR03_QUEUE_FREE_SLOT_TAG;
}
/*- End of function --------------------------------------------------------*/

static void add_timer_queue_last_entry(sprt_state_t *s, int channel, int slot)
{
    if (s->tx.chan[channel].last_in_time == TR03_QUEUE_FREE_SLOT_TAG)
    {
        /* The list is empty, so link both ways */
        s->tx.chan[channel].first_in_time = slot;
    }
    else
    {
        s->tx.chan[channel].next_in_time[s->tx.chan[channel].last_in_time] = slot;
    }
    /*endif*/
    s->tx.chan[channel].prev_in_time[slot] = s->tx.chan[channel].last_in_time;
    s->tx.chan[channel].next_in_time[slot] = TR03_QUEUE_FREE_SLOT_TAG;
    s->tx.chan[channel].last_in_time = slot;
}
/*- End of function --------------------------------------------------------*/

static int build_and_send_packet(sprt_state_t *s,
                                 int channel,
                                 uint16_t seq_no,
                                 const uint8_t payload[],
                                 int payload_len)
{
    int i;
    int len;
    int noa;
    uint8_t pkt[SPRT_MAX_PACKET_BYTES];

    pkt[0] = s->tx.subsession_id;
    pkt[1] = s->tx.payload_type;
    put_net_unaligned_uint16(&pkt[2], (channel << 14) | (seq_no & SPRT_SEQ_NO_MASK));
    /* The header is of variable length, depending how many of the zero to three acknowledgement
       slots are in use */
    len = 6;
    noa = 0;
    if (s->tx.ack_queue_ptr > 0)
    {
        for (i = 0;  i < s->tx.ack_queue_ptr;  i++)
        {
            put_net_unaligned_uint16(&pkt[len], s->tx.ack_queue[i]);
            len += 2;
            noa++;
        }
        /*endfor*/
        s->tx.ack_queue_ptr = 0;
        s->tx.ta01_timer = 0;
        span_log(&s->logging, SPAN_LOG_FLOW, "TA01 cancelled\n");
    }
    /*endif*/
    /* The base sequence number only varies for the reliable channels. It is always zero
       for the unrelaible channels. */
    put_net_unaligned_uint16(&pkt[4], (noa << 14) | s->rx.chan[channel].base_sequence_no);
    /* If this is purely an acknowledgement packet, there will be no actual message */
    if (payload_len > 0)
    {
        memcpy(&pkt[len], payload, payload_len);
        len += payload_len;
    }
    /*endif*/
    span_log_buf(&s->logging, SPAN_LOG_FLOW, "Tx", pkt, len);
    if (s->tx_packet_handler)
        s->tx_packet_handler(s->tx_user_data, pkt, len);
    /*endif*/
    update_timer(s);
    return len;
}
/*- End of function --------------------------------------------------------*/

static int queue_acknowledgement(sprt_state_t *s, int channel, uint16_t sequence_no)
{
    uint16_t entry;
    bool found;
    int i;

    if (s->tx.ack_queue_ptr >= 3)
    {
        /* The ack queue is already full. This should never happen. It is an internal error
           in this software. */
        span_log(&s->logging, SPAN_LOG_ERROR, "ACK queue overflow\n");
        /* I guess push out the queued ACKs at this point is better than the alternatives */
        build_and_send_packet(s, channel, 0, NULL, 0);
    }
    /*endif*/
    entry = (channel << 14) | sequence_no;
    /* See if we have already queued a response for this sequence number. If the other end
       likes to send its packets in repeating bursts this may happen. */
    found = false;
    for (i = 0;  i < s->tx.ack_queue_ptr;  i++)
    {
        if (s->tx.ack_queue[i] == entry)
        {
            found = true;
            break;
        }
        /*endif*/
    }
    /*endfor*/
    if (!found)
    {
        s->tx.ack_queue[s->tx.ack_queue_ptr] = entry;
        s->tx.ack_queue_ptr++;
        if (s->tx.ack_queue_ptr == 1)
        {
            /* We now have something in the queue. We need to start the timer that will push out
               a partially filled acknowledgement queue if nothing else triggers transmission. */
            if (s->timer_handler)
                s->tx.ta01_timer = s->timer_handler(s->timer_user_data, ~0) + s->tx.ta01_timeout;
            /*endif*/
            span_log(&s->logging, SPAN_LOG_FLOW, "TA01 set to %lu\n", s->tx.ta01_timer);
            update_timer(s);
        }
        else if (s->tx.ack_queue_ptr >= 3)
        {
            /* The ACK queue is now full, so push an ACK only packet to clear it. */
            build_and_send_packet(s, channel, 0, NULL, 0);
        }
        /*endif*/
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static bool retransmit_the_unacknowledged(sprt_state_t *s, int channel, span_timestamp_t now)
{
    uint8_t first;
    sprt_chan_t *chan;
    bool something_was_sent;
    int diff;
    uint16_t seq_no;

    something_was_sent = false;
    if (channel >= SPRT_TCID_MIN_RELIABLE  &&  channel <= SPRT_TCID_MAX_RELIABLE)
    {
        chan = &s->tx.chan[channel];
        while ((first = chan->first_in_time) != TR03_QUEUE_FREE_SLOT_TAG
               &&
               chan->tr03_timer[first] <= now)
        {
            diff = chan->buff_in_ptr - first;
            if (diff < 0)
                diff += chan->window_size;
            /*endif*/
            seq_no = chan->queuing_sequence_no - diff;
            if (chan->buff_len[first] != SPRT_LEN_SLOT_FREE)
            {
                build_and_send_packet(s,
                                      channel,
                                      seq_no,
                                      &chan->buff[first*chan->max_payload_bytes],
                                      chan->buff_len[first]);
                something_was_sent = true;
            }
            else
            {
                span_log(&s->logging, SPAN_LOG_ERROR, "Empty slot scheduled %d %d\n", first, chan->buff_len[first]);
            }
            /*endif*/
            delete_timer_queue_entry(s, channel, first);
            chan->remaining_tries[first]--;
            if (chan->remaining_tries[first] <= 0)
            {
                /* TODO: take action on too many retries */
                if (s->status_handler)
                    s->status_handler(s->status_user_data, SPRT_STATUS_EXCESS_RETRIES);
                /*endif*/
            }
            else
            {
                /* Update the timestamp, and requeue the packet */
                chan->tr03_timer[first] += chan->tr03_timeout;
                add_timer_queue_last_entry(s, channel, first);
            }
            /*endif*/
        }
        /*endif*/
    }
    /*endif*/
    return something_was_sent;
}
/*- End of function --------------------------------------------------------*/

static void process_acknowledgements(sprt_state_t *s, int noa, int tcn[3], int sqn[3])
{
    int i;
    int slot;
    int ptr;
    int diff;
    int channel;
    sprt_chan_t *chan;

    /* Process the set of 1 to 3 acknowledgements from a received SPRT packet */
    if (noa > 0)
        span_log(&s->logging, SPAN_LOG_FLOW, "Received %d acknowledgements\n", noa);
    /*endif*/
    for (i = 0;  i < noa;  i++)
    {
        channel = tcn[i];
        span_log(&s->logging, SPAN_LOG_FLOW, "ACK received for channel %s, seq no %d\n", sprt_transmission_channel_to_str(tcn[i]), sqn[i]);
        chan = &s->tx.chan[channel];
        switch (channel)
        {
        case SPRT_TCID_RELIABLE_SEQUENCED:
        case SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED:
            diff = (chan->queuing_sequence_no - sqn[i]) & SPRT_SEQ_NO_MASK;
            if (diff < chan->window_size)
            {
                /* Find this sequence no in the buffer */
                slot = chan->buff_in_ptr - diff;
                if (slot < 0)
                    slot += chan->window_size;
                /*endif*/
                if (chan->buff_len[slot] != SPRT_LEN_SLOT_FREE)
                {
                    /* This packet is no longer needed. We can clear the buffer slot. */
                    span_log(&s->logging, SPAN_LOG_FLOW, "Slot OK %d/%d contains %d [%d, %d]\n", channel, slot, sqn[i], chan->queuing_sequence_no, chan->buff_in_ptr);
                    chan->buff_len[slot] = SPRT_LEN_SLOT_FREE;
                    /* TODO: We are deleting the resend timer here, without updating the next timeout. This
                       should be harmless. However, the spurious timeouts it may result in seems messy. */
                    chan->tr03_timer[slot] = 0;
                    span_log(&s->logging, SPAN_LOG_FLOW, "TR03(%d)[%d] cancelled\n", channel, slot);
                    delete_timer_queue_entry(s, channel, slot);
                    ptr = chan->buff_acked_out_ptr;
                    if (slot == ptr)
                    {
                        /* This is the next packet in sequence to be delivered. So, we can now drop it, and
                           anything following which may have already been ACKed, until we reach something
                           which has not been ACKed, or we have emptied the buffer. */
                        do
                        {
                            if (++ptr >= chan->window_size)
                                ptr = 0;
                            /*endif*/
                        }
                        while (ptr != chan->buff_in_ptr  &&  chan->buff_len[ptr] == SPRT_LEN_SLOT_FREE);
                        chan->buff_acked_out_ptr = ptr;
                    }
                    /*endif*/
                }
                else
                {
                    /* This slot might be free, because we received an ACK already (e.g. if we got a late ACK
                       after sending a retransmission, and now we have the ACK from the retransmission). This
                       can be ignored.
                       The slot might have a new sequence number in it, and we are getting a late ACK for the
                       sequence number it contained before. It should be best to ignore this too. */
                    span_log(&s->logging, SPAN_LOG_FLOW, "Slot BAD %d/%d does not contain %d [%d, %d]\n", channel, slot, sqn[i], chan->queuing_sequence_no, chan->buff_in_ptr);
                }
                /*endif*/
            }
            else
            {
                /* This slot might be free, because we received an ACK already (e.g. if we got a late ACK
                   after sending a retransmission, and now we have the ACK from the retransmission). This
                   can be ignored.
                   The slot might have a new sequence number in it, and we are getting a late ACK for the
                   sequence number it contained before. It should be best to ignore this too. */
                span_log(&s->logging, SPAN_LOG_FLOW, "Slot BAD %d This is an ack for something outside the current window - %d %d\n", channel, chan->queuing_sequence_no, sqn[i]);
            }
            /*endif*/
            break;
        case SPRT_TCID_UNRELIABLE_UNSEQUENCED:
        case SPRT_TCID_UNRELIABLE_SEQUENCED:
            /* Getting here means we have an acknowledgement for an unreliable packet. This should never happen. The received packet has a problem. */
            span_log(&s->logging,
                     SPAN_LOG_FLOW,
                     "Acknowledgement received for unreliable channel %s\n",
                     sprt_transmission_channel_to_str(channel));
            break;
        }
        /*endswitch*/
    }
    /*endfor*/
}
/*- End of function --------------------------------------------------------*/

static int sprt_deliver(sprt_state_t *s)
{
    int channel;
    int iptr;
    sprt_chan_t *chan;

    for (channel = SPRT_TCID_MIN_RELIABLE;  channel <= SPRT_TCID_MAX_RELIABLE;  channel++)
    {
        chan = &s->rx.chan[channel];
        iptr = chan->buff_in_ptr;
        while (chan->buff_len[iptr] != SPRT_LEN_SLOT_FREE)
        {
            /* We need to check for busy before delivering each packet, in case the app applied
               flow control between packets. */
            if (chan->busy)
                break;
            /*endif*/
            /* Deliver the body of the message */
            if (s->rx_delivery_handler)
                s->rx_delivery_handler(s->rx_user_data, channel, chan->base_sequence_no, &chan->buff[iptr*chan->max_payload_bytes], chan->buff_len[iptr]);
            /*endif*/
            chan->base_sequence_no = (chan->base_sequence_no + 1) & SPRT_SEQ_NO_MASK;
            chan->buff_len[iptr] = SPRT_LEN_SLOT_FREE;
            if (++iptr >= chan->window_size)
                iptr = 0;
            /*endif*/
        }
        /*endwhile*/
        /* Only change the pointer now we have really finished. */
        chan->buff_in_ptr = iptr;
    }
    /*endfor*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) sprt_timer_expired(sprt_state_t *s, span_timestamp_t now)
{
    int i;
    bool something_was_sent_for_channel;
    bool something_was_sent;

    span_log(&s->logging, SPAN_LOG_FLOW, "Timer expired at %lu\n", now);

    if (now < s->latest_timer)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Timer returned %luus early\n", s->latest_timer - now);
        /* Request the same timeout point again. */
        if (s->timer_handler)
            s->timer_handler(s->timer_user_data, s->latest_timer);
        /*endif*/
        return 0;
    }
    /*endif*/

    something_was_sent = false;

    if (s->tx.immediate_timer)
    {
        s->tx.immediate_timer = false;
        sprt_deliver(s);
    }
    /*endif*/

    for (i = SPRT_TCID_MIN_RELIABLE;  i <= SPRT_TCID_MAX_RELIABLE;  i++)
    {
        something_was_sent_for_channel = retransmit_the_unacknowledged(s, i, now);
        /* There's a keepalive timer for each reliable channel. We only need to send a keepalive if we
           didn't just send a retransmit for this channel. */
        if (s->tx.chan[i].ta02_timer != 0)
        {
            if (s->tx.chan[i].ta02_timer <= now  &&  !something_was_sent_for_channel)
            {
                /* Send a keepalive packet for this channel. */
                span_log(&s->logging, SPAN_LOG_FLOW, "Keepalive only packet sent\n");
                build_and_send_packet(s, i, 0, NULL, 0);
                something_was_sent_for_channel = true;
            }
            /*endif*/
            if (something_was_sent_for_channel)
            {
                s->tx.chan[i].ta02_timer = now + s->tx.chan[i].ta02_timeout;
                span_log(&s->logging, SPAN_LOG_FLOW, "TA02(%d) set to %lu\n", i, s->tx.chan[i].ta02_timer);
            }
            /*endif*/
        }
        /*endif*/
        if (something_was_sent_for_channel)
            something_was_sent = true;
        /*endif*/
    }
    /*endfor*/

    /* There's a single ACK holdoff timer, which applies to all channels. */
    /* We only need to push ACKs if we haven't yet pushed out a packet for any channel during this
       timer expired processing. */
    if (!something_was_sent  &&  s->tx.ta01_timer != 0  &&  s->tx.ta01_timer <= now)
    {
        /* Push any outstanding ACKs and we are done. We don't need to start a new timing operation. */
        if (s->tx.ack_queue_ptr > 0)
        {
            /* Push an ACK only packet */
            span_log(&s->logging, SPAN_LOG_FLOW, "ACK only packet sent\n");
            build_and_send_packet(s, SPRT_TCID_UNRELIABLE_UNSEQUENCED, 0, NULL, 0);
            something_was_sent = true;
        }
        /*endif*/
    }
    /*endif*/
    update_timer(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void sprt_rx_reinit(sprt_state_t *s)
{
    /* TODO */
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) sprt_rx_packet(sprt_state_t *s, const uint8_t pkt[], int len)
{
    int i;
    int header_extension_bit;
    int reserved_bit;
    uint8_t subsession_id;
    uint8_t payload_type;
    int channel;
    uint16_t base_sequence_no;
    uint16_t sequence_no;
    int noa;
    int tcn[3];
    int sqn[3];
    int header_len;
    int payload_len;
    int iptr;
    int diff;
    sprt_chan_t *chan;

    span_log_buf(&s->logging, SPAN_LOG_FLOW, "Rx", pkt, len);
    /* An SPRT packet has 3 essential components: A base sequence number, some ACKs and a payload.
       - A packet with no ACKs or payload is a keepalive. Its there to report the continued existance
         of the far end, and to report the far end's base sequence number for a reliable channel.
       - A packet with ACKs and no payload performs the above, and also ACKs one or more reliable
         packets in the other direction.
       - A packet with a payload does all of the above, with some data as well. There might be zero
         things to ACK. */
    if (len < 6)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Rx packet too short\n");
        return -1;
    }
    /*endif*/
    header_extension_bit = (pkt[0] >> 7) & 1;
    reserved_bit = (pkt[1] >> 7) & 1;
    subsession_id = pkt[0] & 0x7F;
    payload_type = pkt[1] & 0x7F;

    if (header_extension_bit != 0  ||  reserved_bit != 0)
    {
        /* This doesn't look like an SPRT packet */
        span_log(&s->logging, SPAN_LOG_FLOW, "Rx packet header does not look like SPRT\n");
        return -1;
    }
    /*endif*/
    if (payload_type != s->rx.payload_type)
    {
        /* This is not the payload type we are looking for */
        span_log(&s->logging, SPAN_LOG_FLOW, "Rx payload type %d, expected %d\n", payload_type, s->rx.payload_type);
        return -1;
    }
    /*endif*/
    if (s->rx.subsession_id == 0xFF)
    {
        /* This is the first subsession ID we have seen, so accept it going forwards as the
           subsession ID to be expected for future packets. The spec says the IDs start at zero,
           so if both sides started up together the subsession ID on both sides should probably be
           in sync, but is this guaranteed? Should the subsession ID we send match the one we
           receive? */
        s->rx.subsession_id = subsession_id;
    }
    else
    {
        if (subsession_id != s->rx.subsession_id)
        {
            /* This doesn't look good. We have a new subsession ID. The payload type field check out
               OK. What other integrity checks can we make, to check we are seeing sane packets from
               a new subsession ID, rather than garbage? */
            span_log(&s->logging, SPAN_LOG_FLOW, "Rx subsession ID %d, expected %d\n", subsession_id, s->rx.subsession_id);
            if (s->status_handler)
                s->status_handler(s->status_user_data, SPRT_STATUS_SUBSESSION_CHANGED);
            /*endif*/
            sprt_rx_reinit(s);
            return -1;
        }
        /*endif*/
    }
    /*endif*/
    /* The packet's framework looks OK, so let's process its contents */
    channel = (pkt[2] >> 6) & 3;
    sequence_no = get_net_unaligned_uint16(&pkt[2]) & SPRT_SEQ_NO_MASK;
    noa = (pkt[4] >> 6) & 3;
    chan = &s->rx.chan[channel];

    /* Deal with the keepalive and base sequence no reporting aspects of the packet */
    base_sequence_no = get_net_unaligned_uint16(&pkt[4]) & SPRT_SEQ_NO_MASK;
    if (s->tx.chan[channel].busy)
    {
        if (s->tx.chan[channel].base_sequence_no != base_sequence_no)
            span_log(&s->logging, SPAN_LOG_FLOW, "BSN for channel %d changed from %u to %u\n", channel, s->tx.chan[channel].base_sequence_no, base_sequence_no);
        /*endif*/
    }
    /*endif*/
    s->tx.chan[channel].base_sequence_no = base_sequence_no;
    /* TODO: record the time the channel was last seen. */

    /* Deal with the ACKs that might be present in the packet */
    header_len = 6;
    if (noa > 0)
    {
        /* There are some ACKs to process. */
        if (len < 6 + 2*noa)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Rx packet too short\n");
            return -1;
        }
        /*endif*/
        for (i = 0;  i < noa;  i++)
        {
            tcn[i] = (pkt[header_len] >> 6) & 3;
            sqn[i] = get_net_unaligned_uint16(&pkt[header_len]) & SPRT_SEQ_NO_MASK;
            header_len += 2;
        }
        /*endfor*/
        process_acknowledgements(s, noa, tcn, sqn);
    }
    /*endif*/
    payload_len = len - header_len;
    span_log(&s->logging, SPAN_LOG_FLOW, "Rx ch %d seq %d noa %d len %d\n", channel, sequence_no, noa, payload_len);
    /* Deal with the payload, if any, in the packet */
    /* V.150.1 says SPRT_TCID_UNRELIABLE_UNSEQUENCED should be used for ACK only packets, but in the real
       world you should expect any of the transport channel IDs. These ACK only packets have the sequence
       number set to zero, regardless of where the sequence number for that channel currently stands.
       (figure B.3/V.150.1) */
    if (payload_len > 0)
    {
        /* There is a payload to process */
        if (payload_len > chan->max_payload_bytes)
        {
            span_log(&s->logging, SPAN_LOG_ERROR, "Payload too long %d (%d)\n", payload_len, chan->max_payload_bytes);
        }
        else
        {
            switch (channel)
            {
            case SPRT_TCID_RELIABLE_SEQUENCED:
                /* Used for data */
            case SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED:
                /* Used for control/signalling data */
                if (sequence_no == chan->base_sequence_no)
                {
                    iptr = chan->buff_in_ptr;
                    queue_acknowledgement(s, channel, sequence_no);
                    if (chan->busy)
                    {
                        /* We can't deliver this right now, so we need to store it at the head of the buffer */
                        memcpy(&chan->buff[iptr*chan->max_payload_bytes], pkt + header_len, payload_len);
                        chan->buff_len[iptr] = payload_len;
                    }
                    else
                    {
                        /* This is exactly the next packet in sequence, so deliver it. */
                        if (s->rx_delivery_handler)
                            s->rx_delivery_handler(s->rx_user_data, channel, sequence_no, pkt + header_len, payload_len);
                        /*endif*/
                        chan->base_sequence_no = (chan->base_sequence_no + 1) & SPRT_SEQ_NO_MASK;
                        chan->buff_len[iptr] = SPRT_LEN_SLOT_FREE;
                        if (++iptr >= chan->window_size)
                            iptr = 0;
                        /*endif*/
                        /* See if there are any contiguously following packets in the buffer, which can be delivered immediately. */
                        while (chan->buff_len[iptr] != SPRT_LEN_SLOT_FREE)
                        {
                            /* We need to check for busy before delivering each packet, in case the app applied
                               flow control between packets. */
                            if (chan->busy)
                                break;
                            /*endif*/
                            /* Deliver the body of the message */
                            if (s->rx_delivery_handler)
                                s->rx_delivery_handler(s->rx_user_data, channel, chan->base_sequence_no, &chan->buff[iptr*chan->max_payload_bytes], chan->buff_len[iptr]);
                            /*endif*/
                            chan->base_sequence_no = (chan->base_sequence_no + 1) & SPRT_SEQ_NO_MASK;
                            chan->buff_len[iptr] = SPRT_LEN_SLOT_FREE;
                            if (++iptr >= chan->window_size)
                                iptr = 0;
                            /*endif*/
                        }
                        /*endwhile*/
                        /* Only change the pointer now we have really finished. */
                        chan->buff_in_ptr = iptr;
                    }
                    /*endif*/
                }
                else
                {
                    /* This packet is out of sequence, so there may have been some packets lost somewhere. If the
                       packet is older than the last delivered one it must be a repeat. If its beyond the last
                       delievered packet it might be inside or outside the window. We store it if its within the
                       window, so we can deliver it later, when we have the missing intermediate packets. If its
                       later than the window we have to drop it, as we have nowhere to store it. */
                    /* TODO: we probably shouldn't ACK a packet we drop because its beyond the window. */
                    diff = (sequence_no - chan->base_sequence_no) & SPRT_SEQ_NO_MASK;
                    if (diff < chan->window_size)
                    {
                        queue_acknowledgement(s, channel, sequence_no);
                        iptr = chan->buff_in_ptr + diff;
                        if (iptr >= chan->window_size)
                            iptr -= chan->window_size;
                        /*endif*/
                        memcpy(&chan->buff[iptr*chan->max_payload_bytes], pkt + header_len, payload_len);
                        chan->buff_len[iptr] = payload_len;
                    }
                    else if (diff > 2*SPRT_MAX_WINDOWS_SIZE)
                    {
                        /* This is an older packet, or something far in the future. We should acknowledge it, as 
                           its probably a repeat for a packet where the far end missed the previous ACK we sent. */
                        queue_acknowledgement(s, channel, sequence_no);
                        if (s->status_handler)
                            s->status_handler(s->status_user_data, SPRT_STATUS_OUT_OF_SEQUENCE);
                        /*endif*/
                    }
                    else
                    {
                        /* This is a little too far into the future of packets (i.e. just beyond the window).
                           We should not acknowledge it, as the far end will think we have delivered the packet. */
                    }
                    /*endif*/
                }
                /*endif*/
                chan->active = true;
                break;
            case SPRT_TCID_UNRELIABLE_UNSEQUENCED:
                /* Used for ack only */
                /* The payload length should always be zero, although it isn't if we are here. Is this
                   erroneous? Its not quite clear from the spec. */
            case SPRT_TCID_UNRELIABLE_SEQUENCED:
                /* Used for sequenced data that does not require reliable delivery */
                /* We might have missed one or more packets, so this may or may not be the next packet in sequence. We have
                   no way to fix this, so just deliver the payload. */
                /* TODO: This might be a repeat of the last packet, if the sender tries to achieve redundancy by multiple sends.
                   should try to avoid delivering the same packet multiple times. */
                /* Deliver the payload of the message */
                if (s->rx_delivery_handler)
                    s->rx_delivery_handler(s->rx_user_data, channel, sequence_no, pkt + header_len, payload_len);
                /*endif*/
                chan->active = true;
                break;
            }
            /*endswitch*/
        }
        /*endif*/
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) sprt_tx(sprt_state_t *s, int channel, const uint8_t payload[], int len)
{
    int real_len;
    int iptr;
    int optr;
    uint16_t seq_no;
    sprt_chan_t *chan;

    if (channel < SPRT_TCID_MIN  ||  channel > SPRT_TCID_MAX)
        return -1;
    /*endif*/
    chan = &s->tx.chan[channel];
    /* Is the length in range for this particular channel? */
    if (len <= 0  ||  len > chan->max_payload_bytes)
        return -1;
    /*endif*/
    switch (channel)
    {
    case SPRT_TCID_RELIABLE_SEQUENCED:
    case SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED:
        /* We need to queue this message, and set the retry timer for it, so we can handle ACKs and retransmissions. We also need to send it now. */
        /* Snapshot the values (although only optr should be changeable during this processing) */
        iptr = chan->buff_in_ptr;
        optr = chan->buff_acked_out_ptr;
        if ((real_len = optr - iptr - 1) < 0)
            real_len += chan->window_size;
        /*endif*/
        if (real_len < 1)
        {
            /* Queue full */
            return -1;
        }
        /*endif*/
        memcpy(&chan->buff[iptr*chan->max_payload_bytes], payload, len);
        chan->buff_len[iptr] = len;
        seq_no = chan->queuing_sequence_no;
        chan->queuing_sequence_no = (chan->queuing_sequence_no + 1) & SPRT_SEQ_NO_MASK;
        if (s->timer_handler)
            chan->tr03_timer[iptr] = s->timer_handler(s->timer_user_data, ~0) + chan->tr03_timeout;
        /*endif*/
        span_log(&s->logging, SPAN_LOG_FLOW, "TR03(%d)[%d] set to %lu\n", channel, iptr, chan->tr03_timer[iptr]);
        chan->remaining_tries[iptr] = chan->max_tries;
        add_timer_queue_last_entry(s, channel, iptr);
        if (++iptr >= chan->window_size)
            iptr = 0;
        /*endif*/
        /* Only change the pointer now we have really finished. */
        chan->buff_in_ptr = iptr;
        /* If this is the first activity on this channel, we get the TA02 timer started for
           this channel. If the channel is already active we will adjust the timout. */
        if (s->timer_handler)
            chan->ta02_timer = s->timer_handler(s->timer_user_data, ~0) + chan->ta02_timeout;
        /*endif*/
        span_log(&s->logging, SPAN_LOG_FLOW, "TA02(%d) set to %lu\n", channel, chan->ta02_timer);
        /* Now send the first copy */
        build_and_send_packet(s, channel, seq_no, payload, len);
        break;
    case SPRT_TCID_UNRELIABLE_UNSEQUENCED:
        /* It is not clear from the spec if this channel should ever carry data. Table B.1 says
           the channel is "Used for acknowledgements only", and yet Table B.2 defines a parameter
           SPRT_TC0_PAYLOAD_BYTES which is non-zero. */
        /* There is no reason to buffer this. Send it straight out. */
        build_and_send_packet(s, channel, 0, payload, len);
        break;
    case SPRT_TCID_UNRELIABLE_SEQUENCED:
        /* There is no reason to buffer this. Send it straight out. */
        build_and_send_packet(s, channel, chan->queuing_sequence_no, payload, len);
        chan->queuing_sequence_no = (chan->queuing_sequence_no + 1) & SPRT_SEQ_NO_MASK;
        break;
    }
    /*endswitch*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) sprt_set_local_tc_windows_size(sprt_state_t *s, int channel, int size)
{
    if (channel < SPRT_TCID_MIN_RELIABLE  ||  channel > SPRT_TCID_MAX_RELIABLE)
        return -1;
    /*endif*/
    if (size < channel_parm_limits[channel].min_window_size
        ||
        size > channel_parm_limits[channel].max_window_size)
    {
        return -1;
    }
    /*endif*/
    s->rx.chan[channel].window_size = size;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) sprt_get_local_tc_windows_size(sprt_state_t *s, int channel)
{
    if (channel < SPRT_TCID_MIN_RELIABLE  ||  channel > SPRT_TCID_MAX_RELIABLE)
        return -1;
    /*endif*/
    return s->rx.chan[channel].window_size;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) sprt_set_local_tc_payload_bytes(sprt_state_t *s, int channel, int max_len)
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
    s->rx.chan[channel].max_payload_bytes = max_len;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) sprt_get_local_tc_payload_bytes(sprt_state_t *s, int channel)
{
    if (channel < SPRT_TCID_MIN  ||  channel > SPRT_TCID_MAX)
        return -1;
    /*endif*/
    return s->rx.chan[channel].max_payload_bytes;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) sprt_set_local_tc_max_tries(sprt_state_t *s, int channel, int max_tries)
{
    if (channel < SPRT_TCID_MIN_RELIABLE  ||  channel > SPRT_TCID_MAX_RELIABLE)
        return -1;
    /*endif*/
    if (max_tries < SPRT_MIN_MAX_TRIES
        ||
        max_tries > SPRT_MAX_MAX_TRIES)
    {
        return -1;
    }
    /*endif*/
    s->tx.chan[channel].max_tries = max_tries;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) sprt_get_local_tc_max_tries(sprt_state_t *s, int channel)
{
    if (channel < SPRT_TCID_MIN_RELIABLE  ||  channel > SPRT_TCID_MAX_RELIABLE)
        return -1;
    /*endif*/
    return s->tx.chan[channel].max_tries;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) sprt_set_far_tc_windows_size(sprt_state_t *s, int channel, int size)
{
    if (channel < SPRT_TCID_MIN_RELIABLE  ||  channel > SPRT_TCID_MAX_RELIABLE)
        return -1;
    /*endif*/
    if (size < channel_parm_limits[channel].min_window_size
        ||
        size > channel_parm_limits[channel].max_window_size)
    {
        return -1;
    }
    /*endif*/
    s->tx.chan[channel].window_size = size;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) sprt_get_far_tc_windows_size(sprt_state_t *s, int channel)
{
    if (channel < SPRT_TCID_MIN_RELIABLE  ||  channel > SPRT_TCID_MAX_RELIABLE)
        return -1;
    /*endif*/
    return s->tx.chan[channel].window_size;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) sprt_set_far_tc_payload_bytes(sprt_state_t *s, int channel, int max_len)
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
    s->tx.chan[channel].max_payload_bytes = max_len;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) sprt_get_far_tc_payload_bytes(sprt_state_t *s, int channel)
{
    if (channel < SPRT_TCID_MIN  ||  channel > SPRT_TCID_MAX)
        return -1;
    /*endif*/
    return s->tx.chan[channel].max_payload_bytes;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) sprt_set_tc_timeout(sprt_state_t *s, int channel, int timer, int timeout)
{
    switch (timer)
    {
    case SPRT_TIMER_TA01:
        if (channel < SPRT_TCID_MIN  ||  channel > SPRT_TCID_MAX)
            return -1;
        /*endif*/
        s->tx.ta01_timeout = timeout;
        break;
    case SPRT_TIMER_TA02:
        if (channel < SPRT_TCID_MIN_RELIABLE  ||  channel > SPRT_TCID_MAX_RELIABLE)
            return -1;
        /*endif*/
        s->tx.chan[channel].ta02_timeout = timeout;
        break;
    case SPRT_TIMER_TR03:
        if (channel < SPRT_TCID_MIN_RELIABLE  ||  channel > SPRT_TCID_MAX_RELIABLE)
            return -1;
        /*endif*/
        s->tx.chan[channel].tr03_timeout = timeout;
        break;
    default:
        return -1;
    }
    /*endswitch*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) sprt_get_tc_timeout(sprt_state_t *s, int channel, int timer)
{
    int timeout;

    switch (timer)
    {
    case SPRT_TIMER_TA01:
        if (channel < SPRT_TCID_MIN  ||  channel > SPRT_TCID_MAX)
            return -1;
        /*endif*/
        timeout = s->tx.ta01_timeout;
        break;
    case SPRT_TIMER_TA02:
        if (channel < SPRT_TCID_MIN_RELIABLE  ||  channel > SPRT_TCID_MAX_RELIABLE)
            return -1;
        /*endif*/
        timeout = s->tx.chan[channel].ta02_timeout;
        break;
    case SPRT_TIMER_TR03:
        if (channel < SPRT_TCID_MIN_RELIABLE  ||  channel > SPRT_TCID_MAX_RELIABLE)
            return -1;
        /*endif*/
        timeout = s->tx.chan[channel].tr03_timeout;
        break;
    default:
        return -1;
    }
    /*endswitch*/
    return timeout;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) sprt_set_local_busy(sprt_state_t *s, int channel, bool busy)
{
    bool previous_busy;

    previous_busy = false;
    if (channel >= SPRT_TCID_MIN_RELIABLE  &&  channel <= SPRT_TCID_MAX_RELIABLE)
    {
        previous_busy = s->rx.chan[channel].busy;
        s->rx.chan[channel].busy = busy;
        /* We may want to schedule an immediate callback to push out some packets
           which are ready for delivery, if we are removing the busy condition. */
        if (previous_busy  &&  !busy)
        {
            s->tx.immediate_timer = true;
            update_timer(s);
        }
        /*endif*/
    }
    /*endif*/
    return previous_busy;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(bool) sprt_get_far_busy_status(sprt_state_t *s, int channel)
{
    return s->tx.chan[channel].busy;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) sprt_get_logging_state(sprt_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(sprt_state_t *) sprt_init(sprt_state_t *s,
                                       uint8_t subsession_id,
                                       uint8_t rx_payload_type,
                                       uint8_t tx_payload_type,
                                       channel_parms_t parms[SPRT_CHANNELS],
                                       sprt_tx_packet_handler_t tx_packet_handler,
                                       void *tx_user_data,
                                       sprt_rx_delivery_handler_t rx_delivery_handler,
                                       void *rx_user_data,
                                       sprt_timer_handler_t timer_handler,
                                       void *timer_user_data,
                                       span_modem_status_func_t status_handler,
                                       void *status_user_data)
{
    int i;
    int j;

    if (rx_delivery_handler == NULL  ||  tx_packet_handler == NULL  ||  timer_handler == NULL  ||  status_handler == NULL)
        return NULL;
    /*endif*/
    if (parms == NULL)
    {
        parms = default_channel_parms;
    }
    else
    {
        for (i = SPRT_TCID_MIN;  i <= SPRT_TCID_MAX;  i++)
        {
            if (parms[i].payload_bytes < channel_parm_limits[i].min_payload_bytes
                ||
                parms[i].payload_bytes > channel_parm_limits[i].max_payload_bytes)
            {
                return NULL;
            }
            /*endif*/
            if (parms[i].window_size < channel_parm_limits[i].min_window_size
                ||
                parms[i].window_size > channel_parm_limits[i].max_window_size)
            {
                return NULL;
            }
            /*endif*/
        }
        /*endfor*/
    }
    /*endif*/
    if (s == NULL)
    {
        if ((s = (sprt_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
        /*endif*/
    }
    /*endif*/
    memset(s, 0, sizeof(*s));

    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "SPRT");

    /* Set up all the pointers to buffers */
    s->tx.chan[SPRT_TCID_RELIABLE_SEQUENCED].buff = s->tc1_tx_buff;
    s->tx.chan[SPRT_TCID_RELIABLE_SEQUENCED].buff_len = s->tc1_tx_buff_len;
    s->tx.chan[SPRT_TCID_RELIABLE_SEQUENCED].tr03_timer = s->tc1_tx_tr03_timer;
    s->tx.chan[SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED].buff = s->tc2_tx_buff;
    s->tx.chan[SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED].buff_len = s->tc2_tx_buff_len;
    s->tx.chan[SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED].tr03_timer = s->tc2_tx_tr03_timer;

    s->rx.chan[SPRT_TCID_RELIABLE_SEQUENCED].buff = s->tc1_rx_buff;
    s->rx.chan[SPRT_TCID_RELIABLE_SEQUENCED].buff_len = s->tc1_rx_buff_len;
    s->rx.chan[SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED].buff = s->tc2_rx_buff;
    s->rx.chan[SPRT_TCID_EXPEDITED_RELIABLE_SEQUENCED].buff_len = s->tc2_rx_buff_len;

    s->rx.subsession_id = 0xFF;
    s->tx.subsession_id = subsession_id;
    s->rx.payload_type = rx_payload_type;
    s->tx.payload_type = tx_payload_type;

    s->tx.ta01_timeout = default_channel_parms[SPRT_TCID_RELIABLE_SEQUENCED].timer_ta01;
    for (i = SPRT_TCID_MIN;  i <= SPRT_TCID_MAX;  i++)
    {
        s->rx.chan[i].max_payload_bytes = default_channel_parms[i].payload_bytes;
        s->rx.chan[i].window_size = default_channel_parms[i].window_size;
        s->rx.chan[i].ta02_timeout = default_channel_parms[i].timer_ta02;
        s->rx.chan[i].tr03_timeout = default_channel_parms[i].timer_tr03;

        s->tx.chan[i].max_payload_bytes = default_channel_parms[i].payload_bytes;
        s->tx.chan[i].window_size = default_channel_parms[i].window_size;
        s->tx.chan[i].ta02_timeout = default_channel_parms[i].timer_ta02;
        s->tx.chan[i].tr03_timeout = default_channel_parms[i].timer_tr03;

        s->tx.chan[i].max_tries = SPRT_DEFAULT_MAX_TRIES;

        s->rx.chan[i].base_sequence_no = 0;
    }
    /*endfor*/

    for (i = SPRT_TCID_MIN_RELIABLE;  i <= SPRT_TCID_MAX_RELIABLE;  i++)
    {
        /* Initialise the sorted TR03 timeout queues */
        s->tx.chan[i].first_in_time = TR03_QUEUE_FREE_SLOT_TAG;
        s->tx.chan[i].last_in_time = TR03_QUEUE_FREE_SLOT_TAG;

        for (j = 0;  j < channel_parm_limits[i].max_window_size;  j++)
        {
            s->rx.chan[i].buff_len[j] = SPRT_LEN_SLOT_FREE;
            s->tx.chan[i].buff_len[j] = SPRT_LEN_SLOT_FREE;
            s->tx.chan[i].prev_in_time[j] = TR03_QUEUE_FREE_SLOT_TAG;
            s->tx.chan[i].next_in_time[j] = TR03_QUEUE_FREE_SLOT_TAG;
        }
        /*endfor*/
    }
    /*endfor*/

    s->rx_delivery_handler = rx_delivery_handler;
    s->tx_user_data = tx_user_data;
    s->tx_packet_handler = tx_packet_handler;
    s->rx_user_data = rx_user_data;
    s->timer_handler = timer_handler;
    s->timer_user_data = timer_user_data;
    s->status_handler = status_handler;
    s->status_user_data = status_user_data;

    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) sprt_release(sprt_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) sprt_free(sprt_state_t *s)
{
    int ret;

    ret = sprt_release(s);
    span_free(s);
    return ret;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
