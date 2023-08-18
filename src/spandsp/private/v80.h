/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/v80.h - In band DCE control and synchronous data modes for asynchronous DTEs
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2023 Steve Underwood
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

#if !defined(_SPANDSP_PRIVATE_V80_H_)
#define _SPANDSP_PRIVATE_V80_H_

struct v80_state_s
{
    /*! \brief True if we are the calling party */
    bool calling_party;
};

#endif
/*- End of file ------------------------------------------------------------*/
