/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/v29rx.h - ITU V.29 modem receive part
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

#if !defined(_SPANDSP_PRIVATE_V29RX_H_)
#define _SPANDSP_PRIVATE_V29RX_H_

/* Target length for the equalizer is about 63 taps, to deal with the worst stuff
   in V.56bis. */
/*! The length of the equalizer buffer */
#define V29_EQUALIZER_LEN       33
/*! Samples before the target position in the equalizer buffer */
#define V29_EQUALIZER_PRE_LEN   16

/*! The number of taps in the pulse shaping/bandpass filter */
#define V29_RX_FILTER_STEPS     27

/*!
    V.29 modem receive side descriptor. This defines the working state for a
    single instance of a V.29 modem receiver.
*/
struct v29_rx_state_s
{
    /*! \brief The bit rate of the modem. Valid values are 4800, 7200 and 9600. */
    int bit_rate;
    /*! \brief The callback function used to put each bit received. */
    put_bit_func_t put_bit;
    /*! \brief A user specified opaque pointer passed to the put_bit routine. */
    void *put_bit_user_data;

    /*! \brief The callback function used to report modem status changes. */
    modem_status_func_t status_handler;
    /*! \brief A user specified opaque pointer passed to the status function. */
    void *status_user_data;

    /*! \brief A callback function which may be enabled to report every symbol's
               constellation position. */
    qam_report_handler_t qam_report;
    /*! \brief A user specified opaque pointer passed to the qam_report callback
               routine. */
    void *qam_user_data;

#if defined(SPANDSP_USE_FIXED_POINT)
    /*! \brief The scaling factor assessed by the AGC algorithm. */
    int16_t agc_scaling;
    /*! \brief The previous value of agc_scaling, needed to reuse old training. */
    int16_t agc_scaling_save;

    /*! \brief The current delta factor for updating the equalizer coefficients. */
    int16_t eq_delta;
    /*! \brief The adaptive equalizer coefficients. */
    complexi16_t eq_coeff[V29_EQUALIZER_LEN];
    /*! \brief A saved set of adaptive equalizer coefficients for use after restarts. */
    complexi16_t eq_coeff_save[V29_EQUALIZER_LEN];
    /*! \brief The equalizer signal buffer. */
    complexi16_t eq_buf[V29_EQUALIZER_LEN];

    /*! Low band edge filter for symbol sync. */
    int32_t symbol_sync_low[2];
    /*! High band edge filter for symbol sync. */
    int32_t symbol_sync_high[2];
    /*! DC filter for symbol sync. */
    int32_t symbol_sync_dc_filter[2];
    /*! Baud phase for symbol sync. */
    int32_t baud_phase;

    /*! \brief A measure of how much mismatch there is between the real constellation,
               and the decoded symbol positions. */
    int32_t training_error;

    /*! \brief The proportional part of the carrier tracking filter. */
    int32_t carrier_track_p;
    /*! \brief The integral part of the carrier tracking filter. */
    int32_t carrier_track_i;
    /*! \brief The root raised cosine (RRC) pulse shaping filter buffer. */
    int16_t rrc_filter[V29_RX_FILTER_STEPS];
#else
    /*! \brief The scaling factor assessed by the AGC algorithm. */
    float agc_scaling;
    /*! \brief The previous value of agc_scaling, needed to reuse old training. */
    float agc_scaling_save;

    /*! \brief The current delta factor for updating the equalizer coefficients. */
    float eq_delta;
    /*! \brief The adaptive equalizer coefficients. */
    complexf_t eq_coeff[V29_EQUALIZER_LEN];
    /*! \brief A saved set of adaptive equalizer coefficients for use after restarts. */
    complexf_t eq_coeff_save[V29_EQUALIZER_LEN];
    /*! \brief The equalizer signal buffer. */
    complexf_t eq_buf[V29_EQUALIZER_LEN];

    /*! Low band edge filter for symbol sync. */
    float symbol_sync_low[2];
    /*! High band edge filter for symbol sync. */
    float symbol_sync_high[2];
    /*! DC filter for symbol sync. */
    float symbol_sync_dc_filter[2];
    /*! Baud phase for symbol sync. */
    float baud_phase;

    /*! \brief A measure of how much mismatch there is between the real constellation,
               and the decoded symbol positions. */
    float training_error;

    /*! \brief The proportional part of the carrier tracking filter. */
    float carrier_track_p;
    /*! \brief The integral part of the carrier tracking filter. */
    float carrier_track_i;
    /*! \brief The root raised cosine (RRC) pulse shaping filter buffer. */
    float rrc_filter[V29_RX_FILTER_STEPS];
#endif
    /*! \brief Current offset into the RRC pulse shaping filter buffer. */
    int rrc_filter_step;

    /*! \brief The register for the data scrambler. */
    uint32_t scramble_reg;
    /*! \brief The register for the training scrambler. */
    uint8_t training_scramble_reg;
    /*! \brief The current step in the table of CD constellation positions. */
    int training_cd;
    /*! \brief True if the previous trained values are to be reused. */
    bool old_train;
    /*! \brief The section of the training data we are currently in. */
    int training_stage;
    /*! \brief A count of how far through the current training step we are. */
    int training_count;
    /*! \brief The value of the last signal sample, using the a simple HPF for signal power estimation. */
    int16_t last_sample;
    /*! \brief >0 if a signal above the minimum is present. It may or may not be a V.29 signal. */
    int signal_present;
    /*! \brief Whether or not a carrier drop was detected and the signal delivery is pending. */
    int carrier_drop_pending;
    /*! \brief A count of the current consecutive samples below the carrier off threshold. */
    int low_samples;
    /*! \brief A highest magnitude sample seen. */
    int16_t high_sample;

    /*! \brief The current phase of the carrier (i.e. the DDS parameter). */
    uint32_t carrier_phase;
    /*! \brief The update rate for the phase of the carrier (i.e. the DDS increment). */
    int32_t carrier_phase_rate;
    /*! \brief The carrier update rate saved for reuse when using short training. */
    int32_t carrier_phase_rate_save;

    /*! \brief A power meter, to measure the HPF'ed signal power in the channel. */
    power_meter_t power;
    /*! \brief The power meter level at which carrier on is declared. */
    int32_t carrier_on_power;
    /*! \brief The power meter level at which carrier off is declared. */
    int32_t carrier_off_power;

    /*! \brief Current read offset into the equalizer buffer. */
    int eq_step;
    /*! \brief Current write offset into the equalizer buffer. */
    int eq_put_step;
    /*! \brief Symbol counter to the next equalizer update. */
    int eq_skip;

    /*! \brief The current half of the baud. */
    int baud_half;

    /*! \brief The total symbol timing correction since the carrier came up.
               This is only for performance analysis purposes. */
    int total_baud_timing_correction;

    /*! \brief The previous symbol phase angles for the coarse carrier aquisition step. */
    int32_t last_angles[2];
    /*! \brief History list of phase angle differences for the coarse carrier aquisition step. */
    int32_t diff_angles[16];

    /*! \brief The position of the current symbol in the constellation, used for
               differential decoding. */
    int constellation_state;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

#endif
/*- End of file ------------------------------------------------------------*/
