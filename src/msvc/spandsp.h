/*
 * SpanDSP - a series of DSP components for telephony
 *
 * spandsp.h - The head guy amongst the headers
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

#if !defined(_SPANDSP_H_)
#define _SPANDSP_H_

#define __inline__ __inline
#pragma warning(disable:4200)

#undef SPANDSP_USE_FIXED_POINT
#undef SPANDSP_MISALIGNED_ACCESS_FAILS

#define SPANDSP_USE_EXPORT_CAPABILITY 1

#undef SPANDSP_SUPPORT_T43
#undef SPANDSP_SUPPORT_V32BIS
#undef SPANDSP_SUPPORT_V34
#undef SPANDSP_SUPPORT_TIFF_FX

#include <stdlib.h>
#include <msvc/inttypes.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <math.h>
#include <tiffio.h>

#if !defined(__cplusplus)
#include <spandsp/stdbool.h>
#endif
#include <spandsp/telephony.h>
#include <spandsp/alloc.h>
#include <spandsp/unaligned.h>
#include <spandsp/fast_convert.h>
#include <spandsp/logging.h>
#include <spandsp/complex.h>
#include <spandsp/bit_operations.h>
#include <spandsp/bitstream.h>
#include <spandsp/queue.h>
#include <spandsp/agc_float.h>
#include <spandsp/schedule.h>
#include <spandsp/g711.h>
#include <spandsp/timing.h>
#include <spandsp/math_fixed.h>
#include <spandsp/vector_float.h>
#include <spandsp/complex_vector_float.h>
#include <spandsp/vector_int.h>
#include <spandsp/complex_vector_int.h>
#include <spandsp/arctan2.h>
#include <spandsp/biquad.h>
#include <spandsp/fir.h>
#include <spandsp/awgn.h>
#include <spandsp/bert.h>
#include <spandsp/power_meter.h>
#include <spandsp/complex_filters.h>
#include <spandsp/dc_restore.h>
#include <spandsp/dds.h>
#include <spandsp/swept_tone.h>
#include <spandsp/echo.h>
#include <spandsp/modem_echo.h>
#include <spandsp/crc.h>
#include <spandsp/async.h>
#include <spandsp/hdlc.h>
#include <spandsp/noise.h>
#include <spandsp/saturated.h>
#include <spandsp/time_scale.h>
#include <spandsp/tone_detect.h>
#include <spandsp/tone_generate.h>
#include <spandsp/super_tone_rx.h>
#include <spandsp/super_tone_tx.h>
#include <spandsp/dtmf.h>
#include <spandsp/bell_r2_mf.h>
#include <spandsp/sig_tone.h>
#include <spandsp/fsk.h>
#include <spandsp/modem_connect_tones.h>
#include <spandsp/silence_gen.h>
#include <spandsp/v8.h>
#include <spandsp/v80.h>
#include <spandsp/godard.h>
#include <spandsp/v29rx.h>
#include <spandsp/v29tx.h>
#include <spandsp/v17rx.h>
#include <spandsp/v17tx.h>
#include <spandsp/v22bis.h>
#include <spandsp/v27ter_rx.h>
#include <spandsp/v27ter_tx.h>
#if defined(SPANDSP_SUPPORT_V32BIS)
#include <spandsp/v32bis.h>
#endif
#if defined(SPANDSP_SUPPORT_V34)
#include <spandsp/v34.h>
#endif
#include <spandsp/v42.h>
#include <spandsp/v42bis.h>
#include <spandsp/v18.h>
#include <spandsp/timezone.h>
#include <spandsp/ssl_fax.h>
#include <spandsp/t4_rx.h>
#include <spandsp/t4_tx.h>
#include <spandsp/image_translate.h>
#include <spandsp/t4_t6_decode.h>
#include <spandsp/t4_t6_encode.h>
#include <spandsp/t81_t82_arith_coding.h>
#include <spandsp/t85.h>
#include <spandsp/t42.h>
#include <spandsp/t43.h>
#include <spandsp/t30.h>
#include <spandsp/t30_api.h>
#include <spandsp/t30_fcf.h>
#include <spandsp/t30_logging.h>
#include <spandsp/t35.h>
#include <spandsp/at_interpreter.h>
#include <spandsp/data_modems.h>
#include <spandsp/fax_modems.h>
#include <spandsp/fax.h>
#include <spandsp/t38_core.h>
#include <spandsp/t38_non_ecm_buffer.h>
#include <spandsp/t38_gateway.h>
#include <spandsp/t38_terminal.h>
#include <spandsp/t31.h>
#include <spandsp/adsi.h>
#include <spandsp/ademco_contactid.h>
#include <spandsp/oki_adpcm.h>
#include <spandsp/ima_adpcm.h>
#include <spandsp/g722.h>
#include <spandsp/g726.h>
#include <spandsp/lpc10.h>
#include <spandsp/gsm0610.h>
#include <spandsp/plc.h>
#include <spandsp/playout.h>
#include <spandsp/sprt.h>
#include <spandsp/v150_1.h>
#include <spandsp/v150_1_sse.h>

#endif

#if defined(SPANDSP_EXPOSE_INTERNAL_STRUCTURES)
#include <spandsp/expose.h>
#endif
/*- End of file ------------------------------------------------------------*/
