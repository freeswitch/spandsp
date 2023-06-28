/*
 * SpanDSP - a series of DSP components for telephony
 *
 * g1050.c - IP network modeling, as per G.1050/TIA-921.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2007 Steve Underwood
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#define GEN_CONST
#include <math.h>
#endif
#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#else
#include "spandsp/stdbool.h"
#endif
#include "floating_fudge.h"

#include "spandsp.h"
#include "spandsp/g1050.h"

/*
G.1050 2005 [2007] Appendix II

Packet delay and loss algorithms

II.1    General IP network model

The IP network is modelled as a concatenation of five segments: local LAN segment, local access
link segment, core IP network segment, remote access link segment, remote LAN segment. Each
segment introduces packet loss with some probability and a time-varying delay. The input to the
model is a set of segment parameters (LAN and access rates, occupancy and a set of core network
metrics), packet size(s), packet rate and the total number of packets to be passed from end to end.
Time slices of 1 ms are assigned a delay value and loss probability using the model parameters.
When a packet arrives, it is assigned the delay value and loss probability of the millisecond in
which it arrives. The output is the total delay value for each packet and an indication of whether or
not a packet was lost.

[2007: Some IP applications (e.g., IPTV, Internet access) do not involve the full generality of all five
segments. These "core-to-LAN" models include the core IP network segment, one access link
segment, and one LAN segment.]

II.2    Packet loss model

II.2.1  Bursty packet loss model

It is well known that packet loss in IP networks is bursty in nature. Within the context of this model
the definition of "burst" is a period of time bounded by lost packets during which the packet loss
rate is high.
[2005: This is distinguished from a "consecutive loss period", which is a period of time bounded
by lost packets during which all packets are lost.]
[2007: Such a burst may include sequential lost packets.]

Bursty packet loss is modelled with a two-state model, a Gilbert-Elliott model, which switches
between a high-loss-rate state (HIGH_LOSS state) and a low-loss-rate state (LOW_LOSS state).
The Gilbert-Elliott model has four parameters per segment: loss probability in the HIGH_LOSS
state, loss probability in the LOW_LOSS state, the probability of transitioning from the
HIGH_LOSS to the LOW_LOSS state, and the probability of transitioning from the LOW_LOSS to
the HIGH_LOSS state. Loss rates of the core network are given parameters. Loss rates of LAN and
access links depend on LAN and access link occupancy parameters. Pseudo-code for such a model
is shown below:

    if rand() < loss_probability[LOSS_STATE]
        loss = TRUE
    else
        loss = FALSE
    endif
    if rand() < transition_probability[LOSS_STATE]
        if LOSS_STATE == HIGH_LOSS
            LOSS_STATE = LOW_LOSS
        else
            LOSS_STATE = HIGH_LOSS
        endif
    endif

[2005: II.2.2  Consecutive packet loss]
[2007: II.2.2  Link failure model]

Link failure is another source of loss in the core network. This leads to [2005: consecutive] [2007: sequential] packet loss for
some period of time. This is modelled with two parameters, a periodic link failure rate together with
a duration of link outage once it occurs.

II.3    Delay variation model

Time series models are used to represent the characteristics of sequences that have some properties
that vary in time. They typically comprise one or more filter functions driven by a combination of
noise and some underlying signal or periodic element.

The "spiky" nature of delay traces suggests that jitter can be modelled using an impulse noise
sequence. The delay encountered by a packet at some specific stage in the network should be a
function of the serialization delay of interfering traffic and the volume of traffic. The height of the
impulses should therefore be a function of serialization delay and the frequency a function of
congestion level. LAN congestion tends to occur in short bursts - with Ethernet's CSMA/CD
algorithm one packet may be delayed, however the next may gain access to the LAN immediately;
this suggests a short filter response time. Access link congestion tends to be associated with
short-term delay variations due to the queue in the edge router filling; this suggests a longer filter
response time. Pseudo-code for delay variation is shown below:

    if rand() < impulse_probability
        i = impulse_height
    else
        i = 0
    endif
    d(n) = d(n-1) * (TC) + i * (1-TC)

where d(n) = delay of packet n, and TC represents the filter time constant.

II.3.1  LAN and access link jitter

Jitter in the LAN and access links is modelled with per-millisecond delay values created by passing
impulses through a one-pole filter. Within each segment, for each millisecond an impulse or a zero
is input to the filter based on some probability. The filter output is then computed and the result
becomes the delay value for that millisecond. Delay values are applied to packets based on the
current values in the millisecond during which the packet arrives, but arrival packet order is
maintained. The amplitude of the impulses is proportional to the serialization delay of that segment.
The probability of occurrence of an impulse is proportional to the congestion level for that segment.
For the LAN segments no filter is used; delay comes directly from the impulses. For the access link
segments, a filter with a time constant is used to scale the values for 1 ms intervals.
[2007: Also, for LAN segments a random delay value between 0 and 1.5 ms is added.]

II.3.2  Core network jitter

The core network jitter is modelled differently. For each packet, a random delay is added. This delay
is uniformly distributed from 0 to the core network jitter parameter value.

II.3.3  Core network base delay and route flapping

A base delay parameter is associated with the core network. Another source of delay variation is
route flapping in the core network. This is modelled by change in the base delay of the core
network. A periodic route flap rate is a given parameter. When a route flap occurs, the model will
add or subtract the route flap delay hit to or from the core network delay. For each route flap, the
model toggles between adding and subtracting the route flap delay.

II.4    Core packet reordering

In the model, only the core is allowed to reorder packets based on delays. Each time-slice has a
delay value. When a packet arrives, the current delay value is applied to that packet. The core
segment is the only segment that allows reordering. In the other segments, packets are transmitted
in the order they arrive, regardless of the delay values assigned.

II.5    Model output

If a packet is marked as lost in any segment, then it is lost.

The total delay added to a packet is the sum of the delay from each segment. There may be
out-of-order packets due to delay variations. LAN and access links should not cause packet
reordering. Therefore, delay due to LAN and access links is summed together first and delays are
adjusted to keep packets in order. Then delay due to the core network is added. This may result in
out-of-order packets.

II.6    Model parameters

[2007: Figure II.1 represents the five components of the end-to-end network and the associated modules in
the simulation/emulation algorithm. The values from Table 5 through Table 10 and Table 14
provide the inputs to the modules. The outputs of the algorithm are the impairments to be emulated.]

The following is a list of model input parameters and how those parameters are used.

II.6.1  Local and remote LAN segment parameters

[2005: Input parameters from Tables 5 and 6:]
[2007: Input parameters from Tables 5, 6 and 7:]

[2005:  1)  LAN speed. This speed is used to compute LAN segment delay.]
[2007:  1)  LAN rate. This speed is used to compute LAN segment delay.]

2)  LAN percent occupancy.

Derived parameters:

1)  LAN loss probability. One value for each loss state. Current values:
        For the low loss state, the probability is 0.
[2005:  For the high loss state, it is 0.004 × percent occupancy.]
[2007:  For the high loss state, it is 0.000025 × percent occupancy.]

2)  LAN loss state transition probability. One value for each loss state. Current values:
[2005:  The probability of transitioning from the low loss to the high loss state is 0.004 × percent occupancy.]
[2007:  The probability of transitioning from the low loss to the high loss state is 0.0001 × percent occupancy.]
        The probability of the reverse transition is 0.1.

3)  LAN jitter filter impulse height. One value for each loss state. Current values:
        Max impulse height = (MTU - size bit times) × (1 + (percent occupancy/40)).
[2005:  The value for the low loss state is a random variable uniformly distributed from 0 to Max impulse height.
        The value for the high loss state is Max impulse height.]
[2007:  The value for the low loss state is 0
        The value for the high loss state is a random variable uniformly distributed from 0 to Max impulse height.]

4)  LAN jitter filter impulse probability. Current values:
        The value for the low loss state is 0.
        The value for the high loss state is 0.5.

5)  LAN jitter filter coefficients. The filter output is the delay value for the current packet. This
    delay is A × (impulse height) + (1 - A) × (previous delay). Current value: A = 1 (i.e., no
    filtering).

II.6.2  Local and remote link segment parameters

[2005:  1)  Link speed. This speed is used to compute LAN segment delay.]
[2007:  1)  Link rate. This rate is used to compute LAN segment delay.]

2)  Link percent occupancy.

3)  Link MTU size.

4)  Link loss state transition probability. One value for each loss state. Current values:
[2005:  The probability of transitioning from the low loss to the high loss state is 0.0003 × (percent occupancy).]
[2007:  The probability of transitioning from the low loss to the high loss state is 0.0002 × (percent occupancy).]
        The probability of the reverse transition is 0.2/(1 + (percent occupancy)).

5)  Link jitter filter impulse height. One value for each loss state. Current values:
[2005:  Max impulse height = (MTU-size bit times) × (1 + (percent occupancy/40)).]
[2007:  Max impulse height = A × (MTU-size bit times) × (1 + (percent occupancy/40)).]
        The value for the low loss state is a random variable uniformly distributed from 0 to Max impulse height.
        The value for the high loss state is Max impulse height.
[2007:  Current value: A = 0.25.]

6)  Link jitter filter impulse probability. Current values:
        The value for the low loss state is 0.001 + (percent occupancy)/2000.
        The value for the high loss state is 0.3 + 0.4 × (percent occupancy)/100.

7)  Link jitter filter coefficients. The filter output is the delay value for the current packet. This
    delay is A × (impulse height) + (1 - A) × (previous delay). Current value: A = 0.25.
[2007: (same A as in item 5)]

8)  Link loss probability. One value for each loss state. Current values:
        For the low loss state, the probability is 0.
        For the high loss state, the probability is 0.0005 × percent occupancy.

9)  Link base delay. This is packet-size bit times. It is assumed that the packet size is fixed
    based on the application.

II.6.3  Core IP network segment parameters

1)  Delay.

2)  Packet loss. There is only one loss state. The loss probability is just the given core network
    loss probability parameter.

3)  Jitter. The jitter in the core network is modelled as added delay uniformly distributed
    between 0 and the core network jitter parameter value.

4)  Route flap interval.

5)  Route flap delay.

6)  Link failure interval.

7)  Link failure duration.

8)  Reorder percentage.
*/

