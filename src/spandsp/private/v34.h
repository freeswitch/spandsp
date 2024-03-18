/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/v34.h - ITU V.34 modem
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2009 Steve Underwood
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

#if !defined(_SPANDSP_PRIVATE_V34_H_)
#define _SPANDSP_PRIVATE_V34_H_

/*! The number of taps in the info data transmit pulse shaping filter */
#define V34_INFO_TX_FILTER_STEPS            9
#define V34_TX_FILTER_STEPS                 9

#define V34_RX_FILTER_STEPS                 27
#define V34_RX_PULSESHAPER_COEFF_SETS       192
#define V34_RX_CC_PULSESHAPER_COEFF_SETS    12

#define V34_EQUALIZER_PRE_LEN               63
#define V34_EQUALIZER_POST_LEN              63
#define V34_EQUALIZER_MASK                  127

/*! The offset between x index values, and what they mean in terms of the V.34
    spec numbering */
#define V34_XOFF                            3

#define V34_RX_PULSESHAPER_GAIN             1.000000f

extern const complexf_t v34_constellation[16];

#if defined(SPANDSP_USE_FIXED_POINT)
typedef int16_t v34_rx_shaper_t[V34_RX_PULSESHAPER_COEFF_SETS][V34_RX_FILTER_STEPS];
typedef int16_t cc_rx_shaper_t[V34_RX_CC_PULSESHAPER_COEFF_SETS][V34_RX_FILTER_STEPS];
#else
typedef float v34_rx_shaper_t[V34_RX_PULSESHAPER_COEFF_SETS][V34_RX_FILTER_STEPS];
typedef float cc_rx_shaper_t[V34_RX_CC_PULSESHAPER_COEFF_SETS][V34_RX_FILTER_STEPS];
#endif

typedef const uint8_t conv_encode_table_t[64][16];
typedef const uint8_t conv_decode_table_t[16][16];

enum
{
    V34_MODULATION_V34 = 0,
    V34_MODULATION_CC,
    V34_MODULATION_TONES,
    V34_MODULATION_L1_L2,
    V34_MODULATION_SILENCE
};

enum v34_rx_stages_e
{
    V34_RX_STAGE_INFO0 = 1,
    V34_RX_STAGE_INFOH,
    V34_RX_STAGE_INFO1C,
    V34_RX_STAGE_INFO1A,
    V34_RX_STAGE_TONE_A,
    V34_RX_STAGE_TONE_B,
    V34_RX_STAGE_L1_L2,
    V34_RX_STAGE_CC,
    V34_RX_STAGE_PRIMARY_CHANNEL
};

enum v34_tx_stages_e
{
    /*! \brief An initial bit of extra preamble ahead of the first INFO0, to ensure
               bit synchronisation is OK by the first bit of INFO0 */
    V34_TX_STAGE_INITIAL_PREAMBLE = 1,
    /*! \brief INFO0 is being transmitted the first time */
    V34_TX_STAGE_INFO0,
    /*! \brief Transmitting A while waiting for 50ms timeout */
    V34_TX_STAGE_INITIAL_A,
    /*! \brief Transmitting A while waiting for received INFO0c */
    V34_TX_STAGE_FIRST_A,
    V34_TX_STAGE_FIRST_NOT_A,
    V34_TX_STAGE_FIRST_NOT_A_REVERSAL_SEEN,
    V34_TX_STAGE_SECOND_A,
    /*! \brief L1 is being transmitted */
    V34_TX_STAGE_L1,
    /*! \brief L2 is being transmitted */
    V34_TX_STAGE_L2,
    V34_TX_STAGE_POST_L2_A,
    V34_TX_STAGE_POST_L2_NOT_A,
    V34_TX_STAGE_A_SILENCE,
    V34_TX_STAGE_PRE_INFO1_A,
    /*! \brief INFO1 is being trasnmitted */
    V34_TX_STAGE_INFO1,

    V34_TX_STAGE_FIRST_B,
    V34_TX_STAGE_FIRST_B_INFO_SEEN,
    V34_TX_STAGE_FIRST_NOT_B_WAIT,
    V34_TX_STAGE_FIRST_NOT_B,
    V34_TX_STAGE_FIRST_B_SILENCE,
    V34_TX_STAGE_FIRST_B_POST_REVERSAL_SILENCE,
    V34_TX_STAGE_SECOND_B,
    V34_TX_STAGE_SECOND_B_WAIT,
    V34_TX_STAGE_SECOND_NOT_B,
    /*! \brief INFO0 is being resent on a bad startup */
    V34_TX_STAGE_INFO0_RETRY,

