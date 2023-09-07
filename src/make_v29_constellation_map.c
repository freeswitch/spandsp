/*
 * SpanDSP - a series of DSP components for telephony
 *
 * make_v29_constellation_map.c - Create receive constellation
 *                                map for the V.29 modem.
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

#include "v29tx_constellation_maps.h"

static void make_v29_constellation_map(void)
{
    int l;
    int best;
    double best_distance;
    double distance;
    double re;
    double im;
    int ire;
    int iim;

    printf("/* THIS FILE WAS AUTOMATICALLY GENERATED - ANY MODIFICATIONS MADE TO THIS\n");
    printf("   FILE MAY BE OVERWRITTEN DURING FUTURE BUILDS OF THE SOFTWARE */\n");
    printf("\n");

    printf("/* The following table maps every possible point in the constellation space.\n");
    printf("   If you look at the constellations carefully, every point can be accurately\n");
    printf("   mapped at 0.5 unit resolution. */\n");
    printf("\n");

    printf("static const uint8_t space_map_9600[20][20] =\n");
    printf("{\n");
    for (ire = 0;  ire < 20;  ire++)
    {
        re = (ire - 10)/2.0 + 0.25;
        printf("    {");
        for (iim = 0;  iim < 20;  iim++)
        {
            im = (iim - 10)/2.0 + 0.25;
            best = 0;
            best_distance = 1000000.0;
            for (l = 0;  l < 16;  l++)
            {
                distance = (re - v29_9600_constellation[l].re)*(re - v29_9600_constellation[l].re)
                         + (im - v29_9600_constellation[l].im)*(im - v29_9600_constellation[l].im);
                if (distance < best_distance)
                {
                    best = l;
                    best_distance = distance;
                }
                /*endif*/
            }
            /*endfor*/
            printf("%2d", best);
            if (iim < 19)
                printf(", ");
            /*endif*/
        }
        /*endfor*/
        printf("}");
        if (ire < 19)
            printf(",");
        /*endif*/
        printf("\n");
    }
    /*endfor*/
    printf("};\n");
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    make_v29_constellation_map();
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