#define PACKET_LOSS_TIME    -1

g1050_constants_t g1050_constants[1] =
{
    {
        {
            {   /* Side A LAN */
                {
                    0.004,          /*! Probability of loss rate change low->high */
                    0.1             /*! Probability of loss rate change high->low */
                },
                {
                    {
                        0.0,        /*! Probability of an impulse */
                        0.0,
                    },
                    {
                        0.5,
                        0.0
                    }
                },
                1.0,                /*! Impulse height, based on MTU and bit rate */
                0.0,                /*! Impulse decay coefficient */
                0.001,              /*! Probability of packet loss due to occupancy. */
                0.15                /*! Probability of packet loss due to a multiple access collision. */
            },
            {   /* Side A access link */
                {
                    0.0002,         /*! Probability of loss rate change low->high */
                    0.2             /*! Probability of loss rate change high->low */
                },
                {
                    {
                        0.001,      /*! Probability of an impulse */
                        0.0,
                    },
                    {
                        0.3,
                        0.4
                    }
                },
                40.0,               /*! Impulse height, based on MTU and bit rate */
                0.75,               /*! Impulse decay coefficient */
                0.0005,             /*! Probability of packet loss due to occupancy. */
                0.0                 /*! Probability of packet loss due to a multiple access collision. */
            },
            {   /* Side B access link */
                {
                    0.0002,         /*! Probability of loss rate change low->high */
                    0.2             /*! Probability of loss rate change high->low */
                },
                {
                    {
                        0.001,      /*! Probability of an impulse */
                        0.0,
                    },
                    {
                        0.3,
                        0.4
                    }
                },
                40.0,               /*! Impulse height, based on MTU and bit rate */
                0.75,               /*! Impulse decay coefficient */
                0.0005,             /*! Probability of packet loss due to occupancy. */
                0.0                 /*! Probability of packet loss due to a multiple access collision. */
            },
            {   /* Side B LAN */
                {
                    0.004,          /*! Probability of loss rate change low->high */
                    0.1             /*! Probability of loss rate change high->low */
                },
                {
                    {
                        0.0,        /*! Probability of an impulse */
                        0.0,
                    },
                    {
                        0.5,
                        0.0
                    }
                },
                1.0,                /*! Impulse height, based on MTU and bit rate */
                0.0,                /*! Impulse decay coefficient */
                0.001,              /*! Probability of packet loss due to occupancy. */
                0.15                /*! Probability of packet loss due to a multiple access collision. */
            }
        }
    }
};