    V34_TX_STAGE_FIRST_S,
    V34_TX_STAGE_FIRST_NOT_S,
    /*! \brief The optional MD is being transmitted */
    V34_TX_STAGE_MD,
    V34_TX_STAGE_SECOND_S,
    V34_TX_STAGE_SECOND_NOT_S,
    /*! \brief TRN is being transmitted */
    V34_TX_STAGE_TRN,
    /*! \brief J is being transmitted */
    V34_TX_STAGE_J,
    /*! \brief J' is being transmitted */
    V34_TX_STAGE_J_DASHED,
    /*! \brief MP is being transmitted */
    V34_TX_STAGE_MP,

    /*! \brief Half-duplex initial stages */
    V34_TX_STAGE_HDX_INITIAL_A,
    V34_TX_STAGE_HDX_FIRST_A,
    V34_TX_STAGE_HDX_FIRST_NOT_A,
    V34_TX_STAGE_HDX_FIRST_A_SILENCE,
    V34_TX_STAGE_HDX_SECOND_A,
    V34_TX_STAGE_HDX_SECOND_A_WAIT,

    V34_TX_STAGE_HDX_FIRST_B,
    V34_TX_STAGE_HDX_FIRST_B_INFO_SEEN,
    V34_TX_STAGE_HDX_FIRST_NOT_B_WAIT,
    V34_TX_STAGE_HDX_FIRST_NOT_B,
    V34_TX_STAGE_HDX_POST_L2_B,
    V34_TX_STAGE_HDX_POST_L2_SILENCE,

    /*! \brief Half-duplex control channel stages */
    /*! \brief Sh and !Sh are being transmitted */
    V34_TX_STAGE_HDX_SH,
    /*! \brief The first ALT is being transmitted */
    V34_TX_STAGE_HDX_FIRST_ALT,
    /*! \brief The PPh is being transmitted */
    V34_TX_STAGE_HDX_PPH,
    /*! \brief The second ALT is being transmitted */
    V34_TX_STAGE_HDX_SECOND_ALT,
    /*! \brief MPh is being transmitted */
    V34_TX_STAGE_HDX_MPH,
    /*! \brief E is being transmitted */
    V34_TX_STAGE_HDX_E
};

enum v34_events_e
{
    V34_EVENT_NONE = 0,
    V34_EVENT_TONE_SEEN,
    V34_EVENT_REVERSAL_1,
    V34_EVENT_REVERSAL_2,
    V34_EVENT_REVERSAL_3,
    V34_EVENT_INFO0_OK,
    V34_EVENT_INFO0_BAD,
    V34_EVENT_INFO1_OK,
    V34_EVENT_INFO1_BAD,
    V34_EVENT_INFOH_OK,
    V34_EVENT_INFOH_BAD,
    V34_EVENT_L2_SEEN,
    V34_EVENT_S
};

typedef struct
{
    bool support_baud_rate_low_carrier[6];
    bool support_baud_rate_high_carrier[6];
    bool support_power_reduction;
    uint8_t max_baud_rate_difference;
    bool support_1664_point_constellation;
    uint8_t tx_clock_source;
    bool from_cme_modem;
    bool rate_3429_allowed;
} v34_capabilities_t;

typedef struct
{
    bool use_high_carrier;
    int pre_emphasis;
    int max_bit_rate;
} info1c_baud_rate_parms_t;

typedef struct
{
    int power_reduction;
    int additional_power_reduction;
    int md;
    int freq_offset;
    info1c_baud_rate_parms_t rate_data[6];
} info1c_t;

typedef struct
{
    int power_reduction;
    int additional_power_reduction;
    int md;
    int freq_offset;
    bool use_high_carrier;
    int preemphasis_filter;
    int max_data_rate;
    int baud_rate_a_to_c;
    int baud_rate_c_to_a;
} info1a_t;

typedef struct
{
    int power_reduction;
    int length_of_trn;
    bool use_high_carrier;
    int preemphasis_filter;
    int baud_rate;
    bool trn16;
} infoh_t;

