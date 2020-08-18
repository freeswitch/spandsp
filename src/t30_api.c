/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t30_api.c - ITU T.30 FAX transfer processing
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003, 2004, 2005, 2006, 2007 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#else
#include "spandsp3/stdbool.h"
#endif
#include "floating_fudge.h"
#include <tiffio.h>

#include "spandsp3/telephony.h"
#include "spandsp3/alloc.h"
#include "spandsp3/logging.h"
#include "spandsp3/bit_operations.h"
#include "spandsp3/queue.h"
#include "spandsp3/power_meter.h"
#include "spandsp3/complex.h"
#include "spandsp3/tone_generate.h"
#include "spandsp3/async.h"
#include "spandsp3/hdlc.h"
#include "spandsp3/fsk.h"
#include "spandsp3/v29rx.h"
#include "spandsp3/v29tx.h"
#include "spandsp3/v27ter_rx.h"
#include "spandsp3/v27ter_tx.h"
#include "spandsp3/timezone.h"
#include "spandsp3/t4_rx.h"
#include "spandsp3/t4_tx.h"
#include "spandsp3/image_translate.h"
#include "spandsp3/t81_t82_arith_coding.h"
#include "spandsp3/t85.h"
#include "spandsp3/t42.h"
#include "spandsp3/t43.h"
#include "spandsp3/t4_t6_decode.h"
#include "spandsp3/t4_t6_encode.h"
#include "spandsp3/t30_fcf.h"
#include "spandsp3/t35.h"
#include "spandsp3/t30.h"
#include "spandsp3/t30_api.h"
#include "spandsp3/t30_logging.h"

#include "spandsp3/private/logging.h"
#include "spandsp3/private/timezone.h"
#include "spandsp3/private/t81_t82_arith_coding.h"
#include "spandsp3/private/t85.h"
#include "spandsp3/private/t42.h"
#include "spandsp3/private/t43.h"
#include "spandsp3/private/t4_t6_decode.h"
#include "spandsp3/private/t4_t6_encode.h"
#include "spandsp3/private/image_translate.h"
#include "spandsp3/private/t4_rx.h"
#include "spandsp3/private/t4_tx.h"
#include "spandsp3/private/t30.h"

#include "t30_local.h"