g1050_channel_speeds_t g1050_speed_patterns[168] =
{
    {  4000000, 0,   128000,   768000, 0,   4000000, 0,   128000,   768000, 0, 0.360},
    {  4000000, 0,   128000,   768000, 0,  20000000, 0,   128000,   768000, 0, 0.720},
    {  4000000, 0,   128000,   768000, 0, 100000000, 0,   128000,   768000, 0, 0.360},
    { 20000000, 0,   128000,   768000, 0,  20000000, 0,   128000,   768000, 0, 0.360},
    { 20000000, 0,   128000,   768000, 0, 100000000, 0,   128000,   768000, 0, 0.360},
    {100000000, 0,   128000,   768000, 0, 100000000, 0,   128000,   768000, 0, 0.090},
    {  4000000, 0,   128000,  1536000, 0,   4000000, 0,   384000,   768000, 0, 0.720},
    {  4000000, 0,   128000,  1536000, 0,  20000000, 0,   384000,   768000, 0, 1.470},
    {  4000000, 0,   128000,  1536000, 0, 100000000, 0,   384000,   768000, 0, 0.840},
    { 20000000, 0,   128000,  1536000, 0,  20000000, 0,   384000,   768000, 0, 0.750},
    { 20000000, 0,   128000,  1536000, 0, 100000000, 0,   384000,   768000, 0, 0.855},
    {100000000, 0,   128000,  1536000, 0, 100000000, 0,   384000,   768000, 0, 0.240},
    {  4000000, 0,   128000,  3000000, 0,   4000000, 0,   384000,   768000, 0, 0.120},
    {  4000000, 0,   128000,  3000000, 0,  20000000, 0,   384000,   768000, 0, 0.420},
    {  4000000, 0,   128000,  3000000, 0, 100000000, 0,   384000,   768000, 0, 0.840},
    { 20000000, 0,   128000,  3000000, 0,  20000000, 0,   384000,   768000, 0, 0.300},
    { 20000000, 0,   128000,  3000000, 0, 100000000, 0,   384000,   768000, 0, 0.930},
    {100000000, 0,   128000,  3000000, 0, 100000000, 0,   384000,   768000, 0, 0.390},
    {  4000000, 0,   384000,   768000, 0,   4000000, 0,   128000,  1536000, 0, 0.720},
    {  4000000, 0,   384000,   768000, 0,  20000000, 0,   128000,  1536000, 0, 1.470},
    {  4000000, 0,   384000,   768000, 0, 100000000, 0,   128000,  1536000, 0, 0.840},
    { 20000000, 0,   384000,   768000, 0,  20000000, 0,   128000,  1536000, 0, 0.750},
    { 20000000, 0,   384000,   768000, 0, 100000000, 0,   128000,  1536000, 0, 0.855},
    {100000000, 0,   384000,   768000, 0, 100000000, 0,   128000,  1536000, 0, 0.240},
    {  4000000, 0,   384000,  1536000, 0,   4000000, 0,   384000,  1536000, 0, 1.440},
    {  4000000, 0,   384000,  1536000, 0,  20000000, 0,   384000,  1536000, 0, 3.000},
    {  4000000, 0,   384000,  1536000, 0, 100000000, 0,   384000,  1536000, 0, 1.920},
    { 20000000, 0,   384000,  1536000, 0,  20000000, 0,   384000,  1536000, 0, 1.563},
    { 20000000, 0,   384000,  1536000, 0, 100000000, 0,   384000,  1536000, 0, 2.000},
    {100000000, 0,   384000,  1536000, 0, 100000000, 0,   384000,  1536000, 0, 0.640},
    {  4000000, 0,   384000,  3000000, 0,   4000000, 0,   384000,  1536000, 0, 0.240},
    {  4000000, 0,   384000,  3000000, 0,  20000000, 0,   384000,  1536000, 0, 0.850},
    {  4000000, 0,   384000,  3000000, 0, 100000000, 0,   384000,  1536000, 0, 1.720},
    { 20000000, 0,   384000,  3000000, 0,  20000000, 0,   384000,  1536000, 0, 0.625},
    { 20000000, 0,   384000,  3000000, 0, 100000000, 0,   384000,  1536000, 0, 2.025},
    {100000000, 0,   384000,  3000000, 0, 100000000, 0,   384000,  1536000, 0, 1.040},
    {  4000000, 0,   384000,   768000, 0,   4000000, 0,   128000,  3000000, 0, 0.120},
    {  4000000, 0,   384000,   768000, 0,  20000000, 0,   128000,  3000000, 0, 0.420},
    {  4000000, 0,   384000,   768000, 0, 100000000, 0,   128000,  3000000, 0, 0.840},
    { 20000000, 0,   384000,   768000, 0,  20000000, 0,   128000,  3000000, 0, 0.300},
    { 20000000, 0,   384000,   768000, 0, 100000000, 0,   128000,  3000000, 0, 0.930},
    {100000000, 0,   384000,   768000, 0, 100000000, 0,   128000,  3000000, 0, 0.390},
    {  4000000, 0,   384000,  1536000, 0,   4000000, 0,   384000,  3000000, 0, 0.240},
    {  4000000, 0,   384000,  1536000, 0,  20000000, 0,   384000,  3000000, 0, 0.850},
    {  4000000, 0,   384000,  1536000, 0, 100000000, 0,   384000,  3000000, 0, 1.720},
    { 20000000, 0,   384000,  1536000, 0,  20000000, 0,   384000,  3000000, 0, 0.625},
    { 20000000, 0,   384000,  1536000, 0, 100000000, 0,   384000,  3000000, 0, 2.025},
    {100000000, 0,   384000,  1536000, 0, 100000000, 0,   384000,  3000000, 0, 1.040},
    {  4000000, 0,   384000,  3000000, 0,   4000000, 0,   384000,  3000000, 0, 0.040},
    {  4000000, 0,   384000,  3000000, 0,  20000000, 0,   384000,  3000000, 0, 0.200},
    {  4000000, 0,   384000,  3000000, 0, 100000000, 0,   384000,  3000000, 0, 0.520},
    { 20000000, 0,   384000,  3000000, 0,  20000000, 0,   384000,  3000000, 0, 0.250},
    { 20000000, 0,   384000,  3000000, 0, 100000000, 0,   384000,  3000000, 0, 1.300},
    {100000000, 0,   384000,  3000000, 0, 100000000, 0,   384000,  3000000, 0, 1.690},
    {  4000000, 0,   128000,  1536000, 0,  20000000, 0,   768000,  1536000, 0, 0.090},
    {  4000000, 0,   128000,  1536000, 0, 100000000, 0,   768000,  1536000, 0, 0.360},
    { 20000000, 0,   128000,  1536000, 0,  20000000, 0,   768000,  1536000, 0, 0.090},
    { 20000000, 0,   128000,  1536000, 0, 100000000, 0,   768000,  1536000, 0, 0.405},
    {100000000, 0,   128000,  1536000, 0, 100000000, 0,   768000,  1536000, 0, 0.180},
    {  4000000, 0,   128000,  7000000, 0,  20000000, 0,   768000,   768000, 0, 0.270},
    {  4000000, 0,   128000,  7000000, 0, 100000000, 0,   768000,   768000, 0, 1.080},
    { 20000000, 0,   128000,  7000000, 0,  20000000, 0,   768000,   768000, 0, 0.270},
    { 20000000, 0,   128000,  7000000, 0, 100000000, 0,   768000,   768000, 0, 1.215},
    {100000000, 0,   128000,  7000000, 0, 100000000, 0,   768000,   768000, 0, 0.540},
    {  4000000, 0,   128000, 13000000, 0,  20000000, 0,   768000, 13000000, 0, 0.030},
    {  4000000, 0,   128000, 13000000, 0, 100000000, 0,   768000, 13000000, 0, 0.120},
    { 20000000, 0,   128000, 13000000, 0,  20000000, 0,   768000, 13000000, 0, 0.030},
    { 20000000, 0,   128000, 13000000, 0, 100000000, 0,   768000, 13000000, 0, 0.135},
    {100000000, 0,   128000, 13000000, 0, 100000000, 0,   768000, 13000000, 0, 0.060},
    {  4000000, 0,   384000,  1536000, 0,  20000000, 0,  1536000,  1536000, 0, 0.180},
    {  4000000, 0,   384000,  1536000, 0, 100000000, 0,  1536000,  1536000, 0, 0.720},
    { 20000000, 0,   384000,  1536000, 0,  20000000, 0,  1536000,  1536000, 0, 0.188},
    { 20000000, 0,   384000,  1536000, 0, 100000000, 0,  1536000,  1536000, 0, 0.870},
    {100000000, 0,   384000,  1536000, 0, 100000000, 0,  1536000,  1536000, 0, 0.480},
    {  4000000, 0,   384000,  7000000, 0,  20000000, 0,   768000,  1536000, 0, 0.540},
    {  4000000, 0,   384000,  7000000, 0, 100000000, 0,   768000,  1536000, 0, 2.160},
    { 20000000, 0,   384000,  7000000, 0,  20000000, 0,   768000,  1536000, 0, 0.563},
    { 20000000, 0,   384000,  7000000, 0, 100000000, 0,   768000,  1536000, 0, 2.610},
    {100000000, 0,   384000,  7000000, 0, 100000000, 0,   768000,  1536000, 0, 1.440},
    {  4000000, 0,   384000, 13000000, 0,  20000000, 0,  1536000, 13000000, 0, 0.060},
    {  4000000, 0,   384000, 13000000, 0, 100000000, 0,  1536000, 13000000, 0, 0.240},
    { 20000000, 0,   384000, 13000000, 0,  20000000, 0,  1536000, 13000000, 0, 0.063},
    { 20000000, 0,   384000, 13000000, 0, 100000000, 0,  1536000, 13000000, 0, 0.290},
    {100000000, 0,   384000, 13000000, 0, 100000000, 0,  1536000, 13000000, 0, 0.160},
    {  4000000, 0,   384000,  1536000, 0,  20000000, 0,  1536000,  3000000, 0, 0.030},
    {  4000000, 0,   384000,  1536000, 0, 100000000, 0,  1536000,  3000000, 0, 0.120},
    { 20000000, 0,   384000,  1536000, 0,  20000000, 0,  1536000,  3000000, 0, 0.075},
    { 20000000, 0,   384000,  1536000, 0, 100000000, 0,  1536000,  3000000, 0, 0.495},
    {100000000, 0,   384000,  1536000, 0, 100000000, 0,  1536000,  3000000, 0, 0.780},
    {  4000000, 0,   384000,  7000000, 0,  20000000, 0,   768000,  3000000, 0, 0.090},
    {  4000000, 0,   384000,  7000000, 0, 100000000, 0,   768000,  3000000, 0, 0.360},
    { 20000000, 0,   384000,  7000000, 0,  20000000, 0,   768000,  3000000, 0, 0.225},
    { 20000000, 0,   384000,  7000000, 0, 100000000, 0,   768000,  3000000, 0, 1.485},
    {100000000, 0,   384000,  7000000, 0, 100000000, 0,   768000,  3000000, 0, 2.340},
    {  4000000, 0,   384000, 13000000, 0,  20000000, 0,  3000000, 13000000, 0, 0.010},
    {  4000000, 0,   384000, 13000000, 0, 100000000, 0,  3000000, 13000000, 0, 0.040},
    { 20000000, 0,   384000, 13000000, 0,  20000000, 0,  3000000, 13000000, 0, 0.025},
    { 20000000, 0,   384000, 13000000, 0, 100000000, 0,  3000000, 13000000, 0, 0.165},
    {100000000, 0,   384000, 13000000, 0, 100000000, 0,  3000000, 13000000, 0, 0.260},
    {  4000000, 0,   768000,  1536000, 0,  20000000, 0,   128000,  1536000, 0, 0.090},
    { 20000000, 0,   768000,  1536000, 0,  20000000, 0,   128000,  1536000, 0, 0.090},
    { 20000000, 0,   768000,  1536000, 0, 100000000, 0,   128000,  1536000, 0, 0.405},
    {  4000000, 0,   768000,  1536000, 0, 100000000, 0,   128000,  1536000, 0, 0.360},
    {100000000, 0,   768000,  1536000, 0, 100000000, 0,   128000,  1536000, 0, 0.180},
    {  4000000, 0,  1536000,  1536000, 0,  20000000, 0,   384000,  1536000, 0, 0.180},
    { 20000000, 0,  1536000,  1536000, 0,  20000000, 0,   384000,  1536000, 0, 0.188},
    { 20000000, 0,  1536000,  1536000, 0, 100000000, 0,   384000,  1536000, 0, 0.870},
    {  4000000, 0,  1536000,  1536000, 0, 100000000, 0,   384000,  1536000, 0, 0.720},
    {100000000, 0,  1536000,  1536000, 0, 100000000, 0,   384000,  1536000, 0, 0.480},
    {  4000000, 0,  1536000,  3000000, 0,  20000000, 0,   384000,  1536000, 0, 0.030},
    { 20000000, 0,  1536000,  3000000, 0,  20000000, 0,   384000,  1536000, 0, 0.075},
    { 20000000, 0,  1536000,  3000000, 0, 100000000, 0,   384000,  1536000, 0, 0.495},
    {  4000000, 0,  1536000,  3000000, 0, 100000000, 0,   384000,  1536000, 0, 0.120},
    {100000000, 0,  1536000,  3000000, 0, 100000000, 0,   384000,  1536000, 0, 0.780},
    {  4000000, 0,   768000,   768000, 0,  20000000, 0,   128000,  7000000, 0, 0.270},
    { 20000000, 0,   768000,   768000, 0,  20000000, 0,   128000,  7000000, 0, 0.270},
    { 20000000, 0,   768000,   768000, 0, 100000000, 0,   128000,  7000000, 0, 1.215},
    {  4000000, 0,   768000,   768000, 0, 100000000, 0,   128000,  7000000, 0, 1.080},
    {100000000, 0,   768000,   768000, 0, 100000000, 0,   128000,  7000000, 0, 0.540},
    {  4000000, 0,   768000,  1536000, 0,  20000000, 0,   384000,  7000000, 0, 0.540},
    { 20000000, 0,   768000,  1536000, 0,  20000000, 0,   384000,  7000000, 0, 0.563},
    { 20000000, 0,   768000,  1536000, 0, 100000000, 0,   384000,  7000000, 0, 2.610},
    {  4000000, 0,   768000,  1536000, 0, 100000000, 0,   384000,  7000000, 0, 2.160},
    {100000000, 0,   768000,  1536000, 0, 100000000, 0,   384000,  7000000, 0, 1.440},
    {  4000000, 0,   768000,  3000000, 0,  20000000, 0,   384000,  7000000, 0, 0.090},
    { 20000000, 0,   768000,  3000000, 0,  20000000, 0,   384000,  7000000, 0, 0.225},
    { 20000000, 0,   768000,  3000000, 0, 100000000, 0,   384000,  7000000, 0, 1.485},
    {  4000000, 0,   768000,  3000000, 0, 100000000, 0,   384000,  7000000, 0, 0.360},
    {100000000, 0,   768000,  3000000, 0, 100000000, 0,   384000,  7000000, 0, 2.340},
    {  4000000, 0,   768000, 13000000, 0,  20000000, 0,   128000, 13000000, 0, 0.030},
    { 20000000, 0,   768000, 13000000, 0,  20000000, 0,   128000, 13000000, 0, 0.030},
    { 20000000, 0,   768000, 13000000, 0, 100000000, 0,   128000, 13000000, 0, 0.135},
    {  4000000, 0,   768000, 13000000, 0, 100000000, 0,   128000, 13000000, 0, 0.120},
    {100000000, 0,   768000, 13000000, 0, 100000000, 0,   128000, 13000000, 0, 0.060},
    {  4000000, 0,  1536000, 13000000, 0,  20000000, 0,   384000, 13000000, 0, 0.060},
    { 20000000, 0,  1536000, 13000000, 0,  20000000, 0,   384000, 13000000, 0, 0.063},
    { 20000000, 0,  1536000, 13000000, 0, 100000000, 0,   384000, 13000000, 0, 0.290},
    {  4000000, 0,  1536000, 13000000, 0, 100000000, 0,   384000, 13000000, 0, 0.240},
    {100000000, 0,  1536000, 13000000, 0, 100000000, 0,   384000, 13000000, 0, 0.160},
    {  4000000, 0,  3000000, 13000000, 0,  20000000, 0,   384000, 13000000, 0, 0.010},
    { 20000000, 0,  3000000, 13000000, 0,  20000000, 0,   384000, 13000000, 0, 0.025},
    { 20000000, 0,  3000000, 13000000, 0, 100000000, 0,   384000, 13000000, 0, 0.165},
    {  4000000, 0,  3000000, 13000000, 0, 100000000, 0,   384000, 13000000, 0, 0.040},
    {100000000, 0,  3000000, 13000000, 0, 100000000, 0,   384000, 13000000, 0, 0.260},
    { 20000000, 0,  1536000,  1536000, 0,  20000000, 0,  1536000,  1536000, 0, 0.023},
    { 20000000, 0,  1536000,  1536000, 0, 100000000, 0,  1536000,  1536000, 0, 0.180},
    {100000000, 0,  1536000,  1536000, 0, 100000000, 0,  1536000,  1536000, 0, 0.360},
    { 20000000, 0,  1536000,  7000000, 0,  20000000, 0,   768000,  1536000, 0, 0.068},
    { 20000000, 0,  1536000,  7000000, 0, 100000000, 0,   768000,  1536000, 0, 0.540},
    {100000000, 0,  1536000,  7000000, 0, 100000000, 0,   768000,  1536000, 0, 1.080},
    { 20000000, 0,  1536000, 13000000, 0,  20000000, 0,  1536000, 13000000, 0, 0.015},
    { 20000000, 0,  1536000, 13000000, 0, 100000000, 0,  1536000, 13000000, 0, 0.120},
    {100000000, 0,  1536000, 13000000, 0, 100000000, 0,  1536000, 13000000, 0, 0.240},
    { 20000000, 0,   768000,  1536000, 0,  20000000, 0,  1536000,  7000000, 0, 0.068},
    { 20000000, 0,   768000,  1536000, 0, 100000000, 0,  1536000,  7000000, 0, 0.540},
    {100000000, 0,   768000,  1536000, 0, 100000000, 0,  1536000,  7000000, 0, 1.080},
    { 20000000, 0,   768000,  7000000, 0,  20000000, 0,   768000,  7000000, 0, 0.203},
    { 20000000, 0,   768000,  7000000, 0, 100000000, 0,   768000,  7000000, 0, 1.620},
    {100000000, 0,   768000,  7000000, 0, 100000000, 0,   768000,  7000000, 0, 3.240},
    { 20000000, 0,   768000, 13000000, 0,  20000000, 0,  7000000, 13000000, 0, 0.023},
    { 20000000, 0,   768000, 13000000, 0, 100000000, 0,  7000000, 13000000, 0, 0.180},
    {100000000, 0,   768000, 13000000, 0, 100000000, 0,  7000000, 13000000, 0, 0.360},
    { 20000000, 0,  7000000, 13000000, 0,  20000000, 0,   768000, 13000000, 0, 0.023},
    { 20000000, 0,  7000000, 13000000, 0, 100000000, 0,   768000, 13000000, 0, 0.180},
    {100000000, 0,  7000000, 13000000, 0, 100000000, 0,   768000, 13000000, 0, 0.360},
    { 20000000, 0, 13000000, 13000000, 0,  20000000, 0, 13000000, 13000000, 0, 0.003},
    { 20000000, 0, 13000000, 13000000, 0, 100000000, 0, 13000000, 13000000, 0, 0.020},
    {100000000, 0, 13000000, 13000000, 0, 100000000, 0, 13000000, 13000000, 0, 0.040}
};

