##
## SpanDSP - a series of DSP components for telephony
##
## Makefile.am - Process this file with automake to produce Makefile.in
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU Lesser General Public License version 2.1,
## as published by the Free Software Foundation.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU Lesser General Public License for more details.
##
## You should have received a copy of the GNU Lesser General Public
## License along with this program; if not, write to the Free Software
## Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

AM_CFLAGS = $(COMP_VENDOR_CFLAGS)
AM_LDFLAGS = $(COMP_VENDOR_LDFLAGS)

DISTCLEANFILES = $(srcdir)/at_interpreter_dictionary.h \
                 $(srcdir)/cielab_luts.h \
                 $(srcdir)/math_fixed_tables.h \
                 $(srcdir)/v17_v32bis_rx_rrc.h \
                 $(srcdir)/v17_v32bis_rx_godard.h \
                 $(srcdir)/v17_v32bis_tx_rrc.h \
                 $(srcdir)/v22bis_rx_1200_rrc.h \
                 $(srcdir)/v22bis_rx_2400_rrc.h \
                 $(srcdir)/v22bis_tx_rrc.h \
                 $(srcdir)/v27ter_rx_2400_rrc.h \
                 $(srcdir)/v27ter_rx_4800_rrc.h \
                 $(srcdir)/v27ter_tx_2400_rrc.h \
                 $(srcdir)/v27ter_tx_4800_rrc.h \
                 $(srcdir)/v29rx_rrc.h \
                 $(srcdir)/v29rx_godard.h \
                 $(srcdir)/v29tx_rrc.h

CLEANFILES = ${DISTCLEANFILES}
MOSTLYCLEANFILES = ${DISTCLEANFILES}
MAINTAINERCLEANFILES = ${DISTCLEANFILES}

EXTRA_DIST = floating_fudge.h \
             filter_tools.c \
             libspandsp.2012.vcxproj \
             make_at_dictionary.c \
             make_cielab_luts.c \
             make_math_fixed_tables.c \
             make_modem_filter.c \
             make_t43_gray_code_tables.c \
             make_v34_convolutional_coders.c \
             make_v34_probe_signals.c \
             make_v34_shell_map.c \
             make_v34_tx_pre_emphasis_filters.c \
             msvc/config.h \
             msvc/getopt.c \
             msvc/gettimeofday.c \
             msvc/inttypes.h \
             msvc/msvcproj.head \
             msvc/msvcproj.foot \
             msvc/spandsp.h \
             msvc/tgmath.h \
             msvc/tiff/cleancount \
             msvc/unistd.h \
             msvc/util.vbs \
             msvc/vc12proj.foot \
             msvc/vc12proj.head \
             msvc/sys/time.h \
             spandsp/private/README \
             spandsp/version.h.in

AM_CPPFLAGS = -I$(top_builddir)

lib_LTLIBRARIES = libspandsp.la

if COND_V32BIS
V32BIS_SOURCES = v32bis.c
endif

if COND_V34
V34_SOURCES = v34rx.c \
              v34tx.c \
              v34_logging.c
endif

if COND_SSLFAX
SSLFAX_SOURCES = ssl_fax.c
endif

