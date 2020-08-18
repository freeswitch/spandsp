/*
 * SpanDSP - a series of DSP components for telephony
 *
 * playout.h
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2005 Steve Underwood
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

#if !defined(_SPANDSP_PLAYOUT_H_)
#define _SPANDSP_PLAYOUT_H_

/*! \page playout_page Play-out (jitter buffering)
\section playout_page_sec_1 What does it do?
The play-out module provides a static or dynamic length buffer for received frames of
audio or video data. It's goal is to maximise the receiver's tolerance of jitter in the
timing of the received frames.

Dynamic buffers are generally good for speech, since they adapt to provide the smallest delay
consistent with a low rate of packets arriving too late to be used. For things like FoIP and
MoIP, a static length of buffer is normally necessary. Any attempt to elastically change the
buffer length would wreck a modem's data flow.
*/

/* Return codes */
enum
{
    PLAYOUT_OK = 0,
    PLAYOUT_ERROR,
    PLAYOUT_EMPTY,
    PLAYOUT_NOFRAME,
    PLAYOUT_FILLIN,
    PLAYOUT_DROP
};

/* Frame types */
#define PLAYOUT_TYPE_CONTROL    0
#define PLAYOUT_TYPE_SILENCE    1
#define PLAYOUT_TYPE_SPEECH     2

typedef int timestamp_t;

typedef struct playout_frame_s playout_frame_t;

/*!
    Playout (jitter buffer) descriptor. This defines the working state
    for a single instance of playout buffering.
*/
typedef struct playout_state_s playout_state_t;

#if defined(__cplusplus)
extern "C"
{
#endif

/*! Queue a frame
    \param s The play-out context.
    \param data The frame data.
    \param sender_len Length of frame (for voice) in timestamp units.
    \param sender_stamp Sending end's time stamp.
    \param receiver_stamp Local time at which packet was received, in timestamp units.
    \return One of
        PLAYOUT_OK:  Frame queued OK.
        PLAYOUT_ERROR: Some problem occured - e.g. out of memory. */
SPAN_DECLARE(int) playout_put(playout_state_t *s, void *data, int type, timestamp_t sender_len, timestamp_t sender_stamp, timestamp_t receiver_stamp);

/*! Get the next frame.
    \param s The play-out context.
    \param frame The frame.
    \param sender_stamp The sender's timestamp.
    \return One of
        PLAYOUT_OK:  Suitable frame found.
        PLAYOUT_DROP: A frame which should be dropped was found (e.g. it arrived too late).
                      The caller should request the same time again when this occurs.
        PLAYOUT_NOFRAME: There's no frame scheduled for this time.
        PLAYOUT_FILLIN: Synthetic signal must be generated, as no real data is available for
                        this time (either we need to grow, or there was a lost frame).
        PLAYOUT_EMPTY: The buffer is empty.
 */
SPAN_DECLARE(int) playout_get(playout_state_t *s, playout_frame_t *frame, timestamp_t sender_stamp);

/*! Unconditionally get the first buffered frame. This may be used to clear out the queue, and free
    all its contents, before the context is freed.
    \param s The play-out context.
    \return The frame, or NULL is the queue is empty. */
SPAN_DECLARE(playout_frame_t *) playout_get_unconditional(playout_state_t *s);

/*! Find the current length of the buffer.
    \param s The play-out context.
    \return The length of the buffer. */
SPAN_DECLARE(timestamp_t) playout_current_length(playout_state_t *s);

/*! Find the time at which the next queued frame is due to play.
    Note: This value may change backwards as freshly received out of order frames are
          added to the buffer.
    \param s The play-out context.
    \return The next timestamp. */
SPAN_DECLARE(timestamp_t) playout_next_due(playout_state_t *s);

/*! Reset an instance of play-out buffering.
    NOTE:  The buffer should be empty before you call this function, otherwise
           you will leak queued frames, and some internal structures
    \param s The play-out context.
    \param min_length Minimum length of the buffer, in samples.
    \param max_length Maximum length of the buffer, in samples. If this equals min_length, static
           length buffering is used. */
SPAN_DECLARE(void) playout_restart(playout_state_t *s, int min_length, int max_length);

/*! Create a new instance of play-out buffering.
    \param min_length Minimum length of the buffer, in samples.
    \param max_length Maximum length of the buffer, in samples. If this equals min_length, static
           length buffering is used.
    \return The new context */
SPAN_DECLARE(playout_state_t *) playout_init(int min_length, int max_length);

/*! Release an instance of play-out buffering.
    \param s The play-out context to be releaased
    \return 0 if OK, else -1 */
SPAN_DECLARE(int) playout_release(playout_state_t *s);

/*! Free an instance of play-out buffering.
    \param s The play-out context to be destroyed
    \return 0 if OK, else -1 */
SPAN_DECLARE(int) playout_free(playout_state_t *s);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