typedef struct
{
    int type;
    int bit_rate_a_to_c;
    int bit_rate_c_to_a;
    int aux_channel_supported;
    int trellis_size;
    bool use_non_linear_encoder;
    bool expanded_shaping;
    bool mp_acknowledged;
    int signalling_rate_mask;
    bool asymmetric_rates_allowed;
    /*! \brief Only in an MP1 message */
    complexi16_t precoder_coeffs[3];
} mp_t;

typedef struct
{
    int type;
    int max_data_rate;
    int control_channel_2400;
    int trellis_size;
    bool use_non_linear_encoder;
    bool expanded_shaping;
    int signalling_rate_mask;
    bool asymmetric_rates_allowed;
    /*! \brief Only in an MPH1 message */
    complexi16_t precoder_coeffs[3];
} mph_t;

/*! The set of working parameters, which defines operation at the current settings */
typedef struct
{
    /*! \brief The code (0-16) for the maximum bit rate */
    int max_bit_rate_code;
    /*! \brief Parameters for the current bit rate and baud rate */
    int bit_rate;
    /*! \brief Bits per high mapping frame. A low mapping frame is one bit less. */
    int b;
    int j;
    /*! \brief The number of shell mapped bits */
    int k;
    int l;
    int m;
    int p;
    /*! \brief The number of uncoded Q bits per 2D symbol */
    int q;
    int q_mask;
    /*! \brief Mapping frame switching parameter */
    int r;
    int w;
    /*! The numerator of the number of samples per symbol ratio. */
    int samples_per_symbol_numerator;
    /*! The denominator of the number of samples per symbol ratio. */
    int samples_per_symbol_denominator;
} v34_parameters_t;

typedef struct
{
    /*! \brief True if this is the calling side modem. */
    bool calling_party;
    /*! \brief True if this is a full duplex modem. */
    bool duplex;
    /*! The current source end when in half-duplex mode */
    bool half_duplex_source;
    /*! The current operating state when in half-duplex mode */
    bool half_duplex_state;
    /*! \brief */
    int bit_rate;
    /*! \brief The callback function used to get the next bit to be transmitted. */
    span_get_bit_func_t get_bit;
    /*! \brief A user specified opaque pointer passed to the get_bit function. */
    void *get_bit_user_data;

    /*! \brief The callback function used to get the next aux channel bit to be transmitted. */
    span_get_bit_func_t get_aux_bit;
    /*! \brief A user specified opaque pointer passed to the get_aux_bit function. */
    void *get_aux_bit_user_data;

    /*! \brief The current baud rate selection, as a value from 0 to 5. */
    int baud_rate;
    /*! \brief True if using the higher of the two carrier frequency options. */
    bool high_carrier;

    /*! \brief The register for the data scrambler. */
    uint32_t scramble_reg;
    /*! \brief The scrambler tap which selects between the caller and answerer scramblers */
    int scrambler_tap;

    bool use_non_linear_encoder;

#if defined(SPANDSP_USE_FIXED_POINT)
    complexi16_t (*current_getbaud)(v34_state_t *s);
#else
    complexf_t (*current_getbaud)(v34_state_t *s);
#endif

    /*! \brief Mapping frame parsed input */
    uint32_t r0;
    uint16_t qbits[8];
    uint16_t ibits[4];

    /*! \brief (x0,y0) (x1,y1)... */
    int mjk[8];

    int step_2d;

    bitstream_state_t bs;
    uint32_t bitstream;

    int i;

    /*! \brief Parameters for the current bit rate and baud rate */
    v34_parameters_t parms;

    /*! \brief We need to remember some old x values
               in the C code:  x[0]  x[1]  x[2]  x[3] x[4] x[5] x[6] x[7] x[8] x[9] x[10]
               in V.34:        x[-3] x[-2] x[-1] x[0] x[1] x[2] x[3] x[4] x[5] x[6] x[7] */
    complexi16_t x[8 + V34_XOFF];
    /*! \brief Precoder coefficients */
    complexi16_t precoder_coeffs[3];

    complexi16_t c;
    complexi16_t p;
    int z;
    int y0;
    int state;

#if defined(SPANDSP_USE_FIXED_POINT)
    int16_t gain;
#else
    float gain;
#endif

#if defined(SPANDSP_USE_FIXED_POINT)
    /*! \brief The root raised cosine (RRC) pulse shaping filter buffer. */
    int16_t rrc_filter_re[V34_INFO_TX_FILTER_STEPS];
    int16_t rrc_filter_im[V34_INFO_TX_FILTER_STEPS];
    complexi16_t lastbit;
#else
    /*! \brief The root raised cosine (RRC) pulse shaping filter buffer. */
    float rrc_filter_re[V34_INFO_TX_FILTER_STEPS];
    float rrc_filter_im[V34_INFO_TX_FILTER_STEPS];
    complexf_t lastbit;
#endif
    /*! \brief Current offset into the RRC pulse shaping filter buffer. */
    int rrc_filter_step;

    /*! \brief The current phase of the carrier (i.e. the DDS parameter). */
    uint32_t carrier_phase;
    /*! \brief The update rate for the phase of the control channel carrier (i.e. the DDS increment). */
    int32_t cc_carrier_phase_rate;
    /*! \brief The update rate for the phase of the V.34 carrier (i.e. the DDS increment). */
    int32_t v34_carrier_phase_rate;

    /*! \brief The current phase of the guard tone (i.e. the DDS parameter). */
    uint32_t guard_phase;
    /*! \brief The update rate for the phase of the guard tone (i.e. the DDS increment). */
    int32_t guard_phase_rate;
    /*! \brief Guard tone signal level. */
    float guard_level;
    /*! \brief The current fractional phase of the baud timing. */
    int baud_phase;

    int stage;
    int convolution;
    int training_stage;
    int current_modulator;
    int diff;

    int line_probe_cycles;
    int line_probe_step;
    float line_probe_scaling;
    int tone_duration;

    int super_frame;
    int data_frame;
    int s_bit_cnt;
    int aux_bit_cnt;

    uint16_t v0_pattern;

    uint8_t txbuf[50];
    int txbits;
    int txptr;
    const conv_encode_table_t *conv_encode_table;

    bool info0_acknowledgement;

    union
    {
        info1a_t info1a;
        info1c_t info1c;
        infoh_t infoh;
    };
    union
    {
        mp_t mp;
        mph_t mph;
    };

    int persistence2;

    /*! \brief The get_bit function in use at any instant. */
    span_get_bit_func_t current_get_bit;

    /*! \brief Used to align the transmit and receive positions, to ensure things like
               round trip delay are properly handled. */
    span_sample_timer_t sample_time;

    logging_state_t *logging;
} v34_tx_state_t;

