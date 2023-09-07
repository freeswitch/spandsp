/*
 * SpanDSP - a series of DSP components for telephony
 *
 * make_v34_probe_signals.c - ITU V.34 modem shell map generation.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2009 Steve Underwood
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

#include <inttypes.h>
#include <stdio.h>
#include "spandsp.h"

#define SAMPLE_RATE 8000

static void make_line_probe_table(void)
{
    static const struct
    {
        int freq;
        int phase;
    } line_probe_ref[21 + 1] =
    {
        { 150,   0},
        { 300, 180},
        { 450,   0},
        { 600,   0},
        { 750,   0},
        {1050,   0},
        {1350,   0},
        {1500,   0},
        {1650, 180},
        {1950,   0},
        {2100,   0},
        {2250, 180},
        {2550,   0},
        {2700, 180},
        {2850,   0},
        {3000, 180},
        {3150, 180},
        {3300, 180},
        {3450, 180},
        {3600,   0},
        {3750,   0},
        {0,      0}
    };
    static const uint8_t alaw_0db[] =
    {
        0x34, 0x21, 0x21, 0x34, 0xB4, 0xA1, 0xA1, 0xB4
    };
    struct
    {
        int32_t phase_rate;
        uint32_t starting_phase;
        uint32_t phase;
    } line_probe[21];
    int16_t amp[160];
    int i;
    int j;
    g711_state_t *g711;
    double energy1;
    double energy2;
    double scaling;
    double x;
    double max;

    printf("static const struct\n");
    printf("{\n");
    printf("    int32_t phase_rate;\n");
    printf("    int32_t starting_phase;\n");
    printf("} line_probe[] =\n");
    printf("{\n");
    for (i = 0;  line_probe_ref[i].freq;  i++)
    {
        line_probe[i].phase_rate = (int32_t) ((double) line_probe_ref[i].freq*65536.0*65536.0/SAMPLE_RATE);
        line_probe[i].starting_phase =
        line_probe[i].phase =  (line_probe_ref[i].phase == 0)  ?  0  :  0x80000000;
        printf("    {0x%08X, 0x%08X}",
               line_probe[i].phase_rate,
               line_probe[i].starting_phase);
        if (line_probe_ref[i + 1].freq)
            printf(",\n");
        else
            printf("\n");
        /*endif*/
    }
    /*endfor*/
    printf("};\n");
    printf("\n");

    /* This signal repeats every 160 samples. Create one block of the
       signal, scaled to peak at 32767 */
    max = 0.0;
    energy1 = 0.0;
    for (j = 0;  j < 160;  j++)
    {
        x = 0.0;
        for (i = 0;  i < 21;  i++)
        {
            x += cos(j*2.0*3.1415926535*line_probe_ref[i].freq/8000.0 + 3.1415926535*line_probe_ref[i].phase/180.0);
        }
        /*endfor*/
        energy1 += x*x;
        if (fabs(x) > max)
            max = fabs(x);
        /*endif*/
    }
    /*endfor*/
    /* Find the reference energy for 0dBm0, so we can scale to the same energy */
    g711 = g711_init(NULL, G711_ALAW);
    energy2 = 0.0;
    g711_decode(g711, amp, alaw_0db, 8);
    for (j = 0;  j < 8;  j++)
        energy2 += (double) amp[j]*(double) amp[j];
    /*endfor*/
    energy2 *= 160.0/8.0;
    scaling = sqrt(energy2/energy1);
    /* Check that we don't have a crest factor issue at 0dBm0 */
    if (scaling*max > 32767.0)
    {
        printf("Oops. Scaling to 0dBm0 will clip.\n");
        exit(2);
    }
    /*endif*/
    printf("#define LINE_PROBE_SAMPLES 160\n");
    printf("\n");
    printf("#if defined(SPANDSP_USE_FIXED_POINTx)\n");
    printf("static const int16_t line_probe_samples[LINE_PROBE_SAMPLES] =\n");
    printf("#else\n");
    printf("static const float line_probe_samples[LINE_PROBE_SAMPLES] =\n");
    printf("#endif\n");
    printf("{\n");
    for (j = 0;  j < 160;  j++)
    {
        x = 0.0;
        for (i = 0;  i < 21;  i++)
            x += cos(j*2.0*3.1415926535*line_probe_ref[i].freq/8000.0 + 3.1415926535*line_probe_ref[i].phase/180.0);
        /*endfor*/
        printf("    LINE_PROBE_SCALE(%9.2ff)", x*scaling);
        if (j < 159)
            printf(",");
        /*endif*/
        printf("\n");
    }
    /*endfor*/
    printf("};\n");
}
/*- End of function --------------------------------------------------------*/

