/*
 * SpanDSP - a series of DSP components for telephony
 *
 * make_v34_shell_map.c - ITU V.34 modem shell map generation.
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
#include <stdlib.h>
#include <stdio.h>

#define MAX_SHELL_RINGS 18

const int max_ring_bits[] =
{
     0, /*  0 */
     0, /*  1 */
     8, /*  2 */
    12, /*  3 */
    16, /*  4 */
    18, /*  5 */
    20, /*  6 */
    22, /*  7 */
    24, /*  8 */
    25, /*  9 */
    26, /* 10 */
    27, /* 11 */
    28, /* 12 */
    29, /* 13 */
    30, /* 14 */
    31, /* 15 */
    -1, /* 16 - this many rings is never used. */
    30, /* 17 */
    31, /* 18 */
};

/* Shell mapper tables g2 - z8 */
uint32_t g2[4*(MAX_SHELL_RINGS - 1) + 1];
uint32_t g4[8*(MAX_SHELL_RINGS - 1) + 1];
uint32_t g8[8*(MAX_SHELL_RINGS - 1) + 1];
uint32_t z8[8*(MAX_SHELL_RINGS - 1) + 1];

static void make_shell_mapper_tables(void)
{
    int p;
    int32_t k;
    uint64_t sum;
    int M;
    int array_elements;
    int i;
    FILE *file;

    /* V.34/9.4 doesn't quite describe the one ring case properly, so just output the
       simple data needed for that as a special case */
    printf("/* 1 rings deals with up to 0 bits */\n");
    printf("static const uint32_t g2_1_rings[2] =\n");
    printf("{\n");
    printf("    1,\n");
    printf("    0\n");
    printf("};\n");
    printf("\n");

    printf("static const uint32_t g4_1_rings[2] =\n");
    printf("{\n");
    printf("    1,\n");
    printf("    0\n");
    printf("};\n");
    printf("\n");

    printf("static const uint32_t z8_1_rings[2] =\n");
    printf("{\n");
    printf("    0x00000000,\n");
    printf("    0x00000001\n");
    printf("};\n");
    printf("\n");

    for (M = 2;  M <= MAX_SHELL_RINGS;  M++)
    {
        /* Create the shell mapper tables - See V.34/9.4 */
        if (max_ring_bits[M] < 0)
            continue;

        for (p = 0;  p <= 2*(M - 1);  p++)
            g2[p] = M - abs(p - M + 1);
        /*endfor*/
        for (  ;  p <= 4*(M - 1);  p++)
            g2[p] = 0;
        /*endfor*/

        for (p = 0;  p <= 4*(M - 1);  p++)
        {
            sum = 0;
            for (k = 0;  k <= p;  k++)
                sum += g2[k]*g2[p - k];
            /*endfor*/
            g4[p] = sum;
        }
        /*endfor*/
        for (  ;  p <= 8*(M - 1);  p++)
            g4[p] = 0;
        /*endfor*/

        for (p = 0;  p <= 8*(M - 1);  p++)
        {
            sum = 0;
            for (k = 0;  k <= p;  k++)
                sum += g4[k]*g4[p - k];
            /*endfor*/
            g8[p] = sum;
        }
        /*endfor*/

        for (p = 0;  p <= 8*(M - 1);  p++)
        {
            sum = 0;
            for (k = 0;  k <= p - 1;  k++)
                sum += g8[k];
            if (sum <= 0xFFFFFFFFULL)
                z8[p] = sum;
            else
                z8[p] = 0xFFFFFFFFUL;
            /*endif*/
        }
        /*endfor*/

        /* Our tables only need enough g4 and z8 elements to cover the
           required bit range. Find how many that is */

        array_elements = 8*(M - 1);
        for (p = 0;  p <= 8*(M - 1);  p++)
        {
            if (z8[p] >= (1U << max_ring_bits[M]))
            {
                array_elements = p;
                break;
            }
            /*endif*/
        }
        /*endfor*/

        printf("/* %d rings deals with up to %d bits */\n", M, max_ring_bits[M]);

        printf("static const uint32_t g2_%d_rings[%d] =\n", M, 4*(M - 1) + 1);
        printf("{\n");
        for (p = 0;  p <= 4*(M - 1);  p++)
            printf("    %u%s\n", g2[p], (p < 4*(M - 1))  ?  ","  :  "");
        /*endfor*/
        printf("};\n\n");

        printf("static const uint32_t g4_%d_rings[%d] =\n", M, array_elements + 1);
        printf("{\n");
        for (p = 0;  p <= array_elements;  p++)
            printf("    %u%s\n", g4[p], (p < array_elements)  ?  ","  :  "");
        /*endfor*/
        printf("};\n\n");

        printf("static const uint32_t z8_%d_rings[%d] =\n", M, array_elements + 1);
        printf("{\n");
        for (p = 0;  p <= array_elements;  p++)
            printf("    0x%08X%s\n", z8[p], (p < array_elements)  ?  ","  :  "");
        /*endfor*/
        printf("};\n\n");
    }
    /*endfor*/

    printf("static const uint32_t *g2s[19] =\n");
    printf("{\n");
    printf("    NULL,\n");
    for (i = 1;  i <= 15;  i++)
        printf("    g2_%d_rings,\n", i);
    /*endfor*/
    printf("    NULL,\n");
    printf("    g2_17_rings,\n");
    printf("    g2_18_rings\n");
    printf("};\n");
    printf("\n");
    printf("static const uint32_t *g4s[19] =\n");
    printf("{\n");
    printf("    NULL,\n");
    for (i = 1;  i <= 15;  i++)
        printf("    g4_%d_rings,\n", i);
    /*endfor*/
    printf("    NULL,\n");
    printf("    g4_17_rings,\n");
    printf("    g4_18_rings\n");
    printf("};\n");
    printf("\n");
    printf("static const uint32_t *z8s[19] =\n");
    printf("{\n");
    printf("    NULL,\n");
    for (i = 1;  i <= 15;  i++)
        printf("    z8_%d_rings,\n", i);
    /*endfor*/
    printf("    NULL,\n");
    printf("    z8_17_rings,\n");
    printf("    z8_18_rings\n");
    printf("};\n");
    printf("\n");
    printf("/*- End of file ------------------------------------------------------------*/\n");
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    printf("/* THIS FILE WAS AUTOMATICALLY GENERATED - ANY MODIFICATIONS MADE TO THIS\n");
    printf("   FILE MAY BE OVERWRITTEN DURING FUTURE BUILDS OF THE SOFTWARE */\n");
    printf("\n");
    make_shell_mapper_tables();

    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
