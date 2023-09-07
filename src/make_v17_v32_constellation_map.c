/*
 * SpanDSP - a series of DSP components for telephony
 *
 * make_v17_v32_constellation_map.c - Create receive constellation
 *                                    maps for V.17/V.32bis.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2008 Steve Underwood
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
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>

#include "spandsp/telephony.h"
#include "spandsp/complex.h"

#define FP_CONSTELLATION_SCALE(x)       (x)

#include "v17_v32bis_tx_constellation_maps.h"

static void make_v17_v32bis_constellation_map(int v32bis_mode)
{
    int i;
    int l;
    int best;
    double best_distance;
    double distance;
    double re;
    double im;
    int ire;
    int iim;

    /* v32bis_mode causes the 48000bps mode without trellis encoding mode from V.32bis
       which is not used by V.17 */
    printf("/* THIS FILE WAS AUTOMATICALLY GENERATED - ANY MODIFICATIONS MADE TO THIS\n");
    printf("   FILE MAY BE OVERWRITTEN DURING FUTURE BUILDS OF THE SOFTWARE */\n");
    printf("\n");

    printf("/* The following table maps the 8 soft-decisions associated with every possible\n");
    printf("   point in the constellation space. If you look at the constellations carefully,\n");
    printf("   all 4 can be accurately mapped at 0.5 unit resolution. */\n");
    printf("\n");

    printf("static const uint8_t constel_maps[4][36][36][8] =\n");
    printf("{\n");
    printf("    {   /* 14,400bps map */\n");
    for (ire = 0;  ire <= 35;  ire++)
    {
        re = (ire - 18)/2.0 + 0.25;
        printf("        {\n");
        for (iim = 0;  iim <= 35;  iim++)
        {
            im = (iim - 18)/2.0 + 0.25;
            /* Find the closest constellation point in each of the 8 sets. There will often be more
               than one at the same distance, but that just means we are so far from anything sane
               that it really doesn't matter which one we choose. */
            printf("            {");
            for (i = 0;  i < 8;  i++)
            {
                best = 0;
                best_distance = 1000000.0;
                for (l = i;  l < 128;  l += 8)
                {
                    distance = (re - v17_v32bis_14400_constellation[l].re)*(re - v17_v32bis_14400_constellation[l].re)
                             + (im - v17_v32bis_14400_constellation[l].im)*(im - v17_v32bis_14400_constellation[l].im);
                    if (distance <= best_distance)
                    {
                        best = l;
                        best_distance = distance;
                    }
                    /*endif*/
                }
                /*endfor*/
                printf("0x%02x", best);
                if (i < 7)
                    printf(", ");
                /*endif*/
            }
            /*endfor*/
            printf("}");
            if (iim < 35)
                printf(",");
            /*endif*/
            printf("\n");
        }
        printf("        }");
        if (ire < 35)
            printf(",");
        /*endif*/
        printf("\n");
    }
    /*endfor*/
    printf("    },\n");
    printf("    {   /* 12,000bps map */\n");
    for (ire = 0;  ire <= 35;  ire++)
    {
        re = (ire - 18)/2.0 + 0.25;
        printf("        {\n");
        for (iim = 0;  iim <= 35;  iim++)
        {
            im = (iim - 18)/2.0 + 0.25;
            printf("            {");
            for (i = 0;  i < 8;  i++)
            {
                best = 0;
                best_distance = 1000000.0;
                for (l = i;  l < 64;  l += 8)
                {
                    distance = (re - v17_v32bis_12000_constellation[l].re)*(re - v17_v32bis_12000_constellation[l].re)
                             + (im - v17_v32bis_12000_constellation[l].im)*(im - v17_v32bis_12000_constellation[l].im);
                    if (distance <= best_distance)
                    {
                        best = l;
                        best_distance = distance;
                    }
                    /*endif*/
                }
                /*endfor*/
                printf("0x%02x", best);
                if (i < 7)
                    printf(", ");
                /*endif*/
            }
            /*endfor*/
            printf("}");
            if (iim < 35)
                printf(",");
            /*endif*/
            printf("\n");
        }
        /*endfor*/
        printf("        }");
        if (ire < 35)
            printf(",");
        /*endif*/
        printf("\n");
    }
    /*endfor*/
    printf("    },\n");
    printf("    {   /* 9,600bps map */\n");
    for (ire = 0;  ire <= 35;  ire++)
    {
        re = (ire - 18)/2.0 + 0.25;
        printf("        {\n");
        for (iim = 0;  iim <= 35;  iim++)
        {
            im = (iim - 18)/2.0 + 0.25;
            printf("            {");
            for (i = 0;  i < 8;  i++)
            {
                best = 0;
                best_distance = 1000000.0;
                for (l = i;  l < 32;  l += 8)
                {
                    distance = (re - v17_v32bis_9600_constellation[l].re)*(re - v17_v32bis_9600_constellation[l].re)
                             + (im - v17_v32bis_9600_constellation[l].im)*(im - v17_v32bis_9600_constellation[l].im);
                    if (distance <= best_distance)
                    {
                        best = l;
                        best_distance = distance;
                    }
                    /*endif*/
                }
                /*endfor*/
                printf("0x%02x", best);
                if (i < 7)
                    printf(", ");
                /*endif*/
            }
            /*endfor*/
            printf("}");
            if (iim < 35)
                printf(",");
            /*endif*/
            printf("\n");
        }
        /*endfor*/
        printf("        }");
        if (ire < 35)
            printf(",");
        /*endif*/
        printf("\n");
    }
    /*endfor*/
    printf("    },\n");
    printf("    {   /* 7,200bps map */\n");
    for (ire = 0;  ire <= 35;  ire++)
    {
        re = (ire - 18)/2.0 + 0.25;
        printf("        {\n");
        for (iim = 0;  iim <= 35;  iim++)
        {
            im = (iim - 18)/2.0 + 0.25;
            printf("            {");
            for (i = 0;  i < 8;  i++)
            {
                best = 0;
                best_distance = 1000000.0;
                for (l = i;  l < 16;  l += 8)
                {
                    distance = (re - v17_v32bis_7200_constellation[l].re)*(re - v17_v32bis_7200_constellation[l].re)
                             + (im - v17_v32bis_7200_constellation[l].im)*(im - v17_v32bis_7200_constellation[l].im);
                    if (distance <= best_distance)
                    {
                        best = l;
                        best_distance = distance;
                    }
                    /*endif*/
                }
                /*endfor*/
                printf("0x%02x", best);
                if (i < 7)
                    printf(", ");
                /*endif*/
            }
            printf("}");
            if (iim < 35)
                printf(",");
            /*endif*/
            printf("\n");
        }
        printf("        }");
        if (ire < 35)
            printf(",");
        /*endif*/
        printf("\n");
        printf("    }\n");
    }
    /*endfor*/
    printf("};\n");
    if (v32bis_mode)
    {
        printf("\n");
        printf("static const uint8_t constel_map_4800[36][36] =\n");
        printf("{   /* 4,800bps map - No trellis. V.32/V.32bis only */\n");
        for (ire = 0;  ire <= 35;  ire++)
        {
            re = (ire - 18)/2.0 + 0.25;
            printf("    {\n");
            for (iim = 0;  iim <= 35;  iim++)
            {
                im = (iim - 18)/2.0 + 0.25;
                best = 0;
                best_distance = 1000000.0;
                for (l = 0;  l < 4;  l++)
                {
                    distance = (re - v17_v32bis_4800_constellation[l].re)*(re - v17_v32bis_4800_constellation[l].re)
                             + (im - v17_v32bis_4800_constellation[l].im)*(im - v17_v32bis_4800_constellation[l].im);
                    if (distance <= best_distance)
                    {
                        best = l;
                        best_distance = distance;
                    }
                    /*endif*/
                }
                /*endfor*/
                printf("        0x%02x", best);
                if (iim < 35)
                    printf(",");
                /*endif*/
                printf("\n");
            }
            /*endfor*/
            printf("    }");
            if (ire < 35)
                printf(",");
            /*endif*/
            printf("\n");
        }
        /*endfor*/
        printf("};\n");
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Modem not specified. Select V.17 or V32bis.\n");
        exit(2);
    }
    /*endif*/
    if (strcmp(argv[1], "V.17") == 0)
        make_v17_v32bis_constellation_map(0);
    else if (strcmp(argv[1], "V.32bis") == 0)
        make_v17_v32bis_constellation_map(1);
    else
    {
        fprintf(stderr, "Unrecognised modem specified\n");
        exit(2);
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
