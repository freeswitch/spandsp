##
## SpanDSP - a series of DSP components for telephony
##
## Makefile.am - Process this file with automake to produce Makefile.in
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License version 2, as
## published by the Free Software Foundation.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

AM_CFLAGS = $(COMP_VENDOR_CFLAGS)
AM_LDFLAGS = $(COMP_VENDOR_LDFLAGS)

LIBS += $(TESTLIBS)

noinst_DATA = sound_c1_8k.wav sound_c3_8k.wav

EXTRA_DIST = fax_tests.sh \
             regression_tests.sh \
             tsb85_extra_tests.sh \
             tsb85_tests.sh \
             v42bis_tests.sh \
             msvc/complex_tests.2012.vcxproj \
             msvc/complex_tests.2012.vcxproj.filters \
             msvc/complex_vector_float_tests.2012.vcxproj \
             msvc/complex_vector_float_tests.2012.vcxproj.filters \
             msvc/complex_vector_int_tests.2012.vcxproj \
             msvc/complex_vector_int_tests.2012.vcxproj.filters \
             msvc/dtmf_rx_tests.2012.vcxproj \
             msvc/dtmf_rx_tests.2012.vcxproj.filters \
             msvc/dtmf_tx_tests.2012.vcxproj \
             msvc/dtmf_tx_tests.2012.vcxproj.filters \
             msvc/queue_tests.2012.vcxproj \
             msvc/queue_tests.2012.vcxproj.filters \
             msvc/t38_core_tests.2012.vcxproj \
             msvc/t38_core_tests.2012.vcxproj.filters \
             msvc/t38_non_ecm_buffer_tests.2012.vcxproj \
             msvc/t38_non_ecm_buffer_tests.2012.vcxproj.filters \
             msvc/v22bis_tests.2012.vcxproj \
             msvc/v22bis_tests.2012.vcxproj.filters \
             msvc/v29_tests.2012.vcxproj \
             msvc/v29_tests.2012.vcxproj.filters \
             msvc/v8_tests.2012.vcxproj \
             msvc/v8_tests.2012.vcxproj.filters \
             msvc/v80_tests.2012.vcxproj \
             msvc/v80_tests.2012.vcxproj.filters \
             msvc/vector_float_tests.2012.vcxproj \
             msvc/vector_float_tests.2012.vcxproj.filters \
             msvc/vector_int_tests.2012.vcxproj \
             msvc/vector_int_tests.2012.vcxproj.filters

MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = -I$(top_builddir)/src -I$(top_builddir)/spandsp-sim -DDATADIR="\"$(pkgdatadir)\""

LIBDIR = -L$(top_builddir)/src

if COND_SSLFAX
BASE_LIBS = $(LIBDIR) -lspandsp -lssl
else
BASE_LIBS = $(LIBDIR) -lspandsp
endif

if COND_V32BIS
V32BIS_PROGS = v32bis_tests
endif

if COND_V34
V34_PROGS = v34_tests
endif