typedef struct
{
#if defined(SPANDSP_USE_FIXED_POINT)
    /*! \brief Low band edge filter for symbol sync. */
    int32_t symbol_sync_low[2];
    /*! \brief High band edge filter for symbol sync. */
    int32_t symbol_sync_high[2];
    /*! \brief DC filter for symbol sync. */
    int32_t symbol_sync_dc_filter[2];
    /*! \brief Baud phase for symbol sync. */
    int32_t baud_phase;
    
    /*! \brief Low band edge filter coefficients for symbol sync. */
    int32_t low_band_edge_coeff[3];
    /*! \brief High band edge filter coefficients for symbol sync. */
    int32_t high_band_edge_coeff[3];
    /*! \brief A coefficient common to the low and high band edges */
    int32_t mixed_edges_coeff_3;
#else
    /*! \brief Low band edge filter for symbol sync. */
    float symbol_sync_low[2];
    /*! \brief High band edge filter for symbol sync. */
    float symbol_sync_high[2];
    /*! \brief DC filter for symbol sync. */
    float symbol_sync_dc_filter[2];
    /*! \brief Baud phase for symbol sync. */
    float baud_phase;

    /*! \brief Low band edge filter coefficients for symbol sync. */
    float low_band_edge_coeff[3];
    /*! \brief High band edge filter coefficients for symbol sync. */
    float high_band_edge_coeff[3];
    /*! \brief A coefficient common to the low and high band edges */
    float mixed_edges_coeff_3;
#endif
} ted_t;