static void make_pp_signal(void)
{
    int i;
    int k;
    int I;
    double re;
    double im;
    double theta;
    double gain;
    double kx;

    printf("/* The 48 symbol PP signal, which is repeated 6 times, to make a 288 symbol sequence */\n");
    printf("/* See V.34/10.1.3.5 */\n");
    printf("#define PP_REPEATS 6\n");
    printf("#define PP_SYMBOLS (8*PP_REPEATS)\n");
    printf("\n");
    gain = 1.0f;
    printf("#if defined(SPANDSP_USE_FIXED_POINTx)\n");
    printf("static const complexi16_t pp_symbols[PP_SYMBOLS] =\n");
    printf("#else\n");
    printf("static const complexf_t pp_symbols[PP_SYMBOLS] =\n");
    printf("#endif\n");
    printf("{\n");
    for (i = 0;  i < 48;  i++)
    {
        k = i/4;
        I = i%4;
        kx = (k%3 == 1)  ?  4.0  :  0.0;
        theta = 3.1415926535*(k*I + kx)/6.0;
        re = cos(theta)*gain;
        im = sin(theta)*gain;
        printf("    {PP_SYMBOL_SCALE(%10.7ff), PP_SYMBOL_SCALE(%10.7ff)}", re, im);
        if (i < 47)
            printf(",");
        /*endif*/
        printf("\n");
    }
    /*endfor*/
    printf("};\n");
}
/*- End of function --------------------------------------------------------*/

static void make_pph_signal(void)
{
    int i;
    int k;
    int I;
    double re;
    double im;
    double theta;
    double gain;

    /* NB: There seems to be a misprint in V.34. Section 10.2.4.5 says the sequence for PPh is
           e^j*pi*[(2k(k-1)+1)/4]
        but really seems to mean
           e^j*pi*[(2k(k-I)+1)/4]
     */
    printf("/* The 8 symbol PPh signal, which is repeated 4 times, to make a 32 symbol sequence */\n");
    printf("/* See V.34/10.2.4.5 */\n");
    printf("#define PPH_REPEATS 4\n");
    printf("#define PPH_SYMBOLS (8*PPH_REPEATS)\n");
    printf("\n");
    gain = 1.0f;
    printf("#if defined(SPANDSP_USE_FIXED_POINTx)\n");
    printf("static const complexi16_t pph_symbols[PPH_SYMBOLS] =\n");
    printf("#else\n");
    printf("static const complexf_t pph_symbols[PPH_SYMBOLS] =\n");
    printf("#endif\n");
    printf("{\n");
    for (i = 0;  i < 32;  i++)
    {
        k = i/2;
        I = i%2;
        theta = 3.1415926535*(2.0*k*(k - I) + 1)/4.0;
        re = cos(theta)*gain;
        im = sin(theta)*gain;
        printf("    {PP_SYMBOL_SCALE(%10.7ff), PP_SYMBOL_SCALE(%10.7ff)}", re, im);
        if (i < 47)
            printf(",");
        /*endif*/
        printf("   /* %3.0f degrees */\n", fmod(theta*180.0/3.1415926535, 360.0));
    }
    /*endfor*/
    printf("};\n");
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    printf("/* THIS FILE WAS AUTOMATICALLY GENERATED - ANY MODIFICATIONS MADE TO THIS\n");
    printf("   FILE MAY BE OVERWRITTEN DURING FUTURE BUILDS OF THE SOFTWARE */\n");
    printf("\n");

    make_line_probe_table();
    printf("\n");
    make_pp_signal();
    printf("\n");
    make_pph_signal();
    printf("\n");

    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
