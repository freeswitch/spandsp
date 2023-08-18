/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v80_tests.c - In band DCE control and synchronous data modes for asynchronous DTE
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

/*! \page v80_tests_page V.80 tests
\section v80_tests_page_sec_1 What does it do?
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sndfile.h>

#include "spandsp.h"
#include "spandsp-sim.h"

int main(int argc, char *argv[])
{
#define display(a) printf("%-40s %s\n", #a, v80_escape_to_str(a))

    display(V80_FROM_DTE_MFGEXTEND);
    display(V80_FROM_DTE_MFG1);
    display(V80_FROM_DTE_MFG2);
    display(V80_FROM_DTE_MFG3);
    display(V80_FROM_DTE_MFG4);
    display(V80_FROM_DTE_MFG5);
    display(V80_FROM_DTE_MFG6);
    display(V80_FROM_DTE_MFG7);
    display(V80_FROM_DTE_MFG8);
    display(V80_FROM_DTE_MFG9);
    display(V80_FROM_DTE_MFG10);
    display(V80_FROM_DTE_MFG11);
    display(V80_FROM_DTE_MFG12);
    display(V80_FROM_DTE_MFG13);
    display(V80_FROM_DTE_MFG14);
    display(V80_FROM_DTE_MFG15);
    display(V80_FROM_DTE_EXTEND0);
    display(V80_FROM_DTE_EXTEND1);
    display(V80_FROM_DTE_CIRCUIT_105_OFF);
    display(V80_FROM_DTE_CIRCUIT_105_ON);
    display(V80_FROM_DTE_CIRCUIT_108_OFF);
    display(V80_FROM_DTE_CIRCUIT_108_ON);
    display(V80_FROM_DTE_CIRCUIT_133_OFF);
    display(V80_FROM_DTE_CIRCUIT_133_ON);
    display(V80_FROM_DTE_SINGLE_EM_P);
    display(V80_FROM_DTE_DOUBLE_EM_P);
    display(V80_FROM_DTE_FLOW_OFF);
    display(V80_FROM_DTE_FLOW_ON);
    display(V80_FROM_DTE_SINGLE_EM);
    display(V80_FROM_DTE_DOUBLE_EM);
    display(V80_FROM_DTE_POLL);

    display(V80_FROM_DCE_EXTENDMFG);
    display(V80_FROM_DCE_MFG1);
    display(V80_FROM_DCE_MFG2);
    display(V80_FROM_DCE_MFG3);
    display(V80_FROM_DCE_MFG4);
    display(V80_FROM_DCE_MFG5);
    display(V80_FROM_DCE_MFG6);
    display(V80_FROM_DCE_MFG7);
    display(V80_FROM_DCE_MFG8);
    display(V80_FROM_DCE_MFG9);
    display(V80_FROM_DCE_MFG10);
    display(V80_FROM_DCE_MFG11);
    display(V80_FROM_DCE_MFG12);
    display(V80_FROM_DCE_MFG13);
    display(V80_FROM_DCE_MFG14);
    display(V80_FROM_DCE_MFG15);
    display(V80_FROM_DCE_EXTEND0);
    display(V80_FROM_DCE_EXTEND1);
    display(V80_FROM_DCE_CIRCUIT_106_OFF);
    display(V80_FROM_DCE_CIRCUIT_106_ON);
    display(V80_FROM_DCE_CIRCUIT_107_OFF);
    display(V80_FROM_DCE_CIRCUIT_107_ON);
    display(V80_FROM_DCE_CIRCUIT_109_OFF);
    display(V80_FROM_DCE_CIRCUIT_109_ON);
    display(V80_FROM_DCE_CIRCUIT_110_OFF);
    display(V80_FROM_DCE_CIRCUIT_110_ON);
    display(V80_FROM_DCE_CIRCUIT_125_OFF);
    display(V80_FROM_DCE_CIRCUIT_125_ON);
    display(V80_FROM_DCE_CIRCUIT_132_OFF);
    display(V80_FROM_DCE_CIRCUIT_132_ON);
    display(V80_FROM_DCE_CIRCUIT_142_OFF);
    display(V80_FROM_DCE_CIRCUIT_142_ON);
    display(V80_FROM_DCE_SINGLE_EM_P);
    display(V80_FROM_DCE_DOUBLE_EM_P);
    display(V80_FROM_DCE_OFF_LINE);
    display(V80_FROM_DCE_ON_LINE);
    display(V80_FROM_DCE_FLOW_OFF);
    display(V80_FROM_DCE_FLOW_ON);
    display(V80_FROM_DCE_SINGLE_EM);
    display(V80_FROM_DCE_DOUBLE_EM);
    display(V80_FROM_DCE_POLL);

    display(V80_TRANSPARENCY_T1);
    display(V80_TRANSPARENCY_T2);
    display(V80_TRANSPARENCY_T3);
    display(V80_TRANSPARENCY_T4);
    display(V80_TRANSPARENCY_T5);
    display(V80_TRANSPARENCY_T6);
    display(V80_TRANSPARENCY_T7);
    display(V80_TRANSPARENCY_T8);
    display(V80_TRANSPARENCY_T9);
    display(V80_TRANSPARENCY_T10);
    display(V80_TRANSPARENCY_T11);
    display(V80_TRANSPARENCY_T12);
    display(V80_TRANSPARENCY_T13);
    display(V80_TRANSPARENCY_T14);
    display(V80_TRANSPARENCY_T15);
    display(V80_TRANSPARENCY_T16);
    display(V80_TRANSPARENCY_T17);
    display(V80_TRANSPARENCY_T18);
    display(V80_TRANSPARENCY_T19);
    display(V80_TRANSPARENCY_T20);

    display(V80_MARK);
    display(V80_FLAG);
    display(V80_ERR);
    display(V80_HUNT);
    display(V80_UNDER);
    display(V80_TOVER);
    display(V80_ROVER);
    display(V80_RESUME);
    display(V80_BNUM);
    display(V80_UNUM);

    display(V80_EOT);
    display(V80_ECS);
    display(V80_RRN);
    display(V80_RTN);
    display(V80_RATE);

    display(V80_PRI);
    display(V80_CTL);
    display(V80_RTNH);
    display(V80_RTNC);
    display(V80_RATEH);
    display(V80_EOTH);

    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