g1050_model_t g1050_standard_models[9] =
{
    {   /* Severity 0 - no impairment */
        {
            0,          /*! Percentage likelihood of occurance in scenario A */
            0,          /*! Percentage likelihood of occurance in scenario B */
            0,          /*! Percentage likelihood of occurance in scenario C */
        },
        {
            0.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            0.0,        /*! Percentage occupancy */
            512,        /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            0.0,        /*! Basic delay of the regional backbone, in seconds */
            0.0,        /*! Basic delay of the intercontinental backbone, in seconds */
            0.0,        /*! Percentage packet loss of the backbone */
            0.0,        /*! Maximum jitter of the backbone, in seconds */
            0.0,        /*! Interval between the backbone route flapping between two paths, in seconds */
            0.0,        /*! The difference in backbone delay between the two routes we flap between, in seconds */
            0.0,        /*! The interval between link failures, in seconds */
            0.0,        /*! The duration of link failures, in seconds */
            0.0,        /*! Probability of packet loss in the backbone, in percent */
            0.0         /*! Probability of a packet going out of sequence in the backbone. */
        },
        {
            0.0,        /*! Percentage occupancy */
            512,        /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            0.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0         /*! Peak jitter */
        }
    },
    {   /* Severity A */
        {
            50,         /*! Percentage likelihood of occurance in scenario A */
            5,          /*! Percentage likelihood of occurance in scenario B */
            5,          /*! Percentage likelihood of occurance in scenario C */
        },
        {
            1.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        },
        {
            0.0,        /*! Percentage occupancy */
            512,        /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            0.004,      /*! Basic delay of the regional backbone, in seconds */
            0.016,      /*! Basic delay of the intercontinental backbone, in seconds */
            0.0,        /*! Percentage packet loss of the backbone */
            0.005,      /*! Maximum jitter of the backbone, in seconds */
            0.0,        /*! Interval between the backbone route flapping between two paths, in seconds */
            0.0,        /*! The difference in backbone delay between the two routes we flap between, in seconds */
            0.0,        /*! The interval between link failures, in seconds */
            0.0,        /*! The duration of link failures, in seconds */
            0.0,        /*! Probability of packet loss in the backbone, in percent */
            0.0         /*! Probability of a packet going out of sequence in the backbone. */
        },
        {
            0.0,        /*! Percentage occupancy */
            512,        /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            1.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        }
    },
    {   /* Severity B */
        {
            30,         /*! Percentage likelihood of occurance in scenario A */
            25,         /*! Percentage likelihood of occurance in scenario B */
            5,          /*! Percentage likelihood of occurance in scenario C */
        },
        {
            2.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        },
        {
            1.0,        /*! Percentage occupancy */
            512,        /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            0.008,      /*! Basic delay of the regional backbone, in seconds */
            0.032,      /*! Basic delay of the intercontinental backbone, in seconds */
            0.01,       /*! Percentage packet loss of the backbone */
            0.01,       /*! Maximum jitter of the backbone, in seconds */
            3600.0,     /*! Interval between the backbone route flapping between two paths, in seconds */
            0.002,      /*! The difference in backbone delay between the two routes we flap between, in seconds */
            3600.0,     /*! The interval between link failures, in seconds */
            0.064,      /*! The duration of link failures, in seconds */
            0.0,        /*! Probability of packet loss in the backbone, in percent */
            0.0         /*! Probability of a packet going out of sequence in the backbone. */
        },
        {
            1.0,        /*! Percentage occupancy */
            512,        /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            2.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        }
    },
    {   /* Severity C */
        {
            15,         /*! Percentage likelihood of occurance in scenario A */
            30,         /*! Percentage likelihood of occurance in scenario B */
            10,         /*! Percentage likelihood of occurance in scenario C */
        },
        {
            3.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        },
        {
            2.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            0.016,      /*! Basic delay of the regional backbone, in seconds */
            0.064,      /*! Basic delay of the intercontinental backbone, in seconds */
            0.02,       /*! Percentage packet loss of the backbone */
            0.016,      /*! Maximum jitter of the backbone, in seconds */
            1800.0,     /*! Interval between the backbone route flapping between two paths, in seconds */
            0.004,      /*! The difference in backbone delay between the two routes we flap between, in seconds */
            1800.0,     /*! The interval between link failures, in seconds */
            0.128,      /*! The duration of link failures, in seconds */
            0.0,        /*! Probability of packet loss in the backbone, in percent */
            0.0         /*! Probability of a packet going out of sequence in the backbone. */
        },
        {
            2.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            3.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        }
    },
    {   /* Severity D */
        {
            5,          /*! Percentage likelihood of occurance in scenario A */
            25,         /*! Percentage likelihood of occurance in scenario B */
            15,         /*! Percentage likelihood of occurance in scenario C */
        },
        {
            5.0,         /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        },
        {
            4.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            0.032,      /*! Basic delay of the regional backbone, in seconds */
            0.128,      /*! Basic delay of the intercontinental backbone, in seconds */
            0.04,       /*! Percentage packet loss of the backbone */
            0.04,       /*! Maximum jitter of the backbone, in seconds */
            900.0,      /*! Interval between the backbone route flapping between two paths, in seconds */
            0.008,      /*! The difference in backbone delay between the two routes we flap between, in seconds */
            900.0,      /*! The interval between link failures, in seconds */
            0.256,      /*! The duration of link failures, in seconds */
            0.0,        /*! Probability of packet loss in the backbone, in percent */
            0.0         /*! Probability of a packet going out of sequence in the backbone. */
        },
        {
            4.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            5.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        }
    },
    {   /* Severity E */
        {
            0,          /*! Percentage likelihood of occurance in scenario A */
            10,         /*! Percentage likelihood of occurance in scenario B */
            20,         /*! Percentage likelihood of occurance in scenario C */
        },
        {
            8.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        },
        {
            8.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            0.064,      /*! Basic delay of the regional backbone, in seconds */
            0.196,      /*! Basic delay of the intercontinental backbone, in seconds */
            0.1,        /*! Percentage packet loss of the backbone */
            0.07,       /*! Maximum jitter of the backbone, in seconds */
            480.0,      /*! Interval between the backbone route flapping between two paths, in seconds */
            0.016,      /*! The difference in backbone delay between the two routes we flap between, in seconds */
            480.0,      /*! The interval between link failures, in seconds */
            0.4,        /*! The duration of link failures, in seconds */
            0.0,        /*! Probability of packet loss in the backbone, in percent */
            0.0         /*! Probability of a packet going out of sequence in the backbone. */
        },
        {
            8.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            8.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        }
    },
    {   /* Severity F */
        {
            0,          /*! Percentage likelihood of occurance in scenario A */
            0,          /*! Percentage likelihood of occurance in scenario B */
            25,         /*! Percentage likelihood of occurance in scenario C */
        },
        {
            12.0,       /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        },
        {
            15.0,       /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            0.128,      /*! Basic delay of the regional backbone, in seconds */
            0.256,      /*! Basic delay of the intercontinental backbone, in seconds */
            0.2,        /*! Percentage packet loss of the backbone */
            0.1,        /*! Maximum jitter of the backbone, in seconds */
            240.0,      /*! Interval between the backbone route flapping between two paths, in seconds */
            0.032,      /*! The difference in backbone delay between the two routes we flap between, in seconds */
            240.0,      /*! The interval between link failures, in seconds */
            0.8,        /*! The duration of link failures, in seconds */
            0.0,        /*! Probability of packet loss in the backbone, in percent */
            0.0         /*! Probability of a packet going out of sequence in the backbone. */
        },
        {
            15.0,       /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            12.0,       /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        }
    },
    {   /* Severity G */
        {
            0,          /*! Percentage likelihood of occurance in scenario A */
            0,          /*! Percentage likelihood of occurance in scenario B */
            15,         /*! Percentage likelihood of occurance in scenario C */
        },
        {
            16.0,       /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        },
        {
            30.0,       /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            0.256,      /*! Basic delay of the regional backbone, in seconds */
            0.512,      /*! Basic delay of the intercontinental backbone, in seconds */
            0.5,        /*! Percentage packet loss of the backbone */
            0.15,       /*! Maximum jitter of the backbone, in seconds */
            120.0,      /*! Interval between the backbone route flapping between two paths, in seconds */
            0.064,      /*! The difference in backbone delay between the two routes we flap between, in seconds */
            120.0,      /*! The interval between link failures, in seconds */
            1.6,        /*! The duration of link failures, in seconds */
            0.0,        /*! Probability of packet loss in the backbone, in percent */
            0.0         /*! Probability of a packet going out of sequence in the backbone. */
        },
        {
            30.0,       /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            16.0,       /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        }
    },
    {   /* Severity H */
        {
            0,          /*! Percentage likelihood of occurance in scenario A */
            0,          /*! Percentage likelihood of occurance in scenario B */
            5,          /*! Percentage likelihood of occurance in scenario C */
        },
        {
            20.0,       /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        },
        {
            50.0,       /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            0.512,      /*! Basic delay of the regional backbone, in seconds */
            0.768,      /*! Basic delay of the intercontinental backbone, in seconds */
            1.0,        /*! Percentage packet loss of the backbone */
            0.5,        /*! Maximum jitter of the backbone, in seconds */
            60.0,       /*! Interval between the backbone route flapping between two paths, in seconds */
            0.128,      /*! The difference in backbone delay between the two routes we flap between, in seconds */
            60.0,       /*! The interval between link failures, in seconds */
            3.0,        /*! The duration of link failures, in seconds */
            1.0,        /*! Probability of packet loss in the backbone, in percent */
            1.0         /*! Probability of a packet going out of sequence in the backbone. */
        },
        {
            50.0,       /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            20.0,       /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        }
    }
};

