/*
 * SpanDSP - a series of DSP components for telephony
 *
 * make_v17_v32_convolutional_encoder.c - ITU V.17/V.32bis convolutional
 *                                        encoder table generation.
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

static const uint8_t tcm_paths[8][4] =
{
    {0, 6, 2, 4},
    {6, 0, 4, 2},
    {2, 4, 0, 6},
    {4, 2, 6, 0},
    {1, 3, 7, 5},
    {5, 7, 3, 1},
    {7, 5, 1, 3},
    {3, 1, 5, 7}
};

static uint8_t conv_encode_table[8][4];

static void split_bits(int bits[], int word, int len)
{
    int i;
    
    for (i = 0;  i < len;  i++)
    {
        bits[i + 1] = word & 1;
        word >>= 1;
    }
    /*endfor*/
}
/*- End of function --------------------------------------------------------*/

static int pack_bits(int bits[], int len)
{
    int i;
    int word;
    
    word = 0;
    for (i = 0;  i < len;  i++)
    {
        word <<= 1;
        word |= (bits[len - i] & 1);
    }
    /*endfor*/
    return word;
}
/*- End of function --------------------------------------------------------*/

static void make_v17_v32_convolutional_encoder(void)
{
    int diff;
    int y[3];
    int t[4];
    int nt[4];
    int convolution;
    int new_convolution;

    printf("static const uint8_t v17_convolutional_encoder[8][4] =\n");
    printf("{\n");
    for (convolution = 0;  convolution < 8;  convolution++)
    {
        printf("    {");
        for (diff = 0;  diff < 4;  diff++)
        {
            split_bits(y, diff, 2);
            split_bits(t, convolution, 3);

            /* Find the new register bits from the old ones */
            nt[3] = t[1];
            nt[2] = y[2] ^ y[1] ^ t[3] ^ ((y[2] ^ t[2]) & t[1]);
            nt[1] = y[2] ^ t[2] ^ (y[1] & t[1]);

            new_convolution = pack_bits(nt, 3);
            printf("0x%02X", new_convolution);
            if (diff < 3)
                printf(", ");
            conv_encode_table[convolution][diff] = new_convolution;
        }
        /*endfor*/
        if (convolution < 7)
            printf("},\n");
        else
            printf("}\n");
        /*endif*/
    }
    /*endfor*/
    printf("};\n");
    printf("\n");
}
/*- End of function --------------------------------------------------------*/

static void make_v17_v32_convolutional_decoder(void)
{
#if 1
    int state;
    int y;
    int previous_state;
    int next_state;
    int branch;
    int i;
    uint8_t tcm_paths[8][4];
static const uint8_t tcm_pathsx[8][4] =
{
    {0, 6, 2, 4},
    {6, 0, 4, 2},
    {2, 4, 0, 6},
    {4, 2, 6, 0},
    {1, 3, 7, 5},
    {5, 7, 3, 1},
    {7, 5, 1, 3},
    {3, 1, 5, 7}
};


    printf("static const uint8_t v17_convolutional_decoder[8][4] =\n");
    printf("{\n");
    for (state = 0;  state < 8;  state++)
    {
        for (y = 0;  y < 4;  y++)
        {
            next_state = conv_encode_table[state][y];
            tcm_paths[next_state][y] = state;
#if 0
            previous_state = -1;
            for (i = 0;  i < 8;  i++)
            {
                if (conv_encode_table[i][y] == state)
                {
                    tcm_paths[state][y] = i;
                    previous_state = i;
                    branch = (y << 1) | (previous_state & 1);
                }
                /*endif*/
            }
            /*endfor*/
#endif
        }
        /*endfor*/
    }
    /*endfor*/
    for (i = 0;  i < 8;  i++)
    {
        printf("    {");
        for (y = 0;  y < 4;  y++)
        {
            printf("0x%02X", tcm_paths[i][y]);
            if (y < 3)
                printf(", ");
            /*endif*/
        }
        /*endfor*/
        if (i < 7)
            printf("},\n");
        else
            printf("}\n");
        /*endif*/
    }
    /*endfor*/
    printf("};\n");
    printf("\n");
#endif
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    printf("/* THIS FILE WAS AUTOMATICALLY GENERATED - ANY MODIFICATIONS MADE TO THIS\n");
    printf("   FILE MAY BE OVERWRITTEN DURING FUTURE BUILDS OF THE SOFTWARE */\n");
    printf("\n");

    make_v17_v32_convolutional_encoder();
    make_v17_v32_convolutional_decoder();

    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
