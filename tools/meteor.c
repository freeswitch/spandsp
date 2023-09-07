/*
 * SpanDSP - a series of DSP components for telephony
 *
 * meteor.c - The meteor FIR design algorithm
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

/*! \file */

#include <inttypes.h>
#include <stdbool.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <setjmp.h>
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/xinclude.h>

#include "meteor-engine.h"
#include "meteor-xml-reader.h"
#include "ae.h"

void meteor_output_coefficients_as_h_file(struct meteor_working_data_s *s, const char *file_name)
{
    int i;
    int k;
    FILE *coeffs_file;           /* Files for output */
    char uc_filter_name[strlen(s->spec->filter_name) + 2];
    
    /* Print resulting coeffs as a C .h file */
    coeffs_file = fopen(file_name, "wb");
    if (coeffs_file == NULL)
    {
        fprintf(stderr, "Cannot open file '%s'\n", file_name);
        exit(2);
    }
    /*endif*/

    uc_filter_name[0] = '\0';
    k = strlen(s->spec->filter_name);
    if (k > 0)
    {
        uc_filter_name[0] = '_';
        for (i = 0;  i < k;  i++)
            uc_filter_name[i + 1] = toupper(s->spec->filter_name[i]);
        /*endfor*/
        uc_filter_name[k + 1] = '\0';
    }
    if (s->odd_length  &&  s->spec->symmetry_type == symmetry_cosine)
    {
        fprintf(coeffs_file, "#define NUM_COEFFS%s %4d /* cosine symmetry */\n", uc_filter_name, s->m*2 - 1);
        fprintf(coeffs_file, "float %s[NUM_COEFFS%s] =\n", s->spec->filter_name, uc_filter_name);
        fprintf(coeffs_file, "{\n");
        for (i = s->m - 1;  i >= 1;  i--)
            fprintf(coeffs_file, "    % .5E,\n", s->coeff[i]/2);
        /*endfor*/
        fprintf(coeffs_file, "    % .5E,\n", s->coeff[0]);
        for (i = 1;  i < s->m;  i++)
            fprintf(coeffs_file, "    % .5E,\n", s->coeff[i]/2);
        /*endfor*/
    }
    else if (!s->odd_length  &&  s->spec->symmetry_type == symmetry_cosine)
    {
        fprintf(coeffs_file, "#define NUM_COEFFS%s %4d /*cosine symmetry */\n", uc_filter_name, s->m*2);
        fprintf(coeffs_file, "float %s[NUM_COEFFS%s] =\n", s->spec->filter_name, uc_filter_name);
        fprintf(coeffs_file, "{\n");
        for (i = s->m - 1;  i >= 0;  i--)
            fprintf(coeffs_file, "    % .5E,\n", s->coeff[i]/2);
        /*endfor*/
        for (i = 0;  i < s->m;  i++)
            fprintf(coeffs_file, "    % .5E,\n", s->coeff[i]/2);
        /*endfor*/
    }
    else if (s->odd_length  &&  s->spec->symmetry_type == symmetry_sine)
    {
        fprintf(coeffs_file, "#define NUM_COEFFS %4d /* sine symmetry */\n", uc_filter_name, s->m*2 + 1);
        fprintf(coeffs_file, "float %s[NUM_COEFFS] =\n", s->spec->filter_name, uc_filter_name);
        fprintf(coeffs_file, "{\n");
        /* L = length, odd */
        /* Negative of the first m coefs. */
        for (i = s->m - 1;  i >= 0;  i--)
            fprintf(coeffs_file, "    % .5E,\n", s->coeff[i]/-2);
        /*endfor*/
        /* Middle coefficient is always 0 */
        fprintf(coeffs_file, "     0.0,\n");
        for (i = 0;  i < s->m;  i++)
            fprintf(coeffs_file, "    % .5E,\n", s->coeff[i]/2);
        /*endfor*/
    }
    else if (!s->odd_length  &&  s->spec->symmetry_type == symmetry_sine)
    {
        fprintf(coeffs_file, "#define NUM_COEFFS %4d /* sine symmetry */\n", uc_filter_name, s->m*2);
        fprintf(coeffs_file, "float %s[NUM_COEFFS] =\n", s->spec->filter_name, uc_filter_name);
        fprintf(coeffs_file, "{\n");
        /* Negative of the first m coefs. */
        for (i = s->m - 1;  i >= 0;  i--)
            fprintf(coeffs_file, "    % .5E,\n", s->coeff[i]/-2);
        /*endfor*/
        for (i = 0;  i < s->m;  i++)
            fprintf(coeffs_file, "    % .5E,\n", s->coeff[i]/2);
        /*endfor*/
    }
    /*endif*/
    fprintf(coeffs_file, "};\n");
    fclose(coeffs_file);
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    struct meteor_working_data_s state;
    struct meteor_spec_s spec;
    double coeffs[1024];
    int num_coeffs;

    printf("Welcome to Meteor:\n");
    printf("Constraint-based, linear-phase FIR filter design\n");
    ae_open();
    get_xml_filter_spec(&spec, argv[1]);

    if ((num_coeffs = meteor_design_filter(&state, &spec, coeffs)) < 0)
    {
        fprintf(stderr, "Error %d\n", num_coeffs);
        exit(2);
    }

    meteor_output_coefficients_as_h_file(&state, "coeffs.h");
    output_filter_performance_as_csv_file(&state, "performance.csv");
    ae_close();
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