#if defined(HAVE_DRAND48)
static __inline__ void q1050_rand_init(void)
{
    srand48(time(NULL));
}
/*- End of function --------------------------------------------------------*/

static __inline__ double q1050_rand(void)
{
    return drand48();
}
/*- End of function --------------------------------------------------------*/
#else
static __inline__ void q1050_rand_init(void)
{
    srand(time(NULL));
}
/*- End of function --------------------------------------------------------*/

static __inline__ double q1050_rand(void)
{
    return (double) rand()/(double) RAND_MAX;
}
/*- End of function --------------------------------------------------------*/
#endif

static __inline__ double scale_probability(double prob, double scale)
{
    /* Re-calculate probability based on a different time interval */
    return 1.0 - pow(1.0 - prob, scale);
}
/*- End of function --------------------------------------------------------*/

static void g1050_segment_init(g1050_segment_state_t *s,
                               int link_type,
                               g1050_segment_constants_t *constants,
                               g1050_segment_model_t *parms,
                               int bit_rate,
                               int multiple_access,
                               int qos_enabled,
                               int packet_size,
                               int packet_rate)
{
    double x;
    double packet_interval;

    memset(s, 0, sizeof(*s));

    packet_interval = 1000.0/packet_rate;
    /* Some calculatons are common to both LAN and access links, and those that are not. */
    s->link_type = link_type;
    s->prob_loss_rate_change[0] = scale_probability(constants->prob_loss_rate_change[0]*parms->percentage_occupancy, 1.0/packet_interval);

    s->serial_delay = packet_size*8.0/bit_rate;
    if (link_type == G1050_LAN_LINK)
    {
        s->prob_loss_rate_change[1] = scale_probability(constants->prob_loss_rate_change[1], 1.0/packet_interval);
        s->prob_impulse[0] = constants->prob_impulse[0][0];
        s->prob_impulse[1] = constants->prob_impulse[1][0];
        s->impulse_decay_coeff = constants->impulse_decay_coeff;
        s->impulse_height = parms->mtu*(8.0/bit_rate)*(1.0 + parms->percentage_occupancy/constants->impulse_height);
    }
    else if (link_type == G1050_ACCESS_LINK)
    {
        s->prob_loss_rate_change[1] = scale_probability(constants->prob_loss_rate_change[1]/(1.0 + parms->percentage_occupancy), 1.0/packet_interval);
        s->prob_impulse[0] = scale_probability(constants->prob_impulse[0][0] + (parms->percentage_occupancy/2000.0), 1.0/packet_interval);
        s->prob_impulse[1] = scale_probability(constants->prob_impulse[1][0] + (constants->prob_impulse[1][1]*parms->percentage_occupancy/100.0), 1.0/packet_interval);
        s->impulse_decay_coeff = 1.0 - scale_probability(1.0 - constants->impulse_decay_coeff, 1.0/packet_interval);
        x = (1.0 - constants->impulse_decay_coeff)/(1.0 - s->impulse_decay_coeff);
        s->impulse_height = x*parms->mtu*(8.0/bit_rate)*(1.0 + parms->percentage_occupancy/constants->impulse_height);
    }
    /*endif*/

    /* The following are calculated the same way for LAN and access links */
    s->prob_packet_loss = constants->prob_packet_loss*parms->percentage_occupancy;
    s->qos_enabled = qos_enabled;
    s->multiple_access = multiple_access;
    s->prob_packet_collision_loss = constants->prob_packet_collision_loss;
    s->max_jitter = parms->max_jitter;

    /* The following is common state information to all links. */
    s->high_loss = false;
    s->congestion_delay = 0.0;
    s->last_arrival_time = 0.0;

    /* Count of packets lost in this segment. */
    s->lost_packets = 0;
    s->lost_packets_2 = 0;
}
/*- End of function --------------------------------------------------------*/