libspandsp_la_SOURCES = ademco_contactid.c \
                        adsi.c \
                        agc_float.c \
                        alloc.c \
                        async.c \
                        at_interpreter.c \
                        awgn.c \
                        bell_r2_mf.c \
                        bert.c \
                        bit_operations.c \
                        bitstream.c \
                        complex_filters.c \
                        complex_vector_float.c \
                        complex_vector_int.c \
                        crc.c \
                        data_modems.c \
                        dds_float.c \
                        dds_int.c \
                        dtmf.c \
                        echo.c \
                        fax.c \
                        fax_modems.c \
                        fsk.c \
                        g711.c \
                        g722.c \
                        g726.c \
                        godard.c \
                        gsm0610_decode.c \
                        gsm0610_encode.c \
                        gsm0610_long_term.c \
                        gsm0610_lpc.c \
                        gsm0610_preprocess.c \
                        gsm0610_rpe.c \
                        gsm0610_short_term.c \
                        hdlc.c \
                        ima_adpcm.c \
                        image_translate.c \
                        logging.c \
                        lpc10_analyse.c \
                        lpc10_decode.c \
                        lpc10_encode.c \
                        lpc10_placev.c \
                        lpc10_voicing.c \
                        math_fixed.c \
                        modem_echo.c \
                        modem_connect_tones.c \
                        noise.c \
                        oki_adpcm.c \
                        playout.c \
                        plc.c \
                        power_meter.c \
                        queue.c \
                        schedule.c \
                        sig_tone.c \
                        silence_gen.c \
                        sprt.c \
                        super_tone_rx.c \
                        super_tone_tx.c \
                        swept_tone.c \
                        t30.c \
                        t30_api.c \
                        t30_logging.c \
                        t31.c \
                        t35.c \
                        t38_core.c \
                        t38_gateway.c \
                        t38_non_ecm_buffer.c \
                        t38_terminal.c \
                        t4_t6_decode.c \
                        t4_t6_encode.c \
                        t4_rx.c \
                        t4_tx.c \
                        t42.c \
                        t43.c \
                        t81_t82_arith_coding.c \
                        t85_decode.c \
                        t85_encode.c \
                        testcpuid.c \
                        time_scale.c \
                        timezone.c \
                        tone_detect.c \
                        tone_generate.c \
                        v150_1.c \
                        v150_1_sse.c \
                        v17rx.c \
                        v17tx.c \
                        v18.c \
                        v22bis_rx.c \
                        v22bis_tx.c \
                        v27ter_rx.c \
                        v27ter_tx.c \
                        v29rx.c \
                        v29tx.c \
                        v42.c \
                        v42bis.c \
                        v8.c \
                        v80.c \
                        vector_float.c \
                        vector_int.c \
                        $(V32BIS_SOURCES) \
                        $(V34_SOURCES) \
                        $(SSLFAX_SOURCES)

libspandsp_la_LDFLAGS = -version-info @SPANDSP_LT_CURRENT@:@SPANDSP_LT_REVISION@:@SPANDSP_LT_AGE@ $(COMP_VENDOR_LDFLAGS)