noinst_PROGRAMS =   ademco_contactid_tests \
                    adsi_tests \
                    agc_float_tests \
                    alloc_tests \
                    async_tests \
                    at_interpreter_tests \
                    awgn_tests \
                    bell_mf_rx_tests \
                    bell_mf_tx_tests \
                    bert_tests \
                    bit_operations_tests \
                    bitstream_tests \
                    complex_tests \
                    complex_vector_float_tests \
                    complex_vector_int_tests \
                    crc_tests \
                    data_modems_tests \
                    dc_restore_tests \
                    dds_tests \
                    dtmf_rx_tests \
                    dtmf_tx_tests \
                    dummy_modems_tests \
                    echo_tests \
                    fax_decode \
                    fax_tests \
                    fsk_tests \
                    g1050_tests \
                    g168_tests \
                    g711_tests \
                    g722_tests \
                    g726_tests \
                    gsm0610_tests \
                    hdlc_tests \
                    ima_adpcm_tests \
                    image_translate_tests \
                    line_model_tests \
                    logging_tests \
                    lpc10_tests \
                    math_fixed_tests \
                    make_g168_css \
                    modem_connect_tones_tests \
                    modem_echo_tests \
                    noise_tests \
                    oki_adpcm_tests \
                    playout_tests \
                    plc_tests \
                    power_meter_tests \
                    pseudo_terminal_tests \
                    queue_tests \
                    r2_mf_rx_tests \
                    r2_mf_tx_tests \
                    rfc2198_sim_tests \
                    saturated_tests \
                    schedule_tests \
                    sig_tone_tests \
                    sprt_decode \
                    sprt_tests \
                    super_tone_rx_tests \
                    super_tone_tx_tests \
                    swept_tone_tests \
                    t31_pseudo_terminal_tests \
                    t31_tests \
                    t35_tests \
                    t38_core_tests \
                    t38_decode \
                    t38_non_ecm_buffer_tests \
                    t4_tests \
                    t4_t6_tests \
                    t42_tests \
                    t43_tests \
                    t81_t82_arith_coding_tests \
                    t85_tests \
                    time_scale_tests \
                    timezone_tests \
                    tone_detect_tests \
                    tone_generate_tests \
                    tsb85_tests \
                    v150_1_tests \
                    v17_tests \
                    v18_tests \
                    v22bis_tests \
                    v27ter_tests \
                    v29_tests \
                    v42_tests \
                    v42bis_tests \
                    v8_tests \
                    v80_tests \
                    vector_float_tests \
                    vector_int_tests \
                    $(V32BIS_PROGS) \
                    $(V34_PROGS)

noinst_HEADERS =    echo_monitor.h \
                    fax_tester.h \
                    fax_utils.h \
                    line_model_monitor.h \
                    media_monitor.h \
                    modem_monitor.h \
                    pcap_parse.h \
                    pseudo_terminals.h \
                    socket_dgram_harness.h \
                    socket_harness.h \
                    udptl.h

ademco_contactid_tests_SOURCES = ademco_contactid_tests.c
ademco_contactid_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

adsi_tests_SOURCES = adsi_tests.c
adsi_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

agc_float_tests_SOURCES = agc_float_tests.c
agc_float_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

alloc_tests_SOURCES = alloc_tests.c
alloc_tests_LDADD = $(BASE_LIBS)

async_tests_SOURCES = async_tests.c
async_tests_LDADD = $(BASE_LIBS)

at_interpreter_tests_SOURCES = at_interpreter_tests.c
at_interpreter_tests_LDADD = $(BASE_LIBS)

awgn_tests_SOURCES = awgn_tests.c
awgn_tests_LDADD = $(BASE_LIBS)

bell_mf_rx_tests_SOURCES = bell_mf_rx_tests.c
bell_mf_rx_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

bell_mf_tx_tests_SOURCES = bell_mf_tx_tests.c
bell_mf_tx_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

bert_tests_SOURCES = bert_tests.c
bert_tests_LDADD = $(BASE_LIBS)

bit_operations_tests_SOURCES = bit_operations_tests.c
bit_operations_tests_LDADD = $(BASE_LIBS)

bitstream_tests_SOURCES = bitstream_tests.c
bitstream_tests_LDADD = $(BASE_LIBS)

complex_tests_SOURCES = complex_tests.c
complex_tests_LDADD = $(BASE_LIBS)

complex_vector_float_tests_SOURCES = complex_vector_float_tests.c
complex_vector_float_tests_LDADD = $(BASE_LIBS)

complex_vector_int_tests_SOURCES = complex_vector_int_tests.c
complex_vector_int_tests_LDADD = $(BASE_LIBS)

crc_tests_SOURCES = crc_tests.c
crc_tests_LDADD = $(BASE_LIBS)

data_modems_tests_SOURCES = data_modems_tests.c media_monitor.cpp
data_modems_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

dc_restore_tests_SOURCES = dc_restore_tests.c
dc_restore_tests_LDADD = $(BASE_LIBS)

dds_tests_SOURCES = dds_tests.c
dds_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

dtmf_rx_tests_SOURCES = dtmf_rx_tests.c
dtmf_rx_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

dtmf_tx_tests_SOURCES = dtmf_tx_tests.c
dtmf_tx_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