static void g1050_core_init(g1050_core_state_t *s, g1050_core_model_t *parms, int packet_rate)
{
    memset(s, 0, sizeof(*s));

    /* Set up route flapping. */
    /* This is the length of the period of both the delayed duration and the non-delayed. */
    s->route_flap_interval = parms->route_flap_interval*G1050_TICKS_PER_SEC;

    /* How much additional delay is added or subtracted during route flaps. */
    s->route_flap_delta = parms->route_flap_delay;

    /* Current tick count. This is initialized so that we are part way into the first
       CLEAN interval before the first change occurs. This is a random portion of the
       period. When we reach the first flap, the flapping in both directions becomes
       periodic. */
    s->route_flap_counter = s->route_flap_interval - 99 - floor(s->route_flap_interval*q1050_rand());
    s->link_failure_interval_ticks = parms->link_failure_interval*G1050_TICKS_PER_SEC;

    /* Link failures occur when the count reaches this number of ticks. */
    /* Duration of a failure. */
    s->link_failure_duration_ticks = floor((G1050_TICKS_PER_SEC*parms->link_failure_duration));
    /* How far into the first CLEAN interval we are. This is like the route flap initialzation. */
    s->link_failure_counter = s->link_failure_interval_ticks - 99 - floor(s->link_failure_interval_ticks*q1050_rand());
    s->link_recovery_counter = s->link_failure_duration_ticks;

    s->base_delay = parms->base_regional_delay;
    s->max_jitter = parms->max_jitter;
    s->prob_packet_loss = parms->prob_packet_loss/100.0;
    s->prob_oos = parms->prob_oos/100.0;
    s->last_arrival_time = 0.0;
    s->delay_delta = 0;

    /* Count of packets lost in this segment. */
    s->lost_packets = 0;
    s->lost_packets_2 = 0;
}
/*- End of function --------------------------------------------------------*/

static void g1050_segment_model(g1050_segment_state_t *s, double delays[], int len)
{
    int i;
    bool lose;
    int was_high_loss;
    double impulse;
    double slice_delay;

    /* Compute delay and loss value for each time slice. */
    for (i = 0;  i < len;  i++)
    {
        lose = false;
        /* Initialize delay to the serial delay plus some jitter. */
        slice_delay = s->serial_delay + s->max_jitter*q1050_rand();
        /* If no QoS, do congestion delay and packet loss analysis. */
        if (!s->qos_enabled)
        {
            /* To match the logic in G.1050 we need to record the current loss state, before
               checking if we should change. */
            was_high_loss = s->high_loss;
            /* Toggle between the low-loss and high-loss states, based on the transition probability. */
            if (q1050_rand() < s->prob_loss_rate_change[was_high_loss])
                s->high_loss = !s->high_loss;
            /*endif*/
            impulse = 0.0;
            if (q1050_rand() < s->prob_impulse[was_high_loss])
            {
                impulse = s->impulse_height;
                if (!was_high_loss  ||  s->link_type == G1050_LAN_LINK)
                    impulse *= q1050_rand();
                /*endif*/
            }
            /*endif*/

            if (was_high_loss  &&  q1050_rand() < s->prob_packet_loss)
                lose = true;
            /*endif*/
            /* Single pole LPF for the congestion delay impulses. */
            s->congestion_delay = s->congestion_delay*s->impulse_decay_coeff + impulse*(1.0 - s->impulse_decay_coeff);
            slice_delay += s->congestion_delay;
        }
        /*endif*/
        /* If duplex mismatch on LAN, packet loss based on loss probability. */
        if (s->multiple_access  &&  (q1050_rand() < s->prob_packet_collision_loss))
            lose = true;
        /*endif*/
        /* Put computed delay into time slice array. */
        if (lose)
        {
            delays[i] = PACKET_LOSS_TIME;
            s->lost_packets++;
        }
        else
        {
            delays[i] = slice_delay;
        }
        /*endif*/
    }
    /*endfor*/
}
/*- End of function --------------------------------------------------------*/

static void g1050_core_model(g1050_core_state_t *s, double delays[], int len)
{
    int32_t i;
    bool lose;
    double jitter_delay;

    for (i = 0;  i < len;  i++)
    {
        lose = false;
        jitter_delay = s->base_delay + s->max_jitter*q1050_rand();
        /* Route flapping */
        if (--s->route_flap_counter <= 0)
        {
            /* Route changed */
            s->delay_delta = s->route_flap_delta - s->delay_delta;
            s->route_flap_counter = s->route_flap_interval;
        }
        /*endif*/
        if (q1050_rand() < s->prob_packet_loss)
            lose = true;
        /*endif*/
        /* Link failures */
        if (--s->link_failure_counter <= 0)
        {
            /* We are in a link failure */
            lose = true;
            if (--s->link_recovery_counter <= 0)
            {
                /* Leave failure state. */
                s->link_failure_counter = s->link_failure_interval_ticks;
                s->link_recovery_counter = s->link_failure_duration_ticks;
                lose = false;
            }
            /*endif*/
        }
        /*endif*/
        if (lose)
        {
            delays[i] = PACKET_LOSS_TIME;
            s->lost_packets++;
        }
        else
        {
            delays[i] = jitter_delay + s->delay_delta;
        }
        /*endif*/
    }
    /*endfor*/
}
/*- End of function --------------------------------------------------------*/

static int g1050_segment_delay(g1050_segment_state_t *s,
                               double base_time,
                               double arrival_times[],
                               double delays[],
                               int num_packets)
{
    int i;
    int32_t departure_time;
    int lost_packets;

    /* Add appropriate delays to the packets for the segments before the core. */
    lost_packets = 0;
    for (i = 0;  i < num_packets;  i++)
    {
        /* Apply half a millisecond of rounding, as we working in millisecond steps. */
        departure_time = (arrival_times[i] + 0.0005 - base_time)*G1050_TICKS_PER_SEC;
        if (arrival_times[i] == PACKET_LOSS_TIME)
        {
            /* Lost already */
        }
        else if (delays[departure_time] == PACKET_LOSS_TIME)
        {
            arrival_times[i] = PACKET_LOSS_TIME;
            lost_packets++;
        }
        else
        {
            arrival_times[i] += delays[departure_time];
            if (arrival_times[i] < s->last_arrival_time)
                arrival_times[i] = s->last_arrival_time;
            else
                s->last_arrival_time = arrival_times[i];
            /*endif*/
        }
        /*endif*/
    }
    /*endfor*/
    return lost_packets;
}
/*- End of function --------------------------------------------------------*/

