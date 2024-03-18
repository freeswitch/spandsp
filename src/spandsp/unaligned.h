/*
 * SpanDSP - a series of DSP components for telephony
 *
 * unaligned.h - Cross platform unaligned data access
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2006, 2022 Steve Underwood
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

#if !defined(_SPANDSP_UNALIGNED_H_)
#define _SPANDSP_UNALIGNED_H_

#if defined(__cplusplus)
extern "C"
{
#endif

#if defined(_MSC_VER)
#define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop))

PACK(struct __dealign_uint16 { uint16_t datum; };)
PACK(struct __dealign_uint32 { uint32_t datum; };)
PACK(struct __dealign_uint64 { uint64_t datum; };)
#endif

#if defined(__GNUC__)  ||  defined(__clang__)
struct __dealign_uint16 { uint16_t datum; } __attribute__((packed));
struct __dealign_uint32 { uint32_t datum; } __attribute__((packed));
struct __dealign_uint64 { uint64_t datum; } __attribute__((packed));
#endif

#if defined(__GNUC__)  ||  defined(__clang__)  ||  defined(_MSC_VER)
/* If we just tell GCC what's going on, we can trust it to behave optimally */
static __inline__ uint64_t get_unaligned_uint64(const void *p)
{
    const struct __dealign_uint64 *pp = (const struct __dealign_uint64 *) p;

    return pp->datum;
}
/*- End of function --------------------------------------------------------*/

static __inline__ void put_unaligned_uint64(void *p, uint32_t datum)
{
    struct __dealign_uint64 *pp = (struct __dealign_uint64 *) p;

    pp->datum = datum;
}
/*- End of function --------------------------------------------------------*/

static __inline__ uint64_t get_net_unaligned_uint64(const void *p)
{
    const struct __dealign_uint64 *pp = (const struct __dealign_uint64 *) p;

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return pp->datum;
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return __builtin_bswap64(pp->datum);
#endif
}
/*- End of function --------------------------------------------------------*/

static __inline__ void put_net_unaligned_uint64(void *p, uint64_t datum)
{
    struct __dealign_uint64 *pp = (struct __dealign_uint64 *) p;

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    pp->datum = datum;
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    pp->datum = __builtin_bswap64(datum);
#endif
}
/*- End of function --------------------------------------------------------*/

static __inline__ uint32_t get_unaligned_uint32(const void *p)
{
    const struct __dealign_uint32 *pp = (const struct __dealign_uint32 *) p;

    return pp->datum;
}
/*- End of function --------------------------------------------------------*/

static __inline__ void put_unaligned_uint32(void *p, uint32_t datum)
{
    struct __dealign_uint32 *pp = (struct __dealign_uint32 *) p;

    pp->datum = datum;
}
/*- End of function --------------------------------------------------------*/

static __inline__ uint32_t get_net_unaligned_uint32(const void *p)
{
    const struct __dealign_uint32 *pp = (const struct __dealign_uint32 *) p;

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return pp->datum;
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return __builtin_bswap32(pp->datum);
#endif
}
/*- End of function --------------------------------------------------------*/

static __inline__ void put_net_unaligned_uint32(void *p, uint32_t datum)
{
    struct __dealign_uint32 *pp = (struct __dealign_uint32 *) p;

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    pp->datum = datum;
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    pp->datum = __builtin_bswap32(datum);
#endif
}
/*- End of function --------------------------------------------------------*/

static __inline__ uint16_t get_unaligned_uint16(const void *p)
{
    const struct __dealign_uint16 *pp = (const struct __dealign_uint16 *) p;

    return pp->datum;
}
/*- End of function --------------------------------------------------------*/

static __inline__ void put_unaligned_uint16(void *p, uint16_t datum)
{
    struct __dealign_uint16 *pp = (struct __dealign_uint16 *) p;

    pp->datum = datum;
}
/*- End of function --------------------------------------------------------*/

static __inline__ uint16_t get_net_unaligned_uint16(const void *p)
{
    const struct __dealign_uint16 *pp = (const struct __dealign_uint16 *) p;

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return pp->datum;
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return __builtin_bswap16(pp->datum);
#endif
}
/*- End of function --------------------------------------------------------*/

static __inline__ void put_net_unaligned_uint16(void *p, uint16_t datum)
{
    struct __dealign_uint16 *pp = (struct __dealign_uint16 *) p;

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    pp->datum = datum;
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    pp->datum = __builtin_bswap16(datum);
#endif
}
/*- End of function --------------------------------------------------------*/

#elif defined(SOLARIS)  &&  defined(__sparc__)

static __inline__ uint64_t get_unaligned_uint64(const void *p)
{
    const uint8_t *cp = p;

    return (cp[0] << 56) | (cp[1] << 48) | (cp[2] << 40) | (cp[3] << 32) | (cp[4] << 24) | (cp[5] << 16) | (cp[6] << 8) | cp[7];
}
/*- End of function --------------------------------------------------------*/

static __inline__ void put_unaligned_uint64(void *p, uint64_t datum)
{
    const uint8_t *cp = p;

    cp[0] = datum >> 56;
    cp[1] = datum >> 48;
    cp[2] = datum >> 40;
    cp[3] = datum >> 32;
    cp[4] = datum >> 24;
    cp[5] = datum >> 16;
    cp[6] = datum >> 8;
    cp[7] = datum;
}
/*- End of function --------------------------------------------------------*/

static __inline__ uint32_t get_unaligned_uint32(const void *p)
{
    const uint8_t *cp = p;

    return (cp[0] << 24) | (cp[1] << 16) | (cp[2] << 8) | cp[3];
}
/*- End of function --------------------------------------------------------*/

static __inline__ void put_unaligned_uint32(void *p, uint32_t datum)
{
    const uint8_t *cp = p;

    cp[0] = datum >> 24;
    cp[1] = datum >> 16;
    cp[2] = datum >> 8;
    cp[3] = datum;
}
/*- End of function --------------------------------------------------------*/

static __inline__ uint16_t get_unaligned_uint16(const void *p)
{
    const uint8_t *cp = p;

    return (cp[0] << 8) | cp[1];
}
/*- End of function --------------------------------------------------------*/

static __inline__ void put_unaligned_uint16(void *p, uint16_t datum)
{
    uint8_t *cp = p;

    cp[0] = datum >> 8;
    cp[1] = datum;
}
/*- End of function --------------------------------------------------------*/

#else

/* The generic case. Assume we can handle direct load/store. */
#define get_unaligned_uint64(p) (*((uint64_t *) (p)))
#define put_unaligned_uint64(p,d) do { uint64_t *__P = (p); *__P = d; } while(0)
#define get_net_unaligned_uint64(p) (*((uint64_t *) (p)))
#define put_net_unaligned_uint64(p,d) do { uint64_t *__P = (p); *__P = d; } while(0)
#define get_unaligned_uint32(p) (*((uint32_t *) (p)))
#define put_unaligned_uint32(p,d) do { uint32_t *__P = (p); *__P = d; } while(0)
#define get_net_unaligned_uint32(p) (*((uint32_t *) (p)))
#define put_net_unaligned_uint32(p,d) do { uint32_t *__P = (p); *__P = d; } while(0)
#define get_unaligned_uint16(p) (*((uint16_t *) (p)))
#define put_unaligned_uint16(p,d) do { uint16_t *__P = (p); *__P = d; } while(0)
#define get_net_unaligned_uint16(p) (*((uint16_t *) (p)))
#define put_net_unaligned_uint16(p,d) do { uint16_t *__P = (p); *__P = d; } while(0)
#endif

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
