/*
 * SpanDSP - a series of DSP components for telephony
 *
 * expose.h - Expose the internal structures of spandsp, for users who
 *            really need that.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2008 Steve Underwood
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

/* TRY TO ONLY INCLUDE THIS IF YOU REALLY REALLY HAVE TO */

#if !defined(_SPANDSP_EXPOSE_H_)
#define _SPANDSP_EXPOSE_H_

#include <jpeglib.h>

#include <spandsp3/private/logging.h>
#include <spandsp3/private/schedule.h>
#include <spandsp3/private/bitstream.h>
#include <spandsp3/private/queue.h>
#include <spandsp3/private/awgn.h>
#include <spandsp3/private/noise.h>
#include <spandsp3/private/bert.h>
#include <spandsp3/private/power_meter.h>
#include <spandsp3/private/tone_generate.h>
#include <spandsp3/private/bell_r2_mf.h>
#include <spandsp3/private/sig_tone.h>
#include <spandsp3/private/dtmf.h>
#include <spandsp3/private/g711.h>
#include <spandsp3/private/g722.h>
#include <spandsp3/private/g726.h>
#include <spandsp3/private/lpc10.h>
#include <spandsp3/private/gsm0610.h>
#include <spandsp3/private/plc.h>
#include <spandsp3/private/playout.h>
#include <spandsp3/private/oki_adpcm.h>
#include <spandsp3/private/ima_adpcm.h>
#include <spandsp3/private/hdlc.h>
#include <spandsp3/private/time_scale.h>
#include <spandsp3/private/super_tone_tx.h>
#include <spandsp3/private/super_tone_rx.h>
#include <spandsp3/private/silence_gen.h>
#include <spandsp3/private/swept_tone.h>
#include <spandsp3/private/echo.h>
#include <spandsp3/private/modem_echo.h>
#include <spandsp3/private/async.h>
#include <spandsp3/private/fsk.h>
#include <spandsp3/private/modem_connect_tones.h>
#include <spandsp3/private/v8.h>
#include <spandsp3/private/v17rx.h>
#include <spandsp3/private/v17tx.h>
#include <spandsp3/private/v22bis.h>
#include <spandsp3/private/v27ter_rx.h>
#include <spandsp3/private/v27ter_tx.h>
#include <spandsp3/private/v29rx.h>
#include <spandsp3/private/v29tx.h>
#if defined(SPANDSP_SUPPORT_V32BIS)
#include <spandsp3/private/v32bis.h>
#endif
#if defined(SPANDSP_SUPPORT_V34)
#include <spandsp3/private/v34.h>
#endif
#include <spandsp3/private/v42.h>
#include <spandsp3/private/v42bis.h>
#include <spandsp3/private/at_interpreter.h>
#include <spandsp3/private/data_modems.h>
#include <spandsp3/private/fax_modems.h>
#include <spandsp3/private/timezone.h>
#include <spandsp3/private/image_translate.h>
#include <spandsp3/private/t4_t6_decode.h>
#include <spandsp3/private/t4_t6_encode.h>
#include <spandsp3/private/t81_t82_arith_coding.h>
#include <spandsp3/private/t85.h>
#include <spandsp3/private/t42.h>
#include <spandsp3/private/t43.h>
#include <spandsp3/private/t4_rx.h>
#include <spandsp3/private/t4_tx.h>
#include <spandsp3/private/t30.h>
#include <spandsp3/private/fax.h>
#include <spandsp3/private/t38_core.h>
#include <spandsp3/private/t38_non_ecm_buffer.h>
#include <spandsp3/private/t38_gateway.h>
#include <spandsp3/private/t38_terminal.h>
#include <spandsp3/private/t31.h>
#include <spandsp3/private/v18.h>
#include <spandsp3/private/adsi.h>
#include <spandsp3/private/ademco_contactid.h>

#endif
/*- End of file ------------------------------------------------------------*/