static int g1050_segment_delay_preserve_order(g1050_segment_state_t *s,
                                              double base_time,
                                              double arrival_times_a[],
                                              double arrival_times_b[],
                                              double delays[],
                                              int num_packets)
{
    int i;
    int j;
    int departure_time;
    double last_arrival_time;
    double last_arrival_time_temp;
    int lost_packets;

    /* Add appropriate delays to the packets for the segments after the core. */
    last_arrival_time = 0.0;
    last_arrival_time_temp = 0.0;
    lost_packets = 0;
    for (i = 0;  i < num_packets;  i++)
    {
        /* We need to preserve the order that came out of the core, so we
           use an alternate array for the results.  */
        /* Apply half a millisecond of rounding, as we working in millisecond steps. */
        departure_time = (arrival_times_a[i] + 0.0005 - base_time)*G1050_TICKS_PER_SEC;
        if (arrival_times_a[i] == PACKET_LOSS_TIME)
        {
            /* Lost already */
            arrival_times_b[i] = PACKET_LOSS_TIME;
        }
        else if (delays[departure_time] == PACKET_LOSS_TIME)
        {
            arrival_times_b[i] = PACKET_LOSS_TIME;
            lost_packets++;
        }
        else
        {
            arrival_times_b[i] = arrival_times_a[i] + delays[departure_time];
            if (arrival_times_a[i] < last_arrival_time)
            {
                /* If a legitimate out of sequence packet is detected, search
                   back a fixed amount of time to preserve order. */
                for (j = i - 1;  j >= 0;  j--)
                {
                    if ((arrival_times_a[j] != PACKET_LOSS_TIME)
                        &&
                        (arrival_times_b[j] != PACKET_LOSS_TIME))
                    {
                        if ((arrival_times_a[i] - arrival_times_a[j]) > SEARCHBACK_PERIOD)
                            break;
                        /*endif*/
                        if ((arrival_times_a[j] > arrival_times_a[i])
                            &&
                            (arrival_times_b[j] < arrival_times_b[i]))
                        {
                            arrival_times_b[j] = arrival_times_b[i];
                        }
                        /*endif*/
                    }
                    /*endif*/
                }
                /*endfor*/
            }
            else
            {
                last_arrival_time = arrival_times_a[i];
                if (arrival_times_b[i] < last_arrival_time_temp)
                    arrival_times_b[i] = last_arrival_time_temp;
                else
                    last_arrival_time_temp = arrival_times_b[i];
                /*endif*/
            }
            /*endif*/
        }
        /*endif*/
    }
    /*endfor*/
    return lost_packets;
}
/*- End of function --------------------------------------------------------*/

static int g1050_core_delay(g1050_core_state_t *s,
                            double base_time,
                            double arrival_times[],
                            double delays[],
                            int num_packets)
{
    int i;
    int departure_time;
    int lost_packets;

    /* This element does NOT preserve packet order. */
    lost_packets = 0;
    for (i = 0;  i < num_packets;  i++)
    {
        /* Apply half a millisecond of rounding, as we working in millisecond steps. */
        departure_time = (arrival_times[i] + 0.0005 - base_time)*G1050_TICKS_PER_SEC;
        if (arrival_times[i] == PACKET_LOSS_TIME)
        {
            /* Lost already */
        }
        else if (delays[departure_time] == PACKET_LOSS_TIME)
        {
            arrival_times[i] = PACKET_LOSS_TIME;
            lost_packets++;
        }
        else
        {
            /* Not lost. Compute arrival time. */
            arrival_times[i] += delays[departure_time];
            if (arrival_times[i] < s->last_arrival_time)
            {
                /* This packet is EARLIER than the last one. It is out of order! */
                /* Do we allow it to stay out of order? */
                if (q1050_rand() >= s->prob_oos)
                    arrival_times[i] = s->last_arrival_time;
                /*endif*/
            }
            else
            {
                /* Packet is in the correct order, relative to the last one. */
                s->last_arrival_time = arrival_times[i];
            }
            /*endif*/
        }
        /*endif*/
    }
    /*endfor*/
    return lost_packets;
}
/*- End of function --------------------------------------------------------*/