nobase_include_HEADERS = spandsp/ademco_contactid.h \
                         spandsp/adsi.h \
                         spandsp/agc_float.h \
                         spandsp/alloc.h \
                         spandsp/async.h \
                         spandsp/arctan2.h \
                         spandsp/at_interpreter.h \
                         spandsp/awgn.h \
                         spandsp/bell_r2_mf.h \
                         spandsp/bert.h \
                         spandsp/biquad.h \
                         spandsp/bit_operations.h \
                         spandsp/bitstream.h \
                         spandsp/crc.h \
                         spandsp/complex.h \
                         spandsp/complex_filters.h \
                         spandsp/complex_vector_float.h \
                         spandsp/complex_vector_int.h \
                         spandsp/data_modems.h \
                         spandsp/dc_restore.h \
                         spandsp/dds.h \
                         spandsp/dtmf.h \
                         spandsp/echo.h \
                         spandsp/fast_convert.h \
                         spandsp/fax.h \
                         spandsp/fax_modems.h \
                         spandsp/fir.h \
                         spandsp/fsk.h \
                         spandsp/g168models.h \
                         spandsp/g711.h \
                         spandsp/g722.h \
                         spandsp/g726.h \
                         spandsp/godard.h \
                         spandsp/gsm0610.h \
                         spandsp/hdlc.h \
                         spandsp/ima_adpcm.h \
                         spandsp/image_translate.h \
                         spandsp/logging.h \
                         spandsp/lpc10.h \
                         spandsp/math_fixed.h \
                         spandsp/modem_echo.h \
                         spandsp/modem_connect_tones.h \
                         spandsp/noise.h \
                         spandsp/oki_adpcm.h \
                         spandsp/playout.h \
                         spandsp/plc.h \
                         spandsp/power_meter.h \
                         spandsp/queue.h \
                         spandsp/saturated.h \
                         spandsp/schedule.h \
                         spandsp/sig_tone.h \
                         spandsp/silence_gen.h \
                         spandsp/sprt.h \
                         spandsp/ssl_fax.h \
                         spandsp/stdbool.h \
                         spandsp/super_tone_rx.h \
                         spandsp/super_tone_tx.h \
                         spandsp/swept_tone.h \
                         spandsp/t30.h \
                         spandsp/t30_api.h \
                         spandsp/t30_fcf.h \
                         spandsp/t30_logging.h \
                         spandsp/t31.h \
                         spandsp/t35.h \
                         spandsp/t38_core.h \
                         spandsp/t38_gateway.h \
                         spandsp/t38_non_ecm_buffer.h \
                         spandsp/t38_terminal.h \
                         spandsp/t4_rx.h \
                         spandsp/t4_tx.h \
                         spandsp/t4_t6_decode.h \
                         spandsp/t4_t6_encode.h \
                         spandsp/t42.h \
                         spandsp/t43.h \
                         spandsp/t81_t82_arith_coding.h \
                         spandsp/t85.h \
                         spandsp/telephony.h \
                         spandsp/time_scale.h \
                         spandsp/timezone.h \
                         spandsp/timing.h \
                         spandsp/tone_detect.h \
                         spandsp/tone_generate.h \
                         spandsp/unaligned.h \
                         spandsp/v150_1.h \
                         spandsp/v150_1_sse.h \
                         spandsp/v17rx.h \
                         spandsp/v17tx.h \
                         spandsp/v18.h \
                         spandsp/v22bis.h \
                         spandsp/v27ter_rx.h \
                         spandsp/v27ter_tx.h \
                         spandsp/v29rx.h \
                         spandsp/v29tx.h \
                         spandsp/v32bis.h \
                         spandsp/v34.h \
                         spandsp/v42.h \
                         spandsp/v42bis.h \
                         spandsp/v8.h \
                         spandsp/v80.h \
                         spandsp/vector_float.h \
                         spandsp/vector_int.h \
                         spandsp/version.h \
                         spandsp/private/ademco_contactid.h \
                         spandsp/private/adsi.h \
                         spandsp/private/agc_float.h \
                         spandsp/private/async.h \
                         spandsp/private/at_interpreter.h \
                         spandsp/private/awgn.h \
                         spandsp/private/bell_r2_mf.h \
                         spandsp/private/bert.h \
                         spandsp/private/bitstream.h \
                         spandsp/private/data_modems.h \
                         spandsp/private/dtmf.h \
                         spandsp/private/echo.h \
                         spandsp/private/fax.h \
                         spandsp/private/fax_modems.h \
                         spandsp/private/fsk.h \
                         spandsp/private/g711.h \
                         spandsp/private/g722.h \
                         spandsp/private/g726.h \
                         spandsp/private/godard.h \
                         spandsp/private/gsm0610.h \
                         spandsp/private/hdlc.h \
                         spandsp/private/ima_adpcm.h \
                         spandsp/private/image_translate.h \
                         spandsp/private/logging.h \
                         spandsp/private/lpc10.h \
                         spandsp/private/modem_connect_tones.h \
                         spandsp/private/modem_echo.h \
                         spandsp/private/noise.h \
                         spandsp/private/oki_adpcm.h \
                         spandsp/private/playout.h \
                         spandsp/private/plc.h \
                         spandsp/private/power_meter.h \
                         spandsp/private/queue.h \
                         spandsp/private/schedule.h \
                         spandsp/private/sig_tone.h \
                         spandsp/private/silence_gen.h \
                         spandsp/private/sprt.h \
                         spandsp/private/ssl_fax.h \
                         spandsp/private/super_tone_rx.h \
                         spandsp/private/super_tone_tx.h \
                         spandsp/private/swept_tone.h \
                         spandsp/private/t30.h \
                         spandsp/private/t30_dis_dtc_dcs_bits.h \
                         spandsp/private/t31.h \
                         spandsp/private/t38_core.h \
                         spandsp/private/t38_gateway.h \
                         spandsp/private/t38_non_ecm_buffer.h \
                         spandsp/private/t38_terminal.h \
                         spandsp/private/t4_rx.h \
                         spandsp/private/t4_tx.h \
                         spandsp/private/t4_t6_decode.h \
                         spandsp/private/t4_t6_encode.h \
                         spandsp/private/t42.h \
                         spandsp/private/t43.h \
                         spandsp/private/t81_t82_arith_coding.h \
                         spandsp/private/t85.h \
                         spandsp/private/time_scale.h \
                         spandsp/private/timezone.h \
                         spandsp/private/tone_detect.h \
                         spandsp/private/tone_generate.h \
                         spandsp/private/v150_1.h \
                         spandsp/private/v150_1_sse.h \
                         spandsp/private/v17rx.h \
                         spandsp/private/v17tx.h \
                         spandsp/private/v18.h \
                         spandsp/private/v22bis.h \
                         spandsp/private/v27ter_rx.h \
                         spandsp/private/v27ter_tx.h \
                         spandsp/private/v29rx.h \
                         spandsp/private/v29tx.h \
                         spandsp/private/v32bis.h \
                         spandsp/private/v34.h \
                         spandsp/private/v42.h \
                         spandsp/private/v42bis.h \
                         spandsp/private/v8.h \
                         spandsp/private/v80.h \
                         spandsp/expose.h