dummy_modems_tests_SOURCES = dummy_modems_tests.c media_monitor.cpp socket_harness.c pseudo_terminals.c
dummy_modems_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS) -lutil

echo_tests_SOURCES = echo_tests.c echo_monitor.cpp
echo_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

fax_decode_SOURCES = fax_decode.c
fax_decode_LDADD = $(BASE_LIBS)

fax_tests_SOURCES = fax_tests.c fax_utils.c media_monitor.cpp fax_tester.c
fax_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

fsk_tests_SOURCES = fsk_tests.c
fsk_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

g1050_tests_SOURCES = g1050_tests.c media_monitor.cpp
g1050_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

g168_tests_SOURCES = g168_tests.c
g168_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

g711_tests_SOURCES = g711_tests.c
g711_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

g722_tests_SOURCES = g722_tests.c
g722_tests_LDADD = $(BASE_LIBS)

g726_tests_SOURCES = g726_tests.c
g726_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

gsm0610_tests_SOURCES = gsm0610_tests.c
gsm0610_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

hdlc_tests_SOURCES = hdlc_tests.c
hdlc_tests_LDADD = $(BASE_LIBS)

ima_adpcm_tests_SOURCES = ima_adpcm_tests.c
ima_adpcm_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

image_translate_tests_SOURCES = image_translate_tests.c
image_translate_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

line_model_tests_SOURCES = line_model_tests.c
line_model_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

logging_tests_SOURCES = logging_tests.c
logging_tests_LDADD = $(BASE_LIBS)

lpc10_tests_SOURCES = lpc10_tests.c
lpc10_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

math_fixed_tests_SOURCES = math_fixed_tests.c
math_fixed_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

make_g168_css_SOURCES = make_g168_css.c
make_g168_css_LDADD = $(BASE_LIBS)

modem_echo_tests_SOURCES = modem_echo_tests.c echo_monitor.cpp
modem_echo_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

modem_connect_tones_tests_SOURCES = modem_connect_tones_tests.c
modem_connect_tones_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

noise_tests_SOURCES = noise_tests.c
noise_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

oki_adpcm_tests_SOURCES = oki_adpcm_tests.c
oki_adpcm_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

playout_tests_SOURCES = playout_tests.c media_monitor.cpp
playout_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

plc_tests_SOURCES = plc_tests.c
plc_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

power_meter_tests_SOURCES = power_meter_tests.c
power_meter_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

pseudo_terminal_tests_SOURCES = pseudo_terminal_tests.c fax_utils.c pseudo_terminals.c
pseudo_terminal_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim -lspandsp -lutil

queue_tests_SOURCES = queue_tests.c
queue_tests_LDADD = $(BASE_LIBS)

r2_mf_rx_tests_SOURCES = r2_mf_rx_tests.c
r2_mf_rx_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

r2_mf_tx_tests_SOURCES = r2_mf_tx_tests.c
r2_mf_tx_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

rfc2198_sim_tests_SOURCES = rfc2198_sim_tests.c media_monitor.cpp
rfc2198_sim_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

saturated_tests_SOURCES = saturated_tests.c
saturated_tests_LDADD = $(BASE_LIBS) 

schedule_tests_SOURCES = schedule_tests.c
schedule_tests_LDADD = $(BASE_LIBS) 

sig_tone_tests_SOURCES = sig_tone_tests.c
sig_tone_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS) 

sprt_decode_SOURCES = sprt_decode.c pcap_parse.c
sprt_decode_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

sprt_tests_SOURCES = sprt_tests.c socket_dgram_harness.c pseudo_terminals.c
sprt_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

super_tone_rx_tests_SOURCES = super_tone_rx_tests.c
super_tone_rx_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

super_tone_tx_tests_SOURCES = super_tone_tx_tests.c
super_tone_tx_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

swept_tone_tests_SOURCES = swept_tone_tests.c
swept_tone_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

t31_pseudo_terminal_tests_SOURCES = t31_pseudo_terminal_tests.c fax_utils.c pseudo_terminals.c
t31_pseudo_terminal_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim -lspandsp -lutil