SPAN_DECLARE(int) t33_sub_address_extract_field(uint8_t num[21], const uint8_t t33[], int field_no)
{
    int i;
    int j;
    int k;
    int ch;
    int type;

    num[0] = '\0';
    k = 0;
    for (i = 0;  t33[i];  )
    {
        if (k++ == field_no)
        {
            ch = t33[i++];
            j = 0;
            if (ch != '#')
            {
                num[j++] = ch;
                type = T33_EXT;
            }
            else
            {
                type = T33_SST;
            }
            /*endif*/
            while (t33[i])
            {
                ch = t33[i++];
                if (ch == '#')
                    break;
                /*endif*/
                num[j++] = ch;
                if (j >= 20)
                    return -1;
                /*endif*/
            }
            /*endwhile*/
            num[j] = '\0';
            return type;
        }
        /*endif*/
        /* Skip this field */
        i++;
        while (t33[i])
        {
            if (t33[i++] == '#')
                break;
            /*endif*/
        }
        /*endwhile*/
    }
    /*endfor*/
    return T33_NONE;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t33_sub_address_add_field(uint8_t t33[], const uint8_t field[], int type)
{
    if (t33[0] != '\0')
        strcat((char *) t33, "#");
    /*endif*/
    if (type == T33_SST)
        strcat((char *) t33, "#");
    /*endif*/
    strcat((char *) t33, (const char *) field);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_tx_ident(t30_state_t *s, const char *id)
{
    if (id == NULL)
    {
        s->tx_info.ident[0] = '\0';
        return 0;
    }
    /*endif*/
    if (strlen(id) > T30_MAX_IDENT_LEN)
        return -1;
    /*endif*/
    strcpy(s->tx_info.ident, id);
    t4_tx_set_local_ident(&s->t4.tx, s->tx_info.ident);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t30_get_tx_ident(t30_state_t *s)
{
    if (s->tx_info.ident[0] == '\0')
        return NULL;
    /*endif*/
    return s->tx_info.ident;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t30_get_rx_ident(t30_state_t *s)
{
    if (s->rx_info.ident[0] == '\0')
        return NULL;
    /*endif*/
    return s->rx_info.ident;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_tx_sub_address(t30_state_t *s, const char *sub_address)
{
    if (sub_address == NULL)
    {
        s->tx_info.sub_address[0] = '\0';
        return 0;
    }
    /*endif*/
    if (strlen(sub_address) > T30_MAX_IDENT_LEN)
        return -1;
    /*endif*/
    strcpy(s->tx_info.sub_address, sub_address);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t30_get_tx_sub_address(t30_state_t *s)
{
    if (s->tx_info.sub_address[0] == '\0')
        return NULL;
    /*endif*/
    return s->tx_info.sub_address;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t30_get_rx_sub_address(t30_state_t *s)
{
    if (s->rx_info.sub_address[0] == '\0')
        return NULL;
    /*endif*/
    return s->rx_info.sub_address;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_tx_selective_polling_address(t30_state_t *s, const char *selective_polling_address)
{
    if (selective_polling_address == NULL)
    {
        s->tx_info.selective_polling_address[0] = '\0';
        return 0;
    }
    /*endif*/
    if (strlen(selective_polling_address) > T30_MAX_IDENT_LEN)
        return -1;
    /*endif*/
    strcpy(s->tx_info.selective_polling_address, selective_polling_address);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t30_get_tx_selective_polling_address(t30_state_t *s)
{
    if (s->tx_info.selective_polling_address[0] == '\0')
        return NULL;
    /*endif*/
    return s->tx_info.selective_polling_address;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t30_get_rx_selective_polling_address(t30_state_t *s)
{
    if (s->rx_info.selective_polling_address[0] == '\0')
        return NULL;
    /*endif*/
    return s->rx_info.selective_polling_address;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_tx_polled_sub_address(t30_state_t *s, const char *polled_sub_address)
{
    if (polled_sub_address == NULL)
    {
        s->tx_info.polled_sub_address[0] = '\0';
        return 0;
    }
    /*endif*/
    if (strlen(polled_sub_address) > T30_MAX_IDENT_LEN)
        return -1;
    /*endif*/
    strcpy(s->tx_info.polled_sub_address, polled_sub_address);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t30_get_tx_polled_sub_address(t30_state_t *s)
{
    if (s->tx_info.polled_sub_address[0] == '\0')
        return NULL;
    /*endif*/
    return s->tx_info.polled_sub_address;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t30_get_rx_polled_sub_address(t30_state_t *s)
{
    if (s->rx_info.polled_sub_address[0] == '\0')
        return NULL;
    /*endif*/
    return s->rx_info.polled_sub_address;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_tx_sender_ident(t30_state_t *s, const char *sender_ident)
{
    if (sender_ident == NULL)
    {
        s->tx_info.sender_ident[0] = '\0';
        return 0;
    }
    /*endif*/
    if (strlen(sender_ident) > T30_MAX_IDENT_LEN)
        return -1;
    /*endif*/
    strcpy(s->tx_info.sender_ident, sender_ident);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t30_get_tx_sender_ident(t30_state_t *s)
{
    if (s->tx_info.sender_ident[0] == '\0')
        return NULL;
    /*endif*/
    return s->tx_info.sender_ident;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t30_get_rx_sender_ident(t30_state_t *s)
{
    if (s->rx_info.sender_ident[0] == '\0')
        return NULL;
    /*endif*/
    return s->rx_info.sender_ident;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_tx_password(t30_state_t *s, const char *password)
{
    if (password == NULL)
    {
        s->tx_info.password[0] = '\0';
        return 0;
    }
    /*endif*/
    if (strlen(password) > T30_MAX_IDENT_LEN)
        return -1;
    /*endif*/
    strcpy(s->tx_info.password, password);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t30_get_tx_password(t30_state_t *s)
{
    if (s->tx_info.password[0] == '\0')
        return NULL;
    /*endif*/
    return s->tx_info.password;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t30_get_rx_password(t30_state_t *s)
{
    if (s->rx_info.password[0] == '\0')
        return NULL;
    /*endif*/
    return s->rx_info.password;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_tx_nsf(t30_state_t *s, const uint8_t *nsf, int len)
{
    if (s->tx_info.nsf)
        span_free(s->tx_info.nsf);
    /*endif*/
    if (nsf  &&  len > 0  &&  (s->tx_info.nsf = span_alloc(len + 3)))
    {
        memcpy(&s->tx_info.nsf[3], nsf, len);
        s->tx_info.nsf_len = len;
    }
    else
    {
        s->tx_info.nsf = NULL;
        s->tx_info.nsf_len = 0;
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_tx_nsf(t30_state_t *s, const uint8_t *nsf[])
{
    if (nsf)
        *nsf = s->tx_info.nsf;
    /*endif*/
    return s->tx_info.nsf_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_rx_nsf(t30_state_t *s, const uint8_t *nsf[])
{
    if (nsf)
        *nsf = s->rx_info.nsf;
    /*endif*/
    return s->rx_info.nsf_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_tx_nsc(t30_state_t *s, const uint8_t *nsc, int len)
{
    if (s->tx_info.nsc)
        span_free(s->tx_info.nsc);
    /*endif*/
    if (nsc  &&  len > 0  &&  (s->tx_info.nsc = span_alloc(len + 3)))
    {
        memcpy(&s->tx_info.nsc[3], nsc, len);
        s->tx_info.nsc_len = len;
    }
    else
    {
        s->tx_info.nsc = NULL;
        s->tx_info.nsc_len = 0;
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_tx_nsc(t30_state_t *s, const uint8_t *nsc[])
{
    if (nsc)
        *nsc = s->tx_info.nsc;
    /*endif*/
    return s->tx_info.nsc_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_rx_nsc(t30_state_t *s, const uint8_t *nsc[])
{
    if (nsc)
        *nsc = s->rx_info.nsc;
    /*endif*/
    return s->rx_info.nsc_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_tx_nss(t30_state_t *s, const uint8_t *nss, int len)
{
    if (s->tx_info.nss)
        span_free(s->tx_info.nss);
    /*endif*/
    if (nss  &&  len > 0  &&  (s->tx_info.nss = span_alloc(len + 3)))
    {
        memcpy(&s->tx_info.nss[3], nss, len);
        s->tx_info.nss_len = len;
    }
    else
    {
        s->tx_info.nss = NULL;
        s->tx_info.nss_len = 0;
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_tx_nss(t30_state_t *s, const uint8_t *nss[])
{
    if (nss)
        *nss = s->tx_info.nss;
    /*endif*/
    return s->tx_info.nss_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_rx_nss(t30_state_t *s, const uint8_t *nss[])
{
    if (nss)
        *nss = s->rx_info.nss;
    /*endif*/
    return s->rx_info.nss_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_tx_tsa(t30_state_t *s, int type, const char *address, int len)
{
    if (s->tx_info.tsa)
        span_free(s->tx_info.tsa);
    /*endif*/
    if (address == NULL  ||  len == 0)
    {
        s->tx_info.tsa = NULL;
        s->tx_info.tsa_len = 0;
        return 0;
    }
    /*endif*/
    s->tx_info.tsa_type = type;
    if (len < 0)
        len = strlen(address);
    /*endif*/
    if ((s->tx_info.tsa = span_alloc(len)))
    {
        memcpy(s->tx_info.tsa, address, len);
        s->tx_info.tsa_len = len;
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_tx_tsa(t30_state_t *s, int *type, const char *address[])
{
    if (type)
        *type = s->tx_info.tsa_type;
    /*endif*/
    if (address)
        *address = s->tx_info.tsa;
    /*endif*/
    return s->tx_info.tsa_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_rx_tsa(t30_state_t *s, int *type, const char *address[])
{
    if (type)
        *type = s->rx_info.tsa_type;
    /*endif*/
    if (address)
        *address = s->rx_info.tsa;
    /*endif*/
    return s->rx_info.tsa_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_tx_ira(t30_state_t *s, int type, const char *address, int len)
{
    if (s->tx_info.ira)
        span_free(s->tx_info.ira);
    /*endif*/
    if (address == NULL)
    {
        s->tx_info.ira = NULL;
        return 0;
    }
    /*endif*/
    s->tx_info.ira = strdup(address);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_tx_ira(t30_state_t *s, int *type, const char *address[])
{
    if (type)
        *type = s->tx_info.ira_type;
    /*endif*/
    if (address)
        *address = s->tx_info.ira;
    /*endif*/
    return s->tx_info.ira_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_rx_ira(t30_state_t *s, int *type, const char *address[])
{
    if (type)
        *type = s->rx_info.ira_type;
    /*endif*/
    if (address)
        *address = s->rx_info.ira;
    /*endif*/
    return s->rx_info.ira_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_tx_cia(t30_state_t *s, int type, const char *address, int len)
{
    if (s->tx_info.cia)
        span_free(s->tx_info.cia);
    /*endif*/
    if (address == NULL)
    {
        s->tx_info.cia = NULL;
        return 0;
    }
    /*endif*/
    s->tx_info.cia = strdup(address);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_tx_cia(t30_state_t *s, int *type, const char *address[])
{
    if (type)
        *type = s->tx_info.cia_type;
    /*endif*/
    if (address)
        *address = s->tx_info.cia;
    /*endif*/
    return s->tx_info.cia_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_rx_cia(t30_state_t *s, int *type, const char *address[])
{
    if (type)
        *type = s->rx_info.cia_type;
    /*endif*/
    if (address)
        *address = s->rx_info.cia;
    /*endif*/
    return s->rx_info.cia_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_tx_isp(t30_state_t *s, int type, const char *address, int len)
{
    if (s->tx_info.isp)
        span_free(s->tx_info.isp);
    /*endif*/
    if (address == NULL)
    {
        s->tx_info.isp = NULL;
        return 0;
    }
    /*endif*/
    s->tx_info.isp = strdup(address);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_tx_isp(t30_state_t *s, int *type, const char *address[])
{
    if (type)
        *type = s->tx_info.isp_type;
    /*endif*/
    if (address)
        *address = s->tx_info.isp;
    /*endif*/
    return s->tx_info.isp_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_rx_isp(t30_state_t *s, int *type, const char *address[])
{
    if (type)
        *type = s->rx_info.isp_type;
    /*endif*/
    if (address)
        *address = s->rx_info.isp;
    /*endif*/
    return s->rx_info.isp_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_tx_csa(t30_state_t *s, int type, const char *address, int len)
{
    if (s->tx_info.csa)
        span_free(s->tx_info.csa);
    /*endif*/
    if (address == NULL)
    {
        s->tx_info.csa = NULL;
        return 0;
    }
    /*endif*/
    s->tx_info.csa = strdup(address);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_tx_csa(t30_state_t *s, int *type, const char *address[])
{
    if (type)
        *type = s->tx_info.csa_type;
    /*endif*/
    if (address)
        *address = s->tx_info.csa;
    /*endif*/
    return s->tx_info.csa_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_rx_csa(t30_state_t *s, int *type, const char *address[])
{
    if (type)
        *type = s->rx_info.csa_type;
    /*endif*/
    if (address)
        *address = s->rx_info.csa;
    /*endif*/
    return s->rx_info.csa_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_tx_page_header_overlays_image(t30_state_t *s, bool header_overlays_image)
{
    s->header_overlays_image = header_overlays_image;
    t4_tx_set_header_overlays_image(&s->t4.tx, s->header_overlays_image);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_tx_page_header_info(t30_state_t *s, const char *info)
{
    if (info == NULL)
    {
        s->header_info[0] = '\0';
        return 0;
    }
    /*endif*/
    if (strlen(info) > T30_MAX_PAGE_HEADER_INFO)
        return -1;
    /*endif*/
    strcpy(s->header_info, info);
    t4_tx_set_header_info(&s->t4.tx, s->header_info);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) t30_get_tx_page_header_info(t30_state_t *s, char *info)
{
    if (info)
        strcpy(info, s->header_info);
    /*endif*/
    return strlen(s->header_info);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_tx_page_header_tz(t30_state_t *s, const char *tzstring)
{
    if (tz_init(&s->tz, tzstring))
    {
        s->use_own_tz = true;
        t4_tx_set_header_tz(&s->t4.tx, &s->tz);
        return 0;
    }
    /*endif*/
    return -1;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t30_get_rx_country(t30_state_t *s)
{
    return s->country;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t30_get_rx_vendor(t30_state_t *s)
{
    return s->vendor;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t30_get_rx_model(t30_state_t *s)
{
    return s->model;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t30_set_rx_file(t30_state_t *s, const char *file, int stop_page)
{
    strncpy(s->rx_file, file, sizeof(s->rx_file));
    s->rx_file[sizeof(s->rx_file) - 1] = '\0';
    s->rx_stop_page = stop_page;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t30_set_tx_file(t30_state_t *s, const char *file, int start_page, int stop_page)
{
    strncpy(s->tx_file, file, sizeof(s->tx_file));
    s->tx_file[sizeof(s->tx_file) - 1] = '\0';
    s->tx_start_page = start_page;
    s->tx_stop_page = stop_page;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t30_set_iaf_mode(t30_state_t *s, bool iaf)
{
    s->iaf = iaf;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_ecm_capability(t30_state_t *s, bool enabled)
{
    s->ecm_allowed = enabled;
    t30_build_dis_or_dtc(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t30_set_keep_bad_quality_pages(t30_state_t *s, bool keep_bad_pages)
{
    s->keep_bad_pages = keep_bad_pages;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_supported_output_compressions(t30_state_t *s, int supported_compressions)
{
    /* Mask out the ones we actually support today. */
    supported_compressions &= T4_COMPRESSION_T4_1D
                            | T4_COMPRESSION_T4_2D
                            | T4_COMPRESSION_T6
                            | T4_COMPRESSION_T85
                            | T4_COMPRESSION_T85_L0
#if defined(SPANDSP_SUPPORT_T88)
                            | T4_COMPRESSION_T88
#endif
                            | T4_COMPRESSION_T42_T81
#if defined(SPANDSP_SUPPORT_SYCC_T81)
                            | T4_COMPRESSION_SYCC_T81
#endif
#if defined(SPANDSP_SUPPORT_T43)
                            | T4_COMPRESSION_T43
#endif
#if defined(SPANDSP_SUPPORT_T45)
                            | T4_COMPRESSION_T45
#endif
                            | T4_COMPRESSION_UNCOMPRESSED
                            | T4_COMPRESSION_JPEG
                            | 0;
    s->supported_output_compressions = supported_compressions;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_minimum_scan_line_time(t30_state_t *s, int min_time)
{
    /* There are only certain possible times supported, so we need to select
       the code which best matches the request. */
    if (min_time == 0)
        s->local_min_scan_time_code = 7;
    else if (min_time <= 5)
        s->local_min_scan_time_code = 1;
    else if (min_time <= 10)
        s->local_min_scan_time_code = 2;
    else if (min_time <= 20)
        s->local_min_scan_time_code = 0;
    else if (min_time <= 40)
        s->local_min_scan_time_code = 4;
    else
        return -1;
    /*endif*/
    t30_build_dis_or_dtc(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_supported_modems(t30_state_t *s, int supported_modems)
{
    s->supported_modems = supported_modems;
    t30_build_dis_or_dtc(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_supported_compressions(t30_state_t *s, int supported_compressions)
{
    /* Mask out the ones we actually support today. */
    supported_compressions &= T4_COMPRESSION_T4_1D
                            | T4_COMPRESSION_T4_2D
                            | T4_COMPRESSION_T6
                            | T4_COMPRESSION_T85
                            | T4_COMPRESSION_T85_L0
#if defined(SPANDSP_SUPPORT_T88)
                            | T4_COMPRESSION_T88
#endif
                            | T4_COMPRESSION_T42_T81
#if defined(SPANDSP_SUPPORT_SYCC_T81)
                            | T4_COMPRESSION_SYCC_T81
#endif
#if defined(SPANDSP_SUPPORT_T43)
                            | T4_COMPRESSION_T43
#endif
#if defined(SPANDSP_SUPPORT_T45)
                            | T4_COMPRESSION_T45
#endif
                            | T4_COMPRESSION_GRAYSCALE
                            | T4_COMPRESSION_COLOUR
                            | T4_COMPRESSION_12BIT
                            | T4_COMPRESSION_COLOUR_TO_GRAY
                            | T4_COMPRESSION_GRAY_TO_BILEVEL
                            | T4_COMPRESSION_COLOUR_TO_BILEVEL
                            | T4_COMPRESSION_RESCALING
                            | 0;

    s->supported_compressions = supported_compressions;
    t30_build_dis_or_dtc(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_supported_bilevel_resolutions(t30_state_t *s, int supported_resolutions)
{
    supported_resolutions &= T4_RESOLUTION_R8_STANDARD
                           | T4_RESOLUTION_R8_FINE
                           | T4_RESOLUTION_R8_SUPERFINE
                           | T4_RESOLUTION_R16_SUPERFINE
                           | T4_RESOLUTION_200_100
                           | T4_RESOLUTION_200_200
                           | T4_RESOLUTION_200_400
                           | T4_RESOLUTION_300_300
                           | T4_RESOLUTION_300_600
                           | T4_RESOLUTION_400_400
                           | T4_RESOLUTION_400_800
                           | T4_RESOLUTION_600_600
                           | T4_RESOLUTION_600_1200
                           | T4_RESOLUTION_1200_1200;
    /* Make sure anything needed for colour is enabled as a bi-level image, as that is a
       rule from T.30. 100x100 is an exception, as it doesn't exist as a bi-level resolution. */
    supported_resolutions |= (s->supported_colour_resolutions & ~T4_RESOLUTION_100_100);
    s->supported_bilevel_resolutions = supported_resolutions;
    t30_build_dis_or_dtc(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_supported_colour_resolutions(t30_state_t *s, int supported_resolutions)
{
    supported_resolutions &= T4_RESOLUTION_100_100
                           | T4_RESOLUTION_200_200
                           | T4_RESOLUTION_300_300
                           | T4_RESOLUTION_400_400
                           | T4_RESOLUTION_600_600
                           | T4_RESOLUTION_1200_1200;
    s->supported_colour_resolutions = supported_resolutions;
    /* Make sure anything needed for colour is enabled as a bi-level image, as that is a
       rule from T.30. 100x100 is an exception, as it doesn't exist as a bi-level resolution. */
    s->supported_bilevel_resolutions |= (s->supported_colour_resolutions & ~T4_RESOLUTION_100_100);
    t30_build_dis_or_dtc(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_supported_image_sizes(t30_state_t *s, int supported_image_sizes)
{
    /* Force the sizes which are always available */
    supported_image_sizes |= (T4_SUPPORT_WIDTH_215MM | T4_SUPPORT_LENGTH_A4);
    /* Force the sizes which depend on sizes which are supported */
    if ((supported_image_sizes & T4_SUPPORT_LENGTH_UNLIMITED))
        supported_image_sizes |= T4_SUPPORT_LENGTH_B4;
    /*endif*/
    if ((supported_image_sizes & T4_SUPPORT_WIDTH_303MM))
        supported_image_sizes |= T4_SUPPORT_WIDTH_255MM;
    /*endif*/
    s->supported_image_sizes = supported_image_sizes;
    t30_build_dis_or_dtc(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_supported_t30_features(t30_state_t *s, int supported_t30_features)
{
    s->supported_t30_features = supported_t30_features;
    t30_build_dis_or_dtc(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t30_set_status(t30_state_t *s, int status)
{
    if (s->current_status != status)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Status changing to '%s'\n", t30_completion_code_to_str(status));
        s->current_status = status;
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t30_set_receiver_not_ready(t30_state_t *s, int count)
{
    s->receiver_not_ready_count = count;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t30_set_phase_b_handler(t30_state_t *s, t30_phase_b_handler_t handler, void *user_data)
{
    s->phase_b_handler = handler;
    s->phase_b_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t30_set_phase_d_handler(t30_state_t *s, t30_phase_d_handler_t handler, void *user_data)
{
    s->phase_d_handler = handler;
    s->phase_d_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t30_set_phase_e_handler(t30_state_t *s, t30_phase_e_handler_t handler, void *user_data)
{
    s->phase_e_handler = handler;
    s->phase_e_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t30_set_document_handler(t30_state_t *s, t30_document_handler_t handler, void *user_data)
{
    s->document_handler = handler;
    s->document_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t30_set_real_time_frame_handler(t30_state_t *s, t30_real_time_frame_handler_t handler, void *user_data)
{
    s->real_time_frame_handler = handler;
    s->real_time_frame_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t30_set_document_get_handler(t30_state_t *s, t30_document_get_handler_t handler, void *user_data)
{
    s->document_get_handler = handler;
    s->document_get_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t30_set_document_put_handler(t30_state_t *s, t30_document_put_handler_t handler, void *user_data)
{
    s->document_put_handler = handler;
    s->document_put_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) t30_get_logging_state(t30_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