nodist_include_HEADERS = spandsp.h

noinst_HEADERS = cielab_luts.h \
                 faxfont.h \
                 filter_tools.h \
                 gsm0610_local.h \
                 lpc10_encdecs.h \
                 mmx_sse_decs.h \
                 t30_local.h \
                 t4_t6_decode_states.h \
                 t42_t43_local.h \
                 v17_v32bis_rx_constellation_maps.h \
                 v17_v32bis_tx_constellation_maps.h \
                 v29tx_constellation_maps.h \
                 v34_local.h \
                 v34_shell_map.h \
                 v34_convolutional_coders.h \
                 v34_probe_signals.h \
                 v34_superconstellation_map.h \
                 v34_tx_pre_emphasis_filters.h \
                 v34_tables.h

make_at_dictionary$(EXEEXT): $(top_srcdir)/src/make_at_dictionary.c
	$(CC_FOR_BUILD) -o make_at_dictionary$(EXEEXT) $(top_srcdir)/src/make_at_dictionary.c -DHAVE_CONFIG_H -I$(top_builddir)/src

make_cielab_luts$(EXEEXT): $(top_srcdir)/src/make_cielab_luts.c
	$(CC_FOR_BUILD) -o make_cielab_luts$(EXEEXT) $(top_srcdir)/src/make_cielab_luts.c -DHAVE_CONFIG_H -I$(top_builddir)/src -lm

make_math_fixed_tables$(EXEEXT): $(top_srcdir)/src/make_math_fixed_tables.c
	$(CC_FOR_BUILD) -o make_math_fixed_tables$(EXEEXT) $(top_srcdir)/src/make_math_fixed_tables.c -DHAVE_CONFIG_H -I$(top_builddir)/src -lm

make_modem_filter$(EXEEXT): $(top_srcdir)/src/make_modem_filter.c $(top_srcdir)/src/filter_tools.c
	$(CC_FOR_BUILD) -o make_modem_filter$(EXEEXT) $(top_srcdir)/src/make_modem_filter.c $(top_srcdir)/src/filter_tools.c -DHAVE_CONFIG_H -I$(top_builddir)/src -lm

make_modem_godard_descriptor$(EXEEXT): $(top_srcdir)/src/make_modem_godard_descriptor.c $(top_srcdir)/src/filter_tools.c
	$(CC_FOR_BUILD) -o make_modem_godard_descriptor$(EXEEXT) $(top_srcdir)/src/make_modem_godard_descriptor.c $(top_srcdir)/src/filter_tools.c -DHAVE_CONFIG_H -I$(top_builddir)/src -lm

make_t43_gray_code_tables$(EXEEXT): $(top_srcdir)/src/make_t43_gray_code_tables.c
	$(CC_FOR_BUILD) -o make_t43_gray_code_tables$(EXEEXT) $(top_srcdir)/src/make_t43_gray_code_tables.c -DHAVE_CONFIG_H -I$(top_builddir)/src -lm

make_v17_v32_constellation_map$(EXEEXT): $(top_srcdir)/src/make_v17_v32_constellation_map.c $(top_srcdir)/src/g711.c $(top_srcdir)/src/alloc.c
	$(CC_FOR_BUILD) -o make_v17_v32_constellation_map$(EXEEXT) $(top_srcdir)/src/make_v17_v32_constellation_map.c $(top_srcdir)/src/g711.c $(top_srcdir)/src/alloc.c -DHAVE_CONFIG_H -I$(top_builddir)/src -lm