t31_tests_SOURCES = t31_tests.c fax_utils.c media_monitor.cpp
t31_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

t35_tests_SOURCES = t35_tests.c
t35_tests_LDADD = $(BASE_LIBS)

t38_core_tests_SOURCES = t38_core_tests.c
t38_core_tests_LDADD = $(BASE_LIBS)

t38_decode_SOURCES = t38_decode.c fax_utils.c pcap_parse.c udptl.c
t38_decode_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

t38_non_ecm_buffer_tests_SOURCES = t38_non_ecm_buffer_tests.c
t38_non_ecm_buffer_tests_LDADD = $(BASE_LIBS)

t4_tests_SOURCES = t4_tests.c
t4_tests_LDADD = $(BASE_LIBS)

t4_t6_tests_SOURCES = t4_t6_tests.c
t4_t6_tests_LDADD = $(BASE_LIBS)

t42_tests_SOURCES = t42_tests.c
t42_tests_LDADD = $(BASE_LIBS)

t43_tests_SOURCES = t43_tests.c
t43_tests_LDADD = $(BASE_LIBS)

t81_t82_arith_coding_tests_SOURCES = t81_t82_arith_coding_tests.c
t81_t82_arith_coding_tests_LDADD = $(BASE_LIBS)

t85_tests_SOURCES = t85_tests.c
t85_tests_LDADD = $(BASE_LIBS)

time_scale_tests_SOURCES = time_scale_tests.c
time_scale_tests_LDADD = $(BASE_LIBS)

timezone_tests_SOURCES = timezone_tests.c
timezone_tests_LDADD = $(BASE_LIBS)

tone_detect_tests_SOURCES = tone_detect_tests.c
tone_detect_tests_LDADD = $(BASE_LIBS)

tone_generate_tests_SOURCES = tone_generate_tests.c
tone_generate_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

tsb85_tests_SOURCES = tsb85_tests.c fax_utils.c fax_tester.c
tsb85_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

v150_1_tests_SOURCES = v150_1_tests.c socket_dgram_harness.c pseudo_terminals.c
v150_1_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

v17_tests_SOURCES = v17_tests.c line_model_monitor.cpp modem_monitor.cpp
v17_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

v18_tests_SOURCES = v18_tests.c
v18_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

v22bis_tests_SOURCES = v22bis_tests.c line_model_monitor.cpp modem_monitor.cpp
v22bis_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

v27ter_tests_SOURCES = v27ter_tests.c line_model_monitor.cpp modem_monitor.cpp
v27ter_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

v29_tests_SOURCES = v29_tests.c line_model_monitor.cpp modem_monitor.cpp
v29_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

if COND_V32BIS
v32bis_tests_SOURCES = v32bis_tests.c line_model_monitor.cpp modem_monitor.cpp
v32bis_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)
endif

if COND_V34
v34_tests_SOURCES = v34_tests.c line_model_monitor.cpp modem_monitor.cpp
v34_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)
endif

v42_tests_SOURCES = v42_tests.c
v42_tests_LDADD = $(BASE_LIBS)

v42bis_tests_SOURCES = v42bis_tests.c
v42bis_tests_LDADD = $(BASE_LIBS)

v8_tests_SOURCES = v8_tests.c
v8_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

v80_tests_SOURCES = v80_tests.c
v80_tests_LDADD = -L$(top_builddir)/spandsp-sim -lspandsp-sim $(BASE_LIBS)

vector_float_tests_SOURCES = vector_float_tests.c
vector_float_tests_LDADD = $(BASE_LIBS)

vector_int_tests_SOURCES = vector_int_tests.c
vector_int_tests_LDADD = $(BASE_LIBS)

# We need to create the CSS files for echo cancellation tests.

sound_c1_8k.wav sound_c3_8k.wav: make_g168_css$(EXEEXT)
	./make_g168_css$(EXEEXT)
	sox sound_c1.wav -r8000 sound_c1_8k.wav
	sox sound_c3.wav -r8000 sound_c3_8k.wav
	rm sound_c1.wav sound_c3.wav
