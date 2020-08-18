/*
 * SpanDSP - a series of DSP components for telephony
 *
 * complex_filters.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003 Steve Underwood
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#include "spandsp3/telephony.h"
#include "spandsp3/alloc.h"
#include "spandsp3/complex.h"
#include "spandsp3/complex_filters.h"

SPAN_DECLARE(filter_t *) filter_create(fspec_t *fs)
{
    int i;
    filter_t *fi;

    if ((fi = (filter_t *) span_alloc(sizeof(*fi) + sizeof(float)*(fs->np + 1))))
    {
        fi->fs = fs;
        fi->sum = 0.0;
        /* Moving average filters only */
        fi->ptr = 0;
        for (i = 0;  i <= fi->fs->np;  i++)
            fi->v[i] = 0.0;
    }
    return fi;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) filter_delete(filter_t *fi)
{
    if (fi)
        span_free(fi);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(float) filter_step(filter_t *fi, float x)
{
    return fi->fs->fsf(fi, x);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(cfilter_t *) cfilter_create(fspec_t *fs)
{
    cfilter_t *cfi;

    if ((cfi = (cfilter_t *) span_alloc(sizeof(*cfi))))
    {
        if ((cfi->ref = filter_create(fs)) == NULL)
        {
            span_free(cfi);
            return NULL;
        }
        if ((cfi->imf = filter_create(fs)) == NULL)
        {
            span_free(cfi->ref);
            span_free(cfi);
            return NULL;
        }
    }
    return cfi;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) cfilter_delete(cfilter_t *cfi)
{
    if (cfi)
    {
        filter_delete(cfi->ref);
        filter_delete(cfi->imf);
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(complexf_t) cfilter_step(cfilter_t *cfi, const complexf_t *z)
{
    complexf_t cc;

    cc.re = filter_step(cfi->ref, z->re);
    cc.im = filter_step(cfi->imf, z->im);
    return cc;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
