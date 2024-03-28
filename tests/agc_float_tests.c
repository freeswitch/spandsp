/*
 * SpanDSP - a series of DSP components for telephony
 *
 * agc_float_tests.c - tests for floating point AGC for modems.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2024 Steve Underwood
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

#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sndfile.h>

#include "spandsp.h"
#include "spandsp-sim.h"

#define OUT_FILE_NAME   "agc_float.wav"

int main(int argc, char *argv[])
{
    agcf_state_t *agc;
    agcf_descriptor_t *desc;
    power_meter_t pre_meter;
    power_meter_t post_meter;
    int32_t phase_rate;
    uint32_t phase_acc;
    float fbuf[160];
    int16_t buf[160];
    int16_t audio[2*160];
    int32_t pre_level;
    int32_t post_level;
    float scale;
    int i;
    int j;
    bool signal_present;
    float target_level;
    float signal_level;
    SNDFILE *outhandle;
    int outframes;

    if ((outhandle = sf_open_telephony_write(OUT_FILE_NAME, 2)) == NULL)
    {
        fprintf(stderr, "    Cannot create audio file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    /*endif*/

    phase_rate = dds_phase_rate(768.0f);
    phase_acc = 0;

    /* AGC tests from an int16_t signal. */
    for (target_level = -50.0f;  target_level < 3.0f;  target_level += 1.0f)
    {
        for (signal_level = -50.0f;  signal_level < 3.0f;  signal_level += 1.0f)
        {
            power_meter_init(&pre_meter, 5);
            power_meter_init(&post_meter, 5);
            desc = agcf_make_descriptor(NULL, target_level, -45.0f, -48.0f, 5, 5);
            agc = agcf_init(NULL, desc);
            agcf_free_descriptor(desc);
            scale = dds_scaling_dbm0(signal_level);

            for (j = 0;  j < 5;  j++)
            {
                for (i = 0;  i < 160;  i++)
                {
                    buf[i] = dds_mod(&phase_acc, phase_rate, scale, 0);
                    audio[2*i] = buf[i];
                    pre_level = power_meter_update(&pre_meter, buf[i]);
                }
                /*endfor*/
                signal_present = agcf_from_int16_rx(agc, fbuf, buf, 160);
                if (j > 0)
                {
                    if (signal_level > -45.0f)
                    {
                        if (!signal_present)
                        {
                            printf("Signal not present at %fdBm0\n", signal_level);
                            exit(2);
                        }
                        /*endif*/
                    }
                    else
                    {
                        if (signal_present)
                        {
                            printf("Signal present at %fdBm0\n", signal_level);
                            exit(2);
                        }
                        /*endif*/
                    }
                    /*endif*/
                }
                /*endif*/
                for (i = 0;  i < 160;  i++)
                {
                    audio[2*i + 1] = fbuf[i];
                    post_level = power_meter_update(&post_meter, fbuf[i]);
                }
                /*endfor*/
                outframes = sf_writef_short(outhandle, audio, 160);
                if (outframes != 160)
                {
                    fprintf(stderr, "    Error writing audio file\n");
                    exit(2);
                }
                /*endif*/
            }
            /*endfor*/
            if (signal_present  && fabsf(power_meter_current_dbm0(&post_meter) - target_level) > 0.3f)
            {
                printf("Pre %fdBm0, post %fdBm0, target %fdBm0, current %fdBm0, gain %f\n", power_meter_current_dbm0(&pre_meter), power_meter_current_dbm0(&post_meter), target_level, agcf_current_power_dbm0(agc), agcf_get_scaling(agc));
                exit(2);
            }
            /*endif*/
        }
        /*endfor*/
    }
    /*endfor*/

    /* Grow a signal until it is detected, and then shrink it until it is not detected, to check
       the detection hysteresis */
    power_meter_init(&pre_meter, 5);
    power_meter_init(&post_meter, 5);
    desc = agcf_make_descriptor(NULL, target_level, -45.0f, -48.0f, 5, 5);
    agc = agcf_init(NULL, desc);
    agcf_free_descriptor(desc);
    for (signal_level = -55.0f;  signal_level < -30.0f;  signal_level += 1.0f)
    {
        scale = dds_scaling_dbm0(signal_level);

        for (j = 0;  j < 5;  j++)
        {
            for (i = 0;  i < 160;  i++)
            {
                buf[i] = dds_mod(&phase_acc, phase_rate, scale, 0);
                audio[2*i] = buf[i];
                pre_level = power_meter_update(&pre_meter, buf[i]);
            }
            /*endfor*/
            signal_present = agcf_from_int16_rx(agc, fbuf, buf, 160);
            if (j > 0)
            {
                if (signal_level > -45.0f)
                {
                    if (!signal_present)
                    {
                        printf("At %fdBm0 signal is not present\n", signal_level);
                        exit(2);
                    }
                    /*endif*/
                }
                else
                {
                    if (signal_present)
                    {
                        printf("At %fdBm0 signal is present\n", signal_level);
                        exit(2);
                    }
                    /*endif*/
                }
                /*endif*/
            }
            /*endif*/
            for (i = 0;  i < 160;  i++)
            {
                audio[2*i + 1] = fbuf[i];
                post_level = power_meter_update(&post_meter, fbuf[i]);
            }
            /*endfor*/
            outframes = sf_writef_short(outhandle, audio, 160);
            if (outframes != 160)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
            /*endif*/
        }
        /*endfor*/
    }
    /*endfor*/
    for (signal_level = -30.0f;  signal_level > -55.0f;  signal_level -= 1.0f)
    {
        scale = dds_scaling_dbm0(signal_level);

        for (j = 0;  j < 5;  j++)
        {
            for (i = 0;  i < 160;  i++)
            {
                buf[i] = dds_mod(&phase_acc, phase_rate, scale, 0);
                audio[2*i] = buf[i];
                pre_level = power_meter_update(&pre_meter, buf[i]);
            }
            /*endfor*/
            signal_present = agcf_from_int16_rx(agc, fbuf, buf, 160);
            if (j > 0)
            {
                if (signal_level < -48.0f)
                {
                    if (signal_present)
                    {
                        printf("At %fdBm0 signal is present\n", signal_level);
                        exit(2);
                    }
                    /*endif*/
                }
                else
                {
                    if (!signal_present)
                    {
                        printf("At %fdBm0 signal is not present\n", signal_level);
                        exit(2);
                    }
                    /*endif*/
                }
                /*endif*/
            }
            /*endif*/
            for (i = 0;  i < 160;  i++)
            {
                audio[2*i + 1] = fbuf[i];
                post_level = power_meter_update(&post_meter, fbuf[i]);
            }
            /*endfor*/
            outframes = sf_writef_short(outhandle, audio, 160);
            if (outframes != 160)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
            /*endif*/
        }
        /*endfor*/
    }
    /*endfor*/

    /* AGC tests from a floating point signal. */
    for (target_level = -50.0f;  target_level < 3.0f;  target_level += 1.0f)
    {
        for (signal_level = -50.0f;  signal_level < 3.0f;  signal_level += 1.0f)
        {
            power_meter_init(&pre_meter, 5);
            power_meter_init(&post_meter, 5);
            desc = agcf_make_descriptor(NULL, target_level, -45.0f, -48.0f, 5, 5);
            agc = agcf_init(NULL, desc);
            agcf_free_descriptor(desc);
            scale = dds_scaling_dbm0(signal_level);

            for (j = 0;  j < 5;  j++)
            {
                for (i = 0;  i < 160;  i++)
                {
                    fbuf[i] = dds_modf(&phase_acc, phase_rate, scale, 0);
                    audio[2*i] = fbuf[i];
                    pre_level = power_meter_update(&pre_meter, fbuf[i]);
                }
                /*endfor*/
                signal_present = agcf_rx(agc, fbuf, fbuf, 160);
                if (j > 0)
                {
                    if (signal_level > -45.0f)
                    {
                        if (!signal_present)
                        {
                            printf("Signal not present at %fdBm0\n", signal_level);
                            exit(2);
                        }
                        /*endif*/
                    }
                    else
                    {
                        if (signal_present)
                        {
                            printf("Signal present at %fdBm0\n", signal_level);
                            exit(2);
                        }
                        /*endif*/
                    }
                    /*endif*/
                }
                /*endif*/
                for (i = 0;  i < 160;  i++)
                {
                    audio[2*i + 1] = fbuf[i];
                    post_level = power_meter_update(&post_meter, fbuf[i]);
                }
                /*endfor*/
                outframes = sf_writef_short(outhandle, audio, 160);
                if (outframes != 160)
                {
                    fprintf(stderr, "    Error writing audio file\n");
                    exit(2);
                }
                /*endif*/
            }
            /*endfor*/
            if (signal_present  && fabsf(power_meter_current_dbm0(&post_meter) - target_level) > 0.3f)
            {
                printf("Pre %fdBm0, post %fdBm0, target %fdBm0, current %fdBm0, gain %f\n", power_meter_current_dbm0(&pre_meter), power_meter_current_dbm0(&post_meter), target_level, agcf_current_power_dbm0(agc), agcf_get_scaling(agc));
                exit(2);
            }
            /*endif*/
        }
        /*endfor*/
    }
    /*endfor*/

    if (sf_close_telephony(outhandle))
    {
        fprintf(stderr, "    Cannot close audio file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    /*endif*/

    printf("Tests passed.\n");
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