make_v17_v32_convolutional_encoder$(EXEEXT): $(top_srcdir)/src/make_v17_v32_convolutional_encoder.c $(top_srcdir)/src/g711.c $(top_srcdir)/src/alloc.c
	$(CC_FOR_BUILD) -o make_v17_v32_convolutional_encoder$(EXEEXT) $(top_srcdir)/src/make_v17_v32_convolutional_encoder.c $(top_srcdir)/src/g711.c $(top_srcdir)/src/alloc.c -DHAVE_CONFIG_H -I$(top_builddir)/src -lm

make_v29_constellation_map$(EXEEXT): $(top_srcdir)/src/make_v29_constellation_map.c $(top_srcdir)/src/g711.c $(top_srcdir)/src/alloc.c
	$(CC_FOR_BUILD) -o make_v29_constellation_map$(EXEEXT) $(top_srcdir)/src/make_v29_constellation_map.c $(top_srcdir)/src/g711.c $(top_srcdir)/src/alloc.c -DHAVE_CONFIG_H -I$(top_builddir)/src -lm

make_v34_convolutional_coders$(EXEEXT): $(top_srcdir)/src/make_v34_convolutional_coders.c
	$(CC_FOR_BUILD) -o make_v34_convolutional_coders$(EXEEXT) $(top_srcdir)/src/make_v34_convolutional_coders.c -DHAVE_CONFIG_H -I$(top_builddir)/src -lm

make_v34_probe_signals$(EXEEXT): $(top_srcdir)/src/make_v34_probe_signals.c $(top_srcdir)/src/g711.c $(top_srcdir)/src/alloc.c
	$(CC_FOR_BUILD) -o make_v34_probe_signals$(EXEEXT) $(top_srcdir)/src/make_v34_probe_signals.c $(top_srcdir)/src/g711.c $(top_srcdir)/src/alloc.c -DHAVE_CONFIG_H -I$(top_builddir)/src -lm

make_v34_shell_map$(EXEEXT): $(top_srcdir)/src/make_v34_shell_map.c
	$(CC_FOR_BUILD) -o make_v34_shell_map$(EXEEXT) $(top_srcdir)/src/make_v34_shell_map.c -DHAVE_CONFIG_H -I$(top_builddir)/src

make_v34_tx_pre_emphasis_filters$(EXEEXT): $(top_srcdir)/src/make_v34_tx_pre_emphasis_filters.c $(top_srcdir)/tools/meteor-engine.c
	$(CC_FOR_BUILD) -o make_v34_tx_pre_emphasis_filters$(EXEEXT) $(top_srcdir)/src/make_v34_tx_pre_emphasis_filters.c $(top_srcdir)/tools/meteor-engine.c -DHAVE_CONFIG_H -I$(top_builddir)/src -I$(top_builddir)/tools -lm

# We need to run make_at_dictionary, so it generates the
# at_interpreter_dictionary.h file

at_interpreter.$(OBJEXT): at_interpreter_dictionary.h

at_interpreter.lo: at_interpreter_dictionary.h

at_interpreter_dictionary.h: make_at_dictionary$(EXEEXT)
	./make_at_dictionary$(EXEEXT) >at_interpreter_dictionary.h

# We need to run make_math_fixed_tables, so it generates the
# math_fixed.h file

math_fixed.$(OBJEXT): math_fixed_tables.h

math_fixed.lo: math_fixed_tables.h

math_fixed_tables.h: make_math_fixed_tables$(EXEEXT)
	./make_math_fixed_tables$(EXEEXT) >math_fixed_tables.h

t4_rx.$(OBJEXT): spandsp/version.h

t4_rx.lo: spandsp/version.h

# We need to run make_cielab_luts, so it generates the
# cielab_luts.h file

t42.$(OBJEXT): cielab_luts.h

t42.lo: cielab_luts.h

cielab_luts.h: make_cielab_luts$(EXEEXT)
	./make_cielab_luts$(EXEEXT) >cielab_luts.h

# We need to run make_t43_gray_code_tables, so it generates the
# t43_gray_code_tables.h file

t43.lo: t43_gray_code_tables.h

t43_gray_code_tables.h: make_t43_gray_code_tables$(EXEEXT)
	./make_t43_gray_code_tables$(EXEEXT) >t43_gray_code_tables.h

# We need to run make_modem_filter, so it generates the
# filter coefficient files

V17_V32BIS_RX_INCL = v17_v32bis_rx_rrc.h v17_v32bis_rx_godard.h

