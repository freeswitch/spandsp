/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v150_1_local.h - V.150.1
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2023 Steve Underwood
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

int sse_status_handler(v150_1_state_t *s, int status);

span_timestamp_t update_sse_timer(void *user_data, span_timestamp_t timeout);

int v150_1_sse_timer_expired(v150_1_state_t *s, span_timestamp_t now);

void v150_1_sse_init(v150_1_state_t *s,
                     v150_1_sse_tx_packet_handler_t tx_packet_handler,
                     void *tx_packet_user_data);

/*- End of file ------------------------------------------------------------*/