static void g1050_simulate_chunk(g1050_state_t *s)
{
    int i;

    s->base_time += 1.0;

    memmove(&s->segment[0].delays[0], &s->segment[0].delays[G1050_TICKS_PER_SEC], 2*G1050_TICKS_PER_SEC*sizeof(s->segment[0].delays[0]));
    g1050_segment_model(&s->segment[0], &s->segment[0].delays[2*G1050_TICKS_PER_SEC], G1050_TICKS_PER_SEC);

    memmove(&s->segment[1].delays[0], &s->segment[1].delays[G1050_TICKS_PER_SEC], 2*G1050_TICKS_PER_SEC*sizeof(s->segment[1].delays[0]));
    g1050_segment_model(&s->segment[1], &s->segment[1].delays[2*G1050_TICKS_PER_SEC], G1050_TICKS_PER_SEC);

    memmove(&s->core.delays[0], &s->core.delays[G1050_TICKS_PER_SEC], 2*G1050_TICKS_PER_SEC*sizeof(s->core.delays[0]));
    g1050_core_model(&s->core, &s->core.delays[2*G1050_TICKS_PER_SEC], G1050_TICKS_PER_SEC);

    memmove(&s->segment[2].delays[0], &s->segment[2].delays[G1050_TICKS_PER_SEC], 2*G1050_TICKS_PER_SEC*sizeof(s->segment[2].delays[0]));
    g1050_segment_model(&s->segment[2], &s->segment[2].delays[2*G1050_TICKS_PER_SEC], G1050_TICKS_PER_SEC);

    memmove(&s->segment[3].delays[0], &s->segment[3].delays[G1050_TICKS_PER_SEC], 2*G1050_TICKS_PER_SEC*sizeof(s->segment[3].delays[0]));
    g1050_segment_model(&s->segment[3], &s->segment[3].delays[2*G1050_TICKS_PER_SEC], G1050_TICKS_PER_SEC);

    memmove(&s->arrival_times_1[0], &s->arrival_times_1[s->packet_rate], 2*s->packet_rate*sizeof(s->arrival_times_1[0]));
    memmove(&s->arrival_times_2[0], &s->arrival_times_2[s->packet_rate], 2*s->packet_rate*sizeof(s->arrival_times_2[0]));
    for (i = 0;  i < s->packet_rate;  i++)
    {
        s->arrival_times_1[2*s->packet_rate + i] = s->base_time + 2.0 + (double) i/(double) s->packet_rate;
        s->arrival_times_2[2*s->packet_rate + i] = 0.0;
    }
    /*endfor*/

    s->segment[0].lost_packets_2 += g1050_segment_delay(&s->segment[0], s->base_time, s->arrival_times_1, s->segment[0].delays, s->packet_rate);
    s->segment[1].lost_packets_2 += g1050_segment_delay(&s->segment[1], s->base_time, s->arrival_times_1, s->segment[1].delays, s->packet_rate);
    s->core.lost_packets_2 += g1050_core_delay(&s->core, s->base_time, s->arrival_times_1, s->core.delays, s->packet_rate);
    s->segment[2].lost_packets_2 += g1050_segment_delay_preserve_order(&s->segment[2], s->base_time, s->arrival_times_1, s->arrival_times_2, s->segment[2].delays, s->packet_rate);
    s->segment[3].lost_packets_2 += g1050_segment_delay_preserve_order(&s->segment[3], s->base_time, s->arrival_times_2, s->arrival_times_1, s->segment[3].delays, s->packet_rate);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(g1050_state_t *) g1050_init(int model,
                                         int speed_pattern,
                                         int packet_size,
                                         int packet_rate)
{
    g1050_state_t *s;
    g1050_constants_t *constants;
    g1050_channel_speeds_t *sp;
    g1050_model_t *mo;
    int i;

    /* If the random generator has not been seeded it might give endless
       zeroes - it depends on the platform. */
    for (i = 0;  i < 10;  i++)
    {
        if (q1050_rand() != 0.0)
            break;
        /*endif*/
    }
    /*endfor*/
    if (i >= 10)
        q1050_rand_init();
    /*endif*/
    if ((s = (g1050_state_t *) malloc(sizeof(*s))) == NULL)
        return NULL;
    /*endif*/
    memset(s, 0, sizeof(*s));

    constants = &g1050_constants[0];
    sp = &g1050_speed_patterns[speed_pattern - 1];
    mo = &g1050_standard_models[model];

    memset(s, 0, sizeof(*s));

    s->packet_rate = packet_rate;
    s->packet_size = packet_size;

    g1050_segment_init(&s->segment[0],
                       G1050_LAN_LINK,
                       &constants->segment[0],
                       &mo->sidea_lan,
                       sp->sidea_lan_bit_rate,
                       sp->sidea_lan_multiple_access,
                       false,
                       packet_size,
                       packet_rate);
    g1050_segment_init(&s->segment[1],
                       G1050_ACCESS_LINK,
                       &constants->segment[1],
                       &mo->sidea_access_link,
                       sp->sidea_access_link_bit_rate_ab,
                       false,
                       sp->sidea_access_link_qos_enabled,
                       packet_size,
                       packet_rate);
    g1050_core_init(&s->core, &mo->core, packet_rate);
    g1050_segment_init(&s->segment[2],
                       G1050_ACCESS_LINK,
                       &constants->segment[2],
                       &mo->sideb_access_link,
                       sp->sideb_access_link_bit_rate_ba,
                       false,
                       sp->sideb_access_link_qos_enabled,
                       packet_size,
                       packet_rate);
    g1050_segment_init(&s->segment[3],
                       G1050_LAN_LINK,
                       &constants->segment[3],
                       &mo->sideb_lan,
                       sp->sideb_lan_bit_rate,
                       sp->sideb_lan_multiple_access,
                       false,
                       packet_size,
                       packet_rate);

    s->base_time = 0.0;
    /* Start with enough of the future modelled to allow for the worst jitter.
       After this we will always keep at least 2 seconds of the future modelled. */
    g1050_segment_model(&s->segment[0], s->segment[0].delays, 3*G1050_TICKS_PER_SEC);
    g1050_segment_model(&s->segment[1], s->segment[1].delays, 3*G1050_TICKS_PER_SEC);
    g1050_core_model(&s->core, s->core.delays, 3*G1050_TICKS_PER_SEC);
    g1050_segment_model(&s->segment[2], s->segment[2].delays, 3*G1050_TICKS_PER_SEC);
    g1050_segment_model(&s->segment[3], s->segment[3].delays, 3*G1050_TICKS_PER_SEC);

    /* Initialise the arrival times to the departure times */
    for (i = 0;  i < 3*s->packet_rate;  i++)
    {
        s->arrival_times_1[i] = s->base_time + (double) i/(double)s->packet_rate;
        s->arrival_times_2[i] = 0.0;
    }
    /*endfor*/

    s->segment[0].lost_packets_2 += g1050_segment_delay(&s->segment[0], s->base_time, s->arrival_times_1, s->segment[0].delays, s->packet_rate);
    s->segment[1].lost_packets_2 += g1050_segment_delay(&s->segment[1], s->base_time, s->arrival_times_1, s->segment[1].delays, s->packet_rate);
    s->core.lost_packets_2 += g1050_core_delay(&s->core, s->base_time, s->arrival_times_1, s->core.delays, s->packet_rate);
    s->segment[2].lost_packets_2 += g1050_segment_delay_preserve_order(&s->segment[2], s->base_time, s->arrival_times_1, s->arrival_times_2, s->segment[2].delays, s->packet_rate);
    s->segment[3].lost_packets_2 += g1050_segment_delay_preserve_order(&s->segment[3], s->base_time, s->arrival_times_2, s->arrival_times_1, s->segment[3].delays, s->packet_rate);

    s->first = NULL;
    s->last = NULL;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) g1050_free(g1050_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) g1050_dump_parms(int model, int speed_pattern)
{
    g1050_channel_speeds_t *sp;
    g1050_model_t *mo;

    sp = &g1050_speed_patterns[speed_pattern - 1];
    mo = &g1050_standard_models[model];

    printf("Model %d%c\n", speed_pattern, 'A' + model - 1);
    printf("LOO %.6f%% %.6f%% %.6f%%\n", mo->loo[0]*sp->loo/100.0, mo->loo[1]*sp->loo/100.0, mo->loo[2]*sp->loo/100.0);
    printf("Side A LAN %dbps, %.3f%% occupancy, MTU %d, %s MA\n", sp->sidea_lan_bit_rate, mo->sidea_lan.percentage_occupancy, mo->sidea_lan.mtu, (sp->sidea_lan_multiple_access)  ?  ""  :  "no");
    printf("Side A access %dbps, %.3f%% occupancy, MTU %d, %s QoS\n", sp->sidea_access_link_bit_rate_ab, mo->sidea_access_link.percentage_occupancy, mo->sidea_access_link.mtu, (sp->sidea_access_link_qos_enabled)  ?  ""  :  "no");
    printf("Core delay %.4fs (%.4fs), peak jitter %.4fs, prob loss %.4f%%, prob OOS %.4f%%\n", mo->core.base_regional_delay, mo->core.base_intercontinental_delay, mo->core.max_jitter, mo->core.prob_packet_loss, mo->core.prob_oos);
    printf("     Route flap interval %.4fs, delay change %.4fs\n", mo->core.route_flap_interval, mo->core.route_flap_delay);
    printf("     Link failure interval %.4fs, duration %.4fs\n", mo->core.link_failure_interval, mo->core.link_failure_duration);
    printf("Side B access %dbps, %.3f%% occupancy, MTU %d, %s QoS\n", sp->sideb_access_link_bit_rate_ba, mo->sideb_access_link.percentage_occupancy, mo->sideb_access_link.mtu, (sp->sideb_access_link_qos_enabled)  ?  ""  :  "no");
    printf("Side B LAN %dbps, %.3f%% occupancy, MTU %d, %s MA\n", sp->sideb_lan_bit_rate, mo->sideb_lan.percentage_occupancy, mo->sideb_lan.mtu, (sp->sideb_lan_multiple_access)  ?  ""  :  "no");
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) g1050_put(g1050_state_t *s, const uint8_t buf[], int len, int seq_no, double departure_time)
{
    g1050_queue_element_t *element;
    g1050_queue_element_t *e;
    double arrival_time;

    while (departure_time >= s->base_time + 1.0)
        g1050_simulate_chunk(s);
    /*endwhile*/
    arrival_time = s->arrival_times_1[(int) ((departure_time - s->base_time)*(double) s->packet_rate + 0.5)];
    if (arrival_time < 0)
    {
        /* This packet is lost */
        return 0;
    }
    /*endif*/
    if ((element = (g1050_queue_element_t *) malloc(sizeof(*element) + len)) == NULL)
        return -1;
    /*endif*/
    element->next = NULL;
    element->prev = NULL;
    element->seq_no = seq_no;
    element->departure_time = departure_time;
    element->arrival_time = arrival_time;
    element->len = len;
    memcpy(element->pkt, buf, len);
    /* Add it to the queue, in order */
    if (s->last == NULL)
    {
        /* The queue is empty */
        s->first =
        s->last = element;
    }
    else
    {
        for (e = s->last;  e;  e = e->prev)
        {
            if (e->arrival_time <= arrival_time)
                break;
            /*endif*/
        }
        /*endfor*/
        if (e)
        {
            element->next = e->next;
            element->prev = e;
            e->next = element;
        }
        else
        {
            element->next = s->first;
            s->first = element;
        }
        /*endif*/
        if (element->next)
            element->next->prev = element;
        else
            s->last = element;
        /*endif*/
    }
    /*endif*/
    //printf(">> Seq %d, departs %f, arrives %f\n", seq_no, departure_time, arrival_time);
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) g1050_get(g1050_state_t *s, uint8_t buf[], int max_len, double current_time, int *seq_no, double *departure_time, double *arrival_time)
{
    int len;
    g1050_queue_element_t *element;

    element = s->first;
    if (element == NULL)
    {
        if (seq_no)
            *seq_no = -1;
        /*endif*/
        if (departure_time)
            *departure_time = -1;
        /*endif*/
        if (arrival_time)
            *arrival_time = -1;
        /*endif*/
        return -1;
    }
    /*endif*/
    if (element->arrival_time > current_time)
    {
        if (seq_no)
            *seq_no = element->seq_no;
        /*endif*/
        if (departure_time)
            *departure_time = element->departure_time;
        /*endif*/
        if (arrival_time)
            *arrival_time = element->arrival_time;
        /*endif*/
        return -1;
    }
    /*endif*/
    /* Return the first packet in the queue */
    len = element->len;
    memcpy(buf, element->pkt, len);
    if (seq_no)
        *seq_no = element->seq_no;
    /*endif*/
    if (departure_time)
        *departure_time = element->departure_time;
    /*endif*/
    if (arrival_time)
        *arrival_time = element->arrival_time;
    /*endif*/
    //printf("<< Seq %d, arrives %f (%f)\n", element->seq_no, element->arrival_time, current_time);

    /* Remove it from the queue */
    if (s->first == s->last)
        s->last = NULL;
    /*endif*/
    s->first = element->next;
    if (element->next)
        element->next->prev = NULL;
    /*endif*/
    free(element);
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) g1050_queue_dump(g1050_state_t *s)
{
    g1050_queue_element_t *e;

    printf("Queue scanned forewards\n");
    for (e = s->first;  e;  e = e->next)
        printf("Seq %5d, arrival %10.4f, len %3d\n", e->seq_no, e->arrival_time, e->len);
    /*endfor*/
    printf("Queue scanned backwards\n");
    for (e = s->last;  e;  e = e->prev)
        printf("Seq %5d, arrival %10.4f, len %3d\n", e->seq_no, e->arrival_time, e->len);
    /*endfor*/
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