v17rx.$(OBJEXT): ${V17_V32BIS_RX_INCL}

v17rx.lo: ${V17_V32BIS_RX_INCL}

v17_v32bis_rx_rrc.h: make_modem_filter$(EXEEXT)
	./make_modem_filter$(EXEEXT) -m V.17 -r >v17_v32bis_rx_rrc.h

v17_v32bis_rx_godard.h: make_modem_godard_descriptor$(EXEEXT)
	./make_modem_godard_descriptor$(EXEEXT) 1800.0 2400.0 0.99 1000.0 100.0 15 1 >v17_v32bis_rx_godard.h

V17_V32BIS_TX_INCL = v17_v32bis_tx_rrc.h

v17tx.$(OBJEXT): ${V17_V32BIS_TX_INCL}

v17tx.lo: ${V17_V32BIS_TX_INCL}

v17_v32bis_tx_rrc.h: make_modem_filter$(EXEEXT)
	./make_modem_filter$(EXEEXT) -m V.17 -t >v17_v32bis_tx_rrc.h

V22BIS_RX_INCL = v22bis_rx_1200_rrc.h \
                 v22bis_rx_2400_rrc.h

v22bis_rx.$(OBJEXT): ${V22BIS_RX_INCL}

v22bis_rx.lo: ${V22BIS_RX_INCL}

v22bis_rx_1200_rrc.h: make_modem_filter$(EXEEXT)
	./make_modem_filter$(EXEEXT) -m V.22bis1200 -r >v22bis_rx_1200_rrc.h

v22bis_rx_2400_rrc.h: make_modem_filter$(EXEEXT)
	./make_modem_filter$(EXEEXT) -m V.22bis2400 -r >v22bis_rx_2400_rrc.h

V22BIS_TX_INCL = v22bis_tx_rrc.h

v22bis_tx.$(OBJEXT): ${V22BIS_TX_INCL}

v22bis_tx.lo: ${V22BIS_TX_INCL}

v22bis_tx_rrc.h: make_modem_filter$(EXEEXT)
	./make_modem_filter$(EXEEXT) -m V.22bis -t >v22bis_tx_rrc.h

V27_RX_INCL = v27ter_rx_2400_rrc.h \
              v27ter_rx_4800_rrc.h

v27ter_rx.$(OBJEXT): ${V27_RX_INCL}

v27ter_rx.lo: ${V27_RX_INCL}

v27ter_rx_2400_rrc.h: make_modem_filter$(EXEEXT)
	./make_modem_filter$(EXEEXT) -m V.27ter2400 -r >v27ter_rx_2400_rrc.h

v27ter_rx_4800_rrc.h: make_modem_filter$(EXEEXT)
	./make_modem_filter$(EXEEXT) -m V.27ter4800 -r >v27ter_rx_4800_rrc.h

V27TER_TX_INCL = v27ter_tx_2400_rrc.h \
                 v27ter_tx_4800_rrc.h

v27ter_tx_.$(OBJEXT): ${V27TER_TX_INCL}

v27ter_tx.lo: ${V27TER_TX_INCL}

v27ter_tx_2400_rrc.h: make_modem_filter$(EXEEXT)
	./make_modem_filter$(EXEEXT) -m V.27ter2400 -t >v27ter_tx_2400_rrc.h

v27ter_tx_4800_rrc.h: make_modem_filter$(EXEEXT)
	./make_modem_filter$(EXEEXT) -m V.27ter4800 -t >v27ter_tx_4800_rrc.h

V29_RX_INCL = v29rx_rrc.h v29rx_godard.h

v29rx.$(OBJEXT): ${V29_RX_INCL}

v29rx.lo: ${V29_RX_INCL}

v29rx_rrc.h: make_modem_filter$(EXEEXT)
	./make_modem_filter$(EXEEXT) -m V.29 -r >v29rx_rrc.h

v29rx_godard.h: make_modem_godard_descriptor$(EXEEXT)
	./make_modem_godard_descriptor$(EXEEXT) 1700.0 2400.0 0.99 1000.0 30.0 5 1 >v29rx_godard.h

V29_TX_INCL = v29tx_rrc.h

v29tx.$(OBJEXT): ${V29_TX_INCL}

