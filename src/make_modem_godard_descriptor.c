/*
 * SpanDSP - a series of DSP components for telephony
 *
 * make_modem_godard_descriptor.c - Create coefficient sets for Godard
 *                                  symbol sync. filters as a pair of 
 *                                  fixed and floating point descriptor
 *                                  structures for spandsp.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2009, 2024 Steve Underwood
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
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <math.h>
#include <stdbool.h>
#if defined(__sunos)  ||  defined(__solaris)  ||  defined(__sun)
#include <getopt.h>
#endif

#include "spandsp/telephony.h"
#include "spandsp/complex.h"
#include "spandsp/godard.h"
#include "filter_tools.h"

#if !defined(M_PI)
#define M_PI    3.14159265358979323
#endif

#define FP_FACTOR   4096

static void create_godard_coeffs(double carrier,
                                 double baud_rate,
                                 double alpha,
                                 double sample_rate,
                                 double low_band_edge_coeff[3],
                                 double high_band_edge_coeff[3],
                                 double *mixed_band_edges_coeff_3)
{
    double low_edge;
    double high_edge;

    low_edge = 2.0*M_PI*(carrier - baud_rate/2.0)/sample_rate;
    high_edge = 2.0*M_PI*(carrier + baud_rate/2.0)/sample_rate;

    low_band_edge_coeff[0] = 2.0*alpha*cos(low_edge);
    high_band_edge_coeff[0] = 2.0*alpha*cos(high_edge);
    low_band_edge_coeff[1] =
    high_band_edge_coeff[1] = -alpha*alpha;
    low_band_edge_coeff[2] = -alpha*sin(low_edge);
    high_band_edge_coeff[2] = -alpha*sin(high_edge);
    *mixed_band_edges_coeff_3 = -alpha*alpha*(sin(high_edge)*cos(low_edge) - sin(low_edge)*cos(high_edge));
}
/*- End of function --------------------------------------------------------*/

static void usage(void)
{
    fprintf(stderr, "Usage: make_modem_godard_coefficients [-i] [-s] | <carrier> <baud rate> [<alpha>]\n");
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char **argv)
{
    int opt;
    const char *tag;
    double alpha;
    double carrier;
    double baud_rate;
    double sample_rate;
    double low_band_edge_coeff[3];
    double high_band_edge_coeff[3];
    double mixed_band_edges_coeff_3;
    double coarse_trigger;
    double fine_trigger;
    int coarse_step;
    int fine_step;

    alpha = 0.99;
    carrier = 1800.0;
    baud_rate = 2400.0;
    sample_rate = 8000.0;
    coarse_trigger = 1000.0f;
    fine_trigger = 100.0f;
    coarse_step = 15;
    fine_step = 1;

    if (optind < argc)
    {
        carrier = atof(argv[optind++]);
    }
    else
    {
        usage();
        exit(2);
    }
    /*endif*/
    if (optind < argc)
    {
        baud_rate = atof(argv[optind++]);
    }
    else
    {
        usage();
        exit(2);
    }
    /*endif*/
    if (optind < argc)
        alpha = atof(argv[optind++]);
    /*endif*/
    if (optind < argc)
        coarse_trigger = atof(argv[optind++]);
    /*endif*/
    if (optind < argc)
        fine_trigger = atof(argv[optind++]);
    /*endif*/
    if (optind < argc)
        coarse_step = atof(argv[optind++]);
    /*endif*/
    if (optind < argc)
        fine_step = atof(argv[optind++]);
    /*endif*/


    create_godard_coeffs(carrier,
                         baud_rate,
                         alpha,
                         sample_rate,
                         low_band_edge_coeff,
                         high_band_edge_coeff,
                         &mixed_band_edges_coeff_3);

    printf("/* THIS FILE WAS AUTOMATICALLY GENERATED - ANY MODIFICATIONS MADE TO THIS\n");
    printf("   FILE MAY BE OVERWRITTEN DURING FUTURE BUILDS OF THE SOFTWARE */\n");
    printf("\n");
    printf("\n");
    printf("static const godard_ted_descriptor_t godard_desc =\n");
    printf("{\n");
    printf("    /* %.1f samples/second , %.1fHz carrier, %.1f baud, %.3f alpha */\n",
           sample_rate,
           carrier,
           baud_rate,
           alpha);
    printf("#if defined(SPANDSP_USE_FIXED_POINT)\n");
    printf("    {\n");
    printf("        %d,\n", (int) (FP_FACTOR*low_band_edge_coeff[0]));
    printf("        %d,\n", (int) (FP_FACTOR*low_band_edge_coeff[1]));
    printf("        %d,\n", (int) (FP_FACTOR*low_band_edge_coeff[2]));
    printf("    },\n");
    printf("    {\n");
    printf("        %d,\n", (int) (FP_FACTOR*high_band_edge_coeff[0]));
    printf("        %d,\n", (int) (FP_FACTOR*high_band_edge_coeff[1]));
    printf("        %d,\n", (int) (FP_FACTOR*high_band_edge_coeff[2]));
    printf("    },\n");
    printf("    %d,\n", (int) (FP_FACTOR*mixed_band_edges_coeff_3));
    printf("    %d,\n", (int) (FP_FACTOR*coarse_trigger));
    printf("    %d,\n", (int) (FP_FACTOR*fine_trigger));
    printf("    %d,\n", coarse_step);
    printf("    %d\n", fine_step);
    printf("#else\n");
    printf("    {\n");
    printf("        %10.6ff,\n", low_band_edge_coeff[0]);
    printf("        %10.6ff,\n", low_band_edge_coeff[1]);
    printf("        %10.6ff,\n", low_band_edge_coeff[2]);
    printf("    },\n");
    printf("    {\n");
    printf("        %10.6ff,\n", high_band_edge_coeff[0]);
    printf("        %10.6ff,\n", high_band_edge_coeff[1]);
    printf("        %10.6ff,\n", high_band_edge_coeff[2]);
    printf("    },\n");
    printf("    %10.6ff,\n", mixed_band_edges_coeff_3);
    printf("    %14.6ff,\n", coarse_trigger);
    printf("    %14.6ff,\n", fine_trigger);
    printf("    %d,\n", coarse_step);
    printf("    %d\n", fine_step);
    printf("#endif\n");
    printf("};\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
