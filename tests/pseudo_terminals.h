/*
 * SpanDSP - a series of DSP components for telephony
 *
 * pseudo_terminals.h - pseudo terminal handling.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2012 Steve Underwood
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

#if !defined(_SPANDSP_PSEUDO_TERMINALS_H_)
#define _SPANDSP_PSEUDO_TERMINALS_H_

#if !defined(HAVE_POSIX_OPENPT)  &&  !defined(HAVE_DEV_PTMX)  &&  !defined(WIN32)
#define USE_OPENPTY 1
#endif

struct pseudo_terminal_state_s
{
    int slot;
    int master_fd;
    int slave_fd;
    const char *stty;
    char *devlink;
    int block_read;
    int block_write;
    bool dtr;
    struct termios termios;
    logging_state_t logging;
};

typedef struct pseudo_terminal_state_s pseudo_terminal_state_t;

extern const char *pseudo_terminal_device_root_name;


#if defined(__cplusplus)
extern "C"
{
#endif

SPAN_DECLARE(logging_state_t *) pseudo_terminal_get_logging_state(pseudo_terminal_state_t *s);

SPAN_DECLARE(int) pseudo_terminal_check_termios(pseudo_terminal_state_t *pty);

SPAN_DECLARE(int) pseudo_terminal_release(pseudo_terminal_state_t *s);

SPAN_DECLARE(int) pseudo_terminal_free(pseudo_terminal_state_t *s);

SPAN_DECLARE(int) pseudo_terminal_restart(pseudo_terminal_state_t *s);

SPAN_DECLARE(pseudo_terminal_state_t *) pseudo_terminal_init(pseudo_terminal_state_t *s);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