v29tx.lo: ${V29_TX_INCL}

v29tx_rrc.h: make_modem_filter$(EXEEXT)
	./make_modem_filter$(EXEEXT) -m V.29 -t >v29tx_rrc.h

V34_TX_INCL = v34_tx_2400_rrc.h \
              v34_tx_2743_rrc.h \
              v34_tx_2800_rrc.h \
              v34_tx_3000_rrc.h \
              v34_tx_3200_rrc.h \
              v34_tx_3429_rrc.h \
              v34_convolutional_coders.h \
              v34_probe_signals.h \
              v34_shell_map.h \
              v34_tx_pre_emphasis_filters.h

V34_RX_INCL = v34_rx_2400_low_carrier_rrc.h \
              v34_rx_2400_high_carrier_rrc.h \
              v34_rx_2743_low_carrier_rrc.h \
              v34_rx_2743_high_carrier_rrc.h \
              v34_rx_2800_low_carrier_rrc.h \
              v34_rx_2800_high_carrier_rrc.h \
              v34_rx_3000_low_carrier_rrc.h \
              v34_rx_3000_high_carrier_rrc.h \
              v34_rx_3200_low_carrier_rrc.h \
              v34_rx_3200_high_carrier_rrc.h \
              v34_rx_3429_rrc.h \
              v34_convolutional_coders.h \
              v34_probe_signals.h \
              v34_shell_map.h

v34tx.$(OBJEXT): ${V34_TX_INCL}

v34tx.lo: ${V34_TX_INCL}

v34rx.$(OBJEXT): ${V34_RX_INCL}

v34rx.lo: ${V34_RX_INCL}

v34_convolutional_coders.h: make_v34_convolutional_coders$(EXEEXT)
	./make_v34_convolutional_coders$(EXEEXT) >v34_convolutional_coders.h

v34_probe_signals.h: make_v34_probe_signals$(EXEEXT)
	./make_v34_probe_signals$(EXEEXT) >v34_probe_signals.h

v34_shell_map.h: make_v34_shell_map$(EXEEXT)
	./make_v34_shell_map$(EXEEXT) >v34_shell_map.h

v34_tx_pre_emphasis_filters.h: make_v34_tx_pre_emphasis_filters$(EXEEXT)
	./make_v34_tx_pre_emphasis_filters$(EXEEXT) >/dev/null

v34_rx_2400_low_carrier_rrc.h: make_modem_filter$(EXEEXT)
	./make_modem_filter$(EXEEXT) -m V.34_2400 -r >v34_rx_2400_low_carrier_rrc.h

v34_rx_2400_high_carrier_rrc.h: make_modem_filter$(EXEEXT)
	./make_modem_filter$(EXEEXT) -m V.34_2400_high -r >v34_rx_2400_high_carrier_rrc.h

v34_rx_2743_low_carrier_rrc.h: make_modem_filter$(EXEEXT)
	./make_modem_filter$(EXEEXT) -m V.34_2743 -r >v34_rx_2743_low_carrier_rrc.h

v34_rx_2743_high_carrier_rrc.h: make_modem_filter$(EXEEXT)
	./make_modem_filter$(EXEEXT) -m V.34_2743_high -r >v34_rx_2743_high_carrier_rrc.h

v34_rx_2800_low_carrier_rrc.h: make_modem_filter$(EXEEXT)
	./make_modem_filter$(EXEEXT) -m V.34_2800 -r >v34_rx_2800_low_carrier_rrc.h

v34_rx_2800_high_carrier_rrc.h: make_modem_filter$(EXEEXT)
	./make_modem_filter$(EXEEXT) -m V.34_2800_high -r >v34_rx_2800_high_carrier_rrc.h

v34_rx_3000_low_carrier_rrc.h: make_modem_filter$(EXEEXT)
	./make_modem_filter$(EXEEXT) -m V.34_3000 -r >v34_rx_3000_low_carrier_rrc.h

v34_rx_3000_high_carrier_rrc.h: make_modem_filter$(EXEEXT)
	./make_modem_filter$(EXEEXT) -m V.34_3000_high -r >v34_rx_3000_high_carrier_rrc.h

v34_rx_3200_low_carrier_rrc.h: make_modem_filter$(EXEEXT)
	./make_modem_filter$(EXEEXT) -m V.34_3200 -r >v34_rx_3200_low_carrier_rrc.h

