/*
 * SpanDSP - a series of DSP components for telephony
 *
 * make_v34_convolutional_coders.c - ITU V.34 convolutional
 *                                   encoder and decoder table
 *                                   generation.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2015 Steve Underwood
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

static uint8_t v34_conv16_encode_table[16][16];
static uint8_t v34_conv32_encode_table[32][16];
static uint8_t v34_conv64_encode_table[64][16];

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

static void make_v34_16_state_convolutional_encoder(void)
{
    int diff;
    int y[5];
    int t[5];
    int nt[5];
    int convolution;
    int new_convolution;

    printf("/* From Figure 10/V.34 */\n");
    printf("static const uint8_t v34_conv16_encode_table[16][16] =\n");
    printf("{\n");
    for (convolution = 0;  convolution < 16;  convolution++)
    {
        printf("    {");
        for (diff = 0;  diff < 16;  diff++)
        {
            split_bits(y, diff, 4);
            split_bits(t, convolution, 4);

            nt[4] = t[1];
            nt[3] = t[4] ^ t[1] ^ y[2];
            nt[2] = t[3] ^ y[2];
            nt[1] = t[2] ^ y[1];

            new_convolution = pack_bits(nt, 4);
            printf("0x%02X", new_convolution);
            if (diff < 15)
                printf(", ");
            /*endif*/
            v34_conv16_encode_table[convolution][diff] = new_convolution;
        }
        /*endfor*/
        if (convolution < 15)
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

static void make_v34_16_state_convolutional_decoder(void)
{
#if 1
    int state;
    int y;
    int previous_state;
    int branch;
    int i;

    printf("static const uint8_t v34_conv16_decode_table[16][4] =\n");
    printf("{\n");
    for (state = 0;  state < 16;  state++)
    {
        printf("    {");
        for (y = 0;  y < 4;  y++)
        {
            previous_state = -1;
            for (i = 0;  i < 16;  i++)
            {
                if (v34_conv16_encode_table[i][y] == state)
                    previous_state = i;
                /*endif*/
            }
            /*endfor*/
            branch = (y << 1) | (previous_state & 1);
            printf("0x%02X", (previous_state << 3) | branch);
            //printf("(0x%02X << 3) | 0x%02x", previous_state, branch);
            if (y < 3)
                printf(", ");
            /*endif*/
        }
        printf("}%s\n", (state < 15)  ?  ","  :  "");
    }
    /*endfor*/
    printf("};\n");
    printf("\n");
#endif
}
/*- End of function --------------------------------------------------------*/

static void make_v34_32_state_convolutional_encoder(void)
{
    int diff;
    int y[5];
    int t[6];
    int nt[6];
    int convolution;
    int new_convolution;

    printf("/* From Figure 11/V.34 */\n");
    printf("static const uint8_t v34_conv32_encode_table[32][16] =\n");
    printf("{\n");
    for (convolution = 0;  convolution < 32;  convolution++)
    {
        printf("    {");
        for (diff = 0;  diff < 16;  diff++)
        {
            split_bits(y, diff, 4);
            split_bits(t, convolution, 5);

            nt[5] = t[1];
            nt[4] = t[5] ^ y[2];
            nt[3] = t[4] ^ y[1];
            nt[2] = t[3] ^ y[4];
            nt[1] = t[2] ^ y[2];

            new_convolution = pack_bits(nt, 5);
            printf("0x%02X", new_convolution);
            if (diff < 15)
                printf(", ");
            /*endif*/
            v34_conv32_encode_table[convolution][diff] = new_convolution;
        }
        if (convolution < 31)
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

static void make_v34_32_state_convolutional_decoder(void)
{
}
/*- End of function --------------------------------------------------------*/

static void make_v34_64_state_convolutional_encoder(void)
{
    int diff;
    int y[7];
    int t[7];
    int nt[7];
    int convolution;
    int new_convolution;

    printf("/* From Figure 12/V.34 */\n");
    printf("static const uint8_t v34_conv64_encode_table[64][16] =\n");
    printf("{\n");
    for (convolution = 0;  convolution < 64;  convolution++)
    {
        printf("    {");
        for (diff = 0;  diff < 16;  diff++)
        {
            split_bits(y, diff, 4);
            split_bits(t, convolution, 6);

            nt[6] = t[6] ^ t[5] ^ ((t[5] ^ y[1]) & t[4]) ^ y[4];
            nt[5] = t[6] ^ t[5] ^ t[3] ^ y[3] ^ (y[2] & t[4]);
            nt[4] = t[4] ^ t[5] ^ y[1];
            nt[3] = t[4];
            nt[2] = t[1];
            nt[1] = t[2] ^ t[4] ^ y[2];

            new_convolution = pack_bits(nt, 6);
            printf("0x%02X", new_convolution);
            if (diff < 15)
                printf(", ");
            /*endif*/
            v34_conv64_encode_table[convolution][diff] = new_convolution;
        }
        if (convolution < 63)
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

static void make_v34_64_state_convolutional_decoder(void)
{
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    printf("/* THIS FILE WAS AUTOMATICALLY GENERATED - ANY MODIFICATIONS MADE TO THIS\n");
    printf("   FILE MAY BE OVERWRITTEN DURING FUTURE BUILDS OF THE SOFTWARE */\n");
    printf("\n");

    make_v34_16_state_convolutional_encoder();
    make_v34_32_state_convolutional_encoder();
    make_v34_64_state_convolutional_encoder();

    make_v34_16_state_convolutional_decoder();
    make_v34_32_state_convolutional_decoder();
    make_v34_64_state_convolutional_decoder();

    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
