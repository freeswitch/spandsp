/*
 * SpanDSP - a series of DSP components for telephony
 *
 * make_v34_tx_pre_emphasis_filters.c - Create coefficient sets for
 *                                      all the possible transmit
 *                                      pre-emphasis filters.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2013 Steve Underwood
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
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#include "meteor-engine.h"

/* From Table 1/V.34, Table 2/V.34, Table 7/V.34 and Table 9/V.34 */
static const struct
{
    /*! Approximate baud rate (i.e. nearest integer value). */
    int baud_rate;
    struct
    {
        int d;
        int e;
    } low_high[2];
} baud_rate_parameters[] =
{
    {2400, {{2, 3}, {3, 4}}}, /*  2400 baud */
    {2743, {{3, 5}, {2, 3}}}, /* ~2743 baud */
    {2800, {{3, 5}, {2, 3}}}, /*  2800 baud */
    {3000, {{3, 5}, {2, 3}}}, /*  3000 baud */
    {3200, {{4, 7}, {3, 5}}}, /*  3200 baud */
    {3429, {{4, 7}, {4, 7}}}  /* ~3429 baud */
};

struct meteor_spec_s requirements;
struct meteor_working_data_s working;

int main(int argc, char *argv[])
{
    int i;
    int n;
    int j;
    int k;
    int nn;
    float baud_rate;
    float d;
    float e;
    float left_freq;
    float right_freq;
    float left_freq2;
    float right_freq2;
    float left_gain;
    float right_gain;
    float left_gain2;
    float right_gain2;
    float alpha;
    float beta;
    float gamma;
    double coeffs[16 + 2];
    char label[200 + 1];
    FILE *fd;
    FILE *log_fd;

    memset(coeffs, 0, sizeof(coeffs));
    if ((log_fd = fopen("filters.csv", "w")) == NULL)
    {
        fprintf(stderr, "Failed to open '%s'\n", "filters.csv");
        exit(2);
    }
    /*endif*/
    if ((fd = fopen("v34_tx_pre_emphasis_filters.h", "w")) == NULL)
    {
        fprintf(stderr, "Failed to open '%s'\n", "v34_tx_pre_emphasis_filters.h");
        exit(2);
    }
    /*endif*/

    fprintf(fd, "/* THIS FILE WAS AUTOMATICALLY GENERATED - ANY MODIFICATIONS MADE TO THIS\n");
    fprintf(fd, "   FILE MAY BE OVERWRITTEN DURING FUTURE BUILDS OF THE SOFTWARE */\n");
    fprintf(fd, "\n");

    fprintf(fd, "static const float v34_tx_pre_emphasis_filters[6][2][10][16] =\n");
    fprintf(fd, "{\n");
    for (i = 0;  i < 6;  i++)
    {
        baud_rate = baud_rate_parameters[i].baud_rate;
        fprintf(fd, "    {\n");
        for (n = 0;  n < 2;  n++)
        {
            d = baud_rate_parameters[i].low_high[n].d;
            e = baud_rate_parameters[i].low_high[n].e;
            fprintf(fd, "        {\n");
            left_freq = baud_rate*(d/e - 0.45f);
            right_freq = baud_rate*(d/e + 0.45f);
            for (j = 1;  j <= 5;  j++)
            {
                alpha = 2*j;
                left_gain = pow(10.0, (alpha*left_freq/baud_rate)/20.0);
                right_gain = pow(10.0, (alpha*right_freq/baud_rate)/20.0);
                printf("%f %f %f %f\n", left_freq, left_gain, right_freq, right_gain);

                /* Design the filter */
                sprintf(label, "Baud rate %d, %s carrier filter %d, %ddB boost", baud_rate_parameters[i].baud_rate, (n == 0)  ?  "low"  :  "high", j, 2*j);
                requirements.filter_name = label;
                requirements.sample_rate = 8000;
                requirements.shortest = 16;
                requirements.longest = 18;
                requirements.symmetry_type = symmetry_cosine;
                requirements.grid_points = 500;
                requirements.num_specs = 2;

                requirements.spec[0].name = "test";
                requirements.spec[0].type = constraint_type_limit;
                requirements.spec[0].left_freq = left_freq/requirements.sample_rate;
                requirements.spec[0].right_freq = right_freq/requirements.sample_rate;
                requirements.spec[0].left_bound = left_gain*1.1;
                requirements.spec[0].right_bound = right_gain*1.1;
                requirements.spec[0].sense = sense_upper;
                requirements.spec[0].interpolation = interpolation_geometric;
                requirements.spec[0].hug = false;
                requirements.spec[0].band_pushed = false;

                requirements.spec[1].name = "test";
                requirements.spec[1].type = constraint_type_limit;
                requirements.spec[1].left_freq = left_freq/requirements.sample_rate;
                requirements.spec[1].right_freq = right_freq/requirements.sample_rate;
                requirements.spec[1].left_bound = left_gain*0.9;
                requirements.spec[1].right_bound = right_gain*0.9;
                requirements.spec[1].sense = sense_lower;
                requirements.spec[1].interpolation = interpolation_geometric;
                requirements.spec[1].hug = false;
                requirements.spec[1].band_pushed = false;

                if ((nn = meteor_design_filter(&working, &requirements, coeffs)) < 0)
                {
                    fprintf(stderr, "Error %d in filter design\n", nn);
                    exit(2);
                }
                /*endif*/
                working.log_fd = log_fd;
                output_filter_performance_as_csv_file(&working, "performance1.csv");

                fprintf(fd, "            {   /* %s */\n", label);
                for (k = 0;  k < 15;  k++)
                    fprintf(fd, "                %10.5f,\n", coeffs[k]);
                /*endfor*/
                fprintf(fd, "                %10.5f\n", coeffs[k]);
                fprintf(fd, "            },\n");
            }
            /*endfor*/
            left_freq = baud_rate*(d/e - 0.45f);
            right_freq = baud_rate*0.4f;
            left_freq2 = baud_rate*0.8f;
            right_freq2 = baud_rate*(d/e + 0.45f);
            for (j = 1;  j <= 5;  j++)
            {
                beta = (float) j/2.0f;
                gamma = (float) j;
                left_gain = 1.0;
                right_gain = 1.0;
                left_gain2 = pow(10.0, beta/20.0);
                right_gain2 = pow(10.0, ((beta + gamma)*right_freq2/(1.2f*baud_rate))/20.0);
                printf("%f %f %f %f %f %f %f %f\n", left_freq, left_gain, right_freq, right_gain, left_freq2, left_gain2, right_freq2, right_gain2);

                /* Design the filter */
                sprintf(label, "Baud rate %d, %s carrier filter %d, %.1fdB to %.1fdB+%.1fdB boost", baud_rate_parameters[i].baud_rate, (n == 0)  ?  "low"  :  "high", j + 5, beta, beta, gamma);
                requirements.filter_name = label;
                requirements.sample_rate = 8000;
                requirements.shortest = 16;
                requirements.longest = 18;
                requirements.symmetry_type = symmetry_cosine;
                requirements.grid_points = 500;
                requirements.num_specs = 4;

                requirements.spec[0].name = "test";
                requirements.spec[0].type = constraint_type_limit;
                requirements.spec[0].left_freq = left_freq/requirements.sample_rate;
                requirements.spec[0].right_freq = right_freq/requirements.sample_rate;
                requirements.spec[0].left_bound = left_gain*1.1;
                requirements.spec[0].right_bound = right_gain*1.1;
                requirements.spec[0].sense = sense_upper;
                requirements.spec[0].interpolation = interpolation_geometric;
                requirements.spec[0].hug = false;
                requirements.spec[0].band_pushed = false;

                requirements.spec[1].name = "test";
                requirements.spec[1].type = constraint_type_limit;
                requirements.spec[1].left_freq = left_freq/requirements.sample_rate;
                requirements.spec[1].right_freq = right_freq/requirements.sample_rate;
                requirements.spec[1].left_bound = left_gain*0.9;
                requirements.spec[1].right_bound = right_gain*0.9;
                requirements.spec[1].sense = sense_lower;
                requirements.spec[1].interpolation = interpolation_geometric;
                requirements.spec[1].hug = false;
                requirements.spec[1].band_pushed = false;

                requirements.spec[2].name = "test";
                requirements.spec[2].type = constraint_type_limit;
                requirements.spec[2].left_freq = left_freq2/requirements.sample_rate;
                requirements.spec[2].right_freq = right_freq2/requirements.sample_rate;
                requirements.spec[2].left_bound = left_gain2*1.1;
                requirements.spec[2].right_bound = right_gain2*1.1;
                requirements.spec[2].sense = sense_upper;
                requirements.spec[2].interpolation = interpolation_geometric;
                requirements.spec[2].hug = false;
                requirements.spec[2].band_pushed = false;

                requirements.spec[3].name = "test";
                requirements.spec[3].type = constraint_type_limit;
                requirements.spec[3].left_freq = left_freq2/requirements.sample_rate;
                requirements.spec[3].right_freq = right_freq2/requirements.sample_rate;
                requirements.spec[3].left_bound = left_gain2*0.9;
                requirements.spec[3].right_bound = right_gain2*0.9;
                requirements.spec[3].sense = sense_lower;
                requirements.spec[3].interpolation = interpolation_geometric;
                requirements.spec[3].hug = false;
                requirements.spec[3].band_pushed = false;

                if ((nn = meteor_design_filter(&working, &requirements, coeffs)) < 0)
                {
                    fprintf(stderr, "Error %d in filter design\n", nn);
                    exit(2);
                }
                /*endif*/
                working.log_fd = log_fd;
                output_filter_performance_as_csv_file(&working, "performance2.csv");

                fprintf(fd, "            {   /* %s */\n", label);
                for (k = 0;  k < 15;  k++)
                    fprintf(fd, "                %10.5f,\n", coeffs[k]);
                /*endfor*/
                fprintf(fd, "                %10.5f\n", coeffs[k]);
                fprintf(fd, "            }");
                if (j != 5)
                    fprintf(fd, ",\n");
                else
                    fprintf(fd, "\n");
                /*endif*/
            }
            /*endfor*/
            fprintf(fd, "        }");
            if (n != 1)
                fprintf(fd, ",\n");
            else
                fprintf(fd, "\n");
            /*endif*/
        }
        if (i != 5)
            fprintf(fd, "    },\n");
        else
            fprintf(fd, "    }\n");
        /*endif*/
    }
    /*endfor*/
    fprintf(fd, "};\n");
    fclose(fd);
    fclose(log_fd);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