v34_rx_3200_high_carrier_rrc.h: make_modem_filter$(EXEEXT)
	./make_modem_filter$(EXEEXT) -m V.34_3200_high -r >v34_rx_3200_high_carrier_rrc.h

v34_rx_3429_rrc.h: make_modem_filter$(EXEEXT)
	./make_modem_filter$(EXEEXT) -m V.34_3429 -r >v34_rx_3429_rrc.h

v34_tx_2400_rrc.h: make_modem_filter$(EXEEXT)
	./make_modem_filter$(EXEEXT) -m V.34_2400 -t >v34_tx_2400_rrc.h

v34_tx_2743_rrc.h: make_modem_filter$(EXEEXT)
	./make_modem_filter$(EXEEXT) -m V.34_2743 -t >v34_tx_2743_rrc.h

v34_tx_2800_rrc.h: make_modem_filter$(EXEEXT)
	./make_modem_filter$(EXEEXT) -m V.34_2800 -t >v34_tx_2800_rrc.h

v34_tx_3000_rrc.h: make_modem_filter$(EXEEXT)
	./make_modem_filter$(EXEEXT) -m V.34_3000 -t >v34_tx_3000_rrc.h

v34_tx_3200_rrc.h: make_modem_filter$(EXEEXT)
	./make_modem_filter$(EXEEXT) -m V.34_3200 -t >v34_tx_3200_rrc.h

v34_tx_3429_rrc.h: make_modem_filter$(EXEEXT)
	./make_modem_filter$(EXEEXT) -m V.34_3429 -t >v34_tx_3429_rrc.h

VCPROJ12 = libspandsp.2012.vcxproj

WIN32SOURCES = $(libspandsp_la_SOURCES) .\\msvc\\gettimeofday.c
WIN32HEADERS = $(nobase_include_HEADERS) spandsp.h

VCPROJOUT12 = | awk '{printf("%s\r\n", $$0)}' >> $(VCPROJ12)

$(VCPROJ12): msvc/vc12proj.head msvc/vc12proj.foot Makefile.am
	echo "creating $(VCPROJ12)"
	@(cp $(srcdir)/msvc/vc12proj.head $(VCPROJ12); \
	for file in $(WIN32SOURCES); do \
		echo "<File RelativePath=\""$$file"\"></File>" $(VCPROJOUT12); \
	done; \
	echo "</Filter><Filter  Name=\"Header Files\">" $(VCPROJOUT12); \
	for file in $(WIN32HEADERS); do \
		echo "<File RelativePath=\""$$file"\"></File>" $(VCPROJOUT12); \
	done; \
	cat $(srcdir)/msvc/vc12proj.foot $(VCPROJOUT12) )

$(srcdir)/msvc/spandsp.h: spandsp.h.in
	echo "creating $(srcdir)/msvc/spandsp.h"
	@sed -e "s/#define _SPANDSP_H_/#define _SPANDSP_H_\n\n#define __inline__ __inline\n#pragma warning(disable:4200)/" \
	     -e "s/\@SPANDSP_USE_FIXED_POINT\@/#undef SPANDSP_USE_FIXED_POINT/" \
	     -e "s/\@SPANDSP_MISALIGNED_ACCESS_FAILS\@/#undef SPANDSP_MISALIGNED_ACCESS_FAILS/" \
	     -e "s/\@SPANDSP_USE_EXPORT_CAPABILITY\@/#define SPANDSP_USE_EXPORT_CAPABILITY 1/" \
	     -e "s/\@INSERT_INTTYPES_HEADER\@/#include <msvc\/inttypes.h>/" \
	     -e "s/\@INSERT_MATH_HEADER\@/#include <math.h>/" $(srcdir)/spandsp.h.in > $(srcdir)/msvc/spandsp.h

dist-hook: spandsp/version.h

spandsp/version.h:
	mkdir -p $(@D)
	NOWDATE=`date --utc +"%Y%m%d"` ; \
	NOWTIME=`date --utc +"%H%M%S"` ; \
	sed 's/$$SPANDSP_RELEASE_DATE/'$$NOWDATE'/;s/$$SPANDSP_RELEASE_TIME/'$$NOWTIME'/' \
		<$(srcdir)/spandsp/version.h.in >$@