typedef struct
{
    /*! \brief Viterbi trellis state table
               16 4D symbols deep, with 16 states each
               Each state has 4 entries: cumulative path metric, and prev. path pointer, x, y
               circularly addressed */
    struct
    {
        /*! \brief Cumulative path metric */
        uint32_t cumulative_path_metric[16];
        /*! \brief Previous path pointer */
        uint16_t previous_path_ptr[16];
        uint16_t pts[16];
        uint16_t branch_error_x[8];
        /*! \brief Branches of the x and y coords of the points in the eight 4D subsets
                   to which a sequence of 2D points has been sliced.
                   indexed from 0 to 15 --> 8 points for 16 past 4D symbols */
        complexi16_t bb[2][8];
    } vit[16];
    /*! \brief Latest viterbi table slot. */
    int ptr;
    /*! \brief Countdown to the first data being available from the viterbi pipeline */
    int windup;
    int16_t curr_min_state;

    int16_t error[2][4];

    /*! \brief Eight 4D squared branch errors for each of 8 4D subsets.
               Indexed array for indexing from viterbi lookup table */
    uint16_t branch_error[8];

    const conv_decode_table_t *conv_decode_table;
} viterbi_t;

typedef struct
{
    /*! \brief True if this is the calling side modem. */
    bool calling_party;
    /*! \brief True if this is a full duplex modem. */
    bool duplex;
    /*! The current source end when in half-duplex mode */
    bool half_duplex_source;
    /*! The current operating state when in half-duplex mode */
    bool half_duplex_state;
    /*! \brief */
    int bit_rate;
    /*! \brief The callback function used to put each bit received. */
    span_put_bit_func_t put_bit;
    /*! \brief A user specified opaque pointer passed to the put_bit routine. */
    void *put_bit_user_data;

    /*! \brief The callback function used to put each aux bit received. */
    span_put_bit_func_t put_aux_bit;
    /*! \brief A user specified opaque pointer passed to the put_aux_bit routine. */
    void *put_aux_bit_user_data;

    /*! \brief A callback function which may be enabled to report every symbol's
               constellation position. */
    qam_report_handler_t qam_report;
    /*! \brief A user specified opaque pointer passed to the qam_report callback
               routine. */
    void *qam_user_data;

    /*! \brief The current baud rate selection, as a value from 0 to 5. */
    int baud_rate;
    /*! \brief True if using the higher of the two carrier frequency options. */
    bool high_carrier;

    int stage;
    int received_event;

    /*! \brief The register for the data scrambler. */
    uint32_t scramble_reg;
    /*! \brief The scrambler tap which selects between the caller and answerer scramblers */
    int scrambler_tap;

    uint16_t v0_pattern;

    /*! \brief A power meter, to measure the HPF'ed signal power in the channel. */
    power_meter_t power;
    /*! \brief The power meter level at which carrier on is declared. */
    int32_t carrier_on_power;
    /*! \brief The power meter level at which carrier off is declared. */
    int32_t carrier_off_power;
    bool signal_present;

    bitstream_state_t bs;
    uint32_t bitstream;

    /*! \brief Mapping frame output */
    uint32_t r0;
    uint16_t qbits[8];
    uint16_t ibits[4];

    /*! \brief (x0,y0) (x1,y1)... */
    int mjk[8];

    int step_2d;

    /*! \brief Parameters for the current bit rate and baud rate */
    v34_parameters_t parms;

    /*! \brief yt's are the noise corrupted points fed to the viterbi decoder.
               Assumed to have format 9:7 (7 fractional bits) */
    complexi16_t yt;
    complexi16_t xt[4];
  
    complexi16_t x[3];
    complexi16_t h[3];

    /*! \brief These are quantized points in the respective 2D coset (0,1,2,3) */
    complexi16_t xy[2][4];

    viterbi_t viterbi;

    /*! \brief ww contains old z, current z and current w */
    int16_t ww[3];

    /*! \brief The current phase of the carrier (i.e. the DDS parameter). */
    uint32_t carrier_phase;
    /*! \brief The carrier update rate saved for reuse when using short training. */
    int32_t carrier_phase_rate_save;

    /*! \brief The update rate for the phase of the control channel carrier (i.e. the DDS increment). */
    int32_t cc_carrier_phase_rate;
    /*! \brief The update rate for the phase of the V.34 carrier (i.e. the DDS increment). */
    int32_t v34_carrier_phase_rate;

    /*! \brief The root raised cosine (RRC) pulse shaping filter buffer. */
#if defined(SPANDSP_USE_FIXED_POINT)
    int16_t rrc_filter[V34_RX_FILTER_STEPS];
#else
    float rrc_filter[V34_RX_FILTER_STEPS];
#endif
    /*! \brief Current offset into the RRC pulse shaping filter buffer. */
    int rrc_filter_step;
    /*! \brief Current read offset into the equalizer buffer. */
    int eq_step;
    /*! \brief Current write offset into the equalizer buffer. */
    int eq_put_step;
    int shaper_sets;

#if defined(SPANDSP_USE_FIXED_POINT)
    /*! \brief The scaling factor assessed by the AGC algorithm. */
    int16_t agc_scaling;
    /*! \brief The previous value of agc_scaling, needed to reuse old training. */
    int16_t agc_scaling_save;
#else
    /*! \brief The scaling factor assessed by the AGC algorithm. */
    float agc_scaling;
    /*! \brief The previous value of agc_scaling, needed to reuse old training. */
    float agc_scaling_save;
#endif
    ted_t pri_ted;
    ted_t cc_ted;

#if defined(SPANDSP_USE_FIXED_POINT)
    /*! \brief The proportional part of the carrier tracking filter. */
    float carrier_track_p;
    /*! \brief The integral part of the carrier tracking filter. */
    float carrier_track_i;
#else
    /*! \brief The proportional part of the carrier tracking filter. */
    float carrier_track_p;
    /*! \brief The integral part of the carrier tracking filter. */
    float carrier_track_i;
#endif

    const v34_rx_shaper_t *shaper_re;
    const v34_rx_shaper_t *shaper_im;

    /*! \brief The total symbol timing correction since the carrier came up.
               This is only for performance analysis purposes. */
    int total_baud_timing_correction;

    /*! \brief The current half of the baud. */
    int baud_half;
    /*! \brief The measured round trip delay estimate, in sample times */
    int round_trip_delay_estimate;

    int duration;
    int bit_count;
    int target_bits;
    uint16_t crc;
    uint32_t last_angles[2];

    /*! \brief Buffer for receiving info frames. */
    uint8_t info_buf[25];

    int super_frame;
    int data_frame;
    int s_bit_cnt;
    int aux_bit_cnt;

    uint8_t rxbuf[50];
    int rxbits;
    int rxptr;

    int blip_duration;

    v34_capabilities_t far_capabilities;

    /*! \brief Whether or not a carrier drop was detected and the signal delivery is pending. */
    int carrier_drop_pending;
    /*! \brief A count of the current consecutive samples below the carrier off threshold. */
    int low_samples;
    /*! \brief A highest magnitude sample seen. */
    int16_t high_sample;

    bool info0_acknowledgement;

    union
    {
        info1a_t info1a;
        info1c_t info1c;
        infoh_t infoh;
    };

    int step;
    int persistence1;
    int persistence2;

    /* MP or MPh receive tracking data */
    int mp_count;
    int mp_len;
    int mp_and_fill_len;
    int mp_seen;

    int dft_ptr;
#if defined(SPANDSP_USE_FIXED_POINT)
    int16_t dft_buffer[160];
    int32_t l1_l2_gains[25];
    int32_t l1_l2_phases[25];
    int32_t base_phase;
    complexf_t last_sample;
    #else
    complexf_t dft_buffer[160];
    float l1_l2_gains[25];
    float l1_l2_phases[25];
    float base_phase;
    complexf_t last_sample;
#endif
    int l1_l2_duration;

    int current_demodulator;

    /*! \brief Used to align the transmit and receive positions, to ensure things like
               round trip delay are properly handled. */
    span_sample_timer_t sample_time;

    span_sample_timer_t tone_ab_hop_time;

    logging_state_t *logging;
} v34_rx_state_t;

/*!
    V.34 modem descriptor. This defines the working state for a single instance
    of a V.34 modem.
*/
struct v34_state_s
{
    /*! \brief True if this is the calling side modem. */
    bool calling_party;
    /*! \brief True if this is a full duplex modem. */
    bool duplex;
    /*! The current source end when in half-duplex mode */
    bool half_duplex_source;
    /*! The current operating state when in half-duplex mode */
    bool half_duplex_state;
    /*! \brief The bit rate of the modem. */
    int bit_rate;

    v34_tx_state_t tx;
    v34_rx_state_t rx;
    modem_echo_can_state_t *ec;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

#endif
/*- End of file ------------------------------------------------------------*/
