/*
 * SpanDSP - a series of DSP components for telephony
 *
 * pseudo_terminals.c - pseudo terminal handling.
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

#define _GNU_SOURCE

#include <inttypes.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>

#if defined(WIN32)
#include <windows.h>
#else
#if defined(__APPLE__)
#include <util.h>
#include <sys/ioctl.h>
#elif defined(__FreeBSD__)
#include <libutil.h>
#include <termios.h>
#else
#include <pty.h>
#endif
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#endif

#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES

#include "spandsp.h"

#include "pseudo_terminals.h"

const char *pseudo_terminal_device_root_name = "/dev/spandsp";

static int next_slot = 0;

#if 0
SPAN_DECLARE(pseudo_terminal_state_t *) pseudo_terminal_init(pseudo_terminal_state_t *s)
{
#if USE_OPENPTY
    if (openpty(&s->master_fd, &s->slave_fd_fd, NULL, NULL, NULL))
    {
        span_log(&s->logging, SPAN_LOG_ERROR, "Fatal error: failed to initialize pty\n");
        return -1;
    }
    s->stty = ttyname(s->slave_fd);
#else
#if defined(WIN32)
#elif defined(HAVE_POSIX_OPENPT)
    s->master_fd = posix_openpt(O_RDWR | O_NOCTTY);
#else
    /* The following 2 lines should be equivalent. Pick one. */
    s->master_fd = open("/dev/ptmx", O_RDWR);
    //s->master_fd = getpt();
#endif

#if defined(SOLARIS)
    /* Use the STREAMS terminal modules */
    ioctl(s->slave_fd, I_PUSH, "ptem");
    ioctl(s->slave_fd, I_PUSH, "ldterm");
#endif
#endif

    return 0;
}
/*- End of function --------------------------------------------------------*/
#endif

static char *make_message(const char *format, ...)
{
    va_list arg_ptr;
    size_t len;
    char  *buf;

    va_start(arg_ptr, format);
    len = vsnprintf(NULL, 0, format, arg_ptr);
    va_end(arg_ptr);
    if ((buf = malloc(len)) != NULL)
    {
        va_start(arg_ptr, format);
        vsprintf(buf, format, arg_ptr);
        va_end(arg_ptr);
    }
    /*endif*/
    return buf;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) pseudo_terminal_check_termios(pseudo_terminal_state_t *s)
{
    struct termios termios;

    if (tcgetattr(s->master_fd, &termios) < 0)
    {
        span_log(&s->logging, SPAN_LOG_ERROR, "Error getting termios - %s\n", strerror(errno));
    }
    else
    {
        if (memcmp(&termios, &s->termios, sizeof(termios)))
        {
            if (cfgetispeed(&termios) == B0  ||  cfgetospeed(&termios) == B0)
            {
                /* If you hunt the documentation enough, going to zero baud rate is supposed to mean
                   you also drop DTR. Moving from zero baud rate to something higher means you raise DTR */
                if (s->dtr)
                {
                    span_log(&s->logging, SPAN_LOG_DEBUG, "Drop DTR\n");
                    s->dtr = false;
                }
                s->termios = termios;
                return -1;
            }
            else
            {
                if (!s->dtr)
                {
                    span_log(&s->logging, SPAN_LOG_DEBUG, "Raise DTR\n");
                    s->dtr = true;
                }
                if ((termios.c_cflag & HUPCL))
                {
                    span_log(&s->logging, SPAN_LOG_DEBUG, "HUPCL\n");
                    s->termios = termios;
                    //if (s->state != STATE_MODEM_IDLE)
                    //    pty_hup(m, 1);
                    // TBD: drop DTR?
                    return -1;
                }
                else
                {
                    //s->pty_info |= TIOCM_DTR;
                }
                /*endif*/
            }
            /*endif*/
            s->termios = termios;
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) pseudo_terminal_get_logging_state(pseudo_terminal_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) pseudo_terminal_release(pseudo_terminal_state_t *s)
{
    int ret;

    if (s->devlink)
    {
        ret = unlink(s->devlink);
        free(s->devlink);
        s->devlink = NULL;
        if (ret)
            return -1;
        /*endif*/
    }
    /*endif*/

#if defined(WIN32)
    if (s->master_fd)
    {
        CloseHandle(s->master_fd);
        s->master_fd = INVALID_HANDLE_VALUE;
    }
    /*endif*/

    if (s->slave_fd)
    {
        CloseHandle(s->slave_fd);
        s->slave_fd = INVALID_HANDLE_VALUE;
    }
    /*endif*/
#else
    if (s->master_fd > -1)
    {
        shutdown(s->master_fd, 2);
        close(s->master_fd);
        s->master_fd = -1;
    }
    /*endif*/

    if (s->slave_fd > -1)
    {
        shutdown(s->slave_fd, 2);
        close(s->slave_fd);
        s->slave_fd = -1;
    }
    /*endif*/
#endif
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) pseudo_terminal_free(pseudo_terminal_state_t *s)
{
    int ret;

    ret = pseudo_terminal_release(s);
    span_free(s);
    return ret;
}
/*- End of function --------------------------------------------------------*/

static int pseudo_terminal_prepare(pseudo_terminal_state_t *s)
{
#if defined(WIN32)
    COMMTIMEOUTS timeouts = {0};
#endif
    struct termios termios;
    char name[128];

#if defined(WIN32)
    /* WIN32 doesn't have a pty system, like Linux, but you can install the tty0tty software
       that creates pairs of back to back tty ports. The you can use a TTY like you weould use
       the master side of a pty on Linux, */
    s->slot = 4 + next_slot++; /* Need work here. We start at COM4 for now */
    s->devlink = make_message("COM/%d", s->slot);

    s->master_fd = CreateFile(s->devlink,
                              GENERIC_READ | GENERIC_WRITE,
                              0,
                              0,
                              OPEN_EXISTING,
                              FILE_FLAG_OVERLAPPED,
                              0);
    if (s->master_fd == INVALID_HANDLE_VALUE)
    {
        if (GetLastError() == ERROR_FILE_NOT_FOUND)
            span_log(&s->logging, SPAN_LOG_ERROR, "Fatal error: Serial port does not exist\n");
        else
            span_log(&s->logging, SPAN_LOG_ERROR, "Fatal error: Serial port open error\n");
        /*endif*/
        return -1;
    }
    /*endif*/

    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;

    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;

    SetCommMask(s->master_fd, EV_RXCHAR);

    if (!SetCommTimeouts(s->master_fd, &timeouts))
    {
        span_log(&s->logging, SPAN_LOG_ERROR, "Cannot set up non-blocking read on %s\n", s->devlink);
        return -1;
    }
    /*endif*/
    s->threadAbort = CreateEvent(NULL, true, false, NULL);
#else
    /* The following 2 lines should be equivalent. Not every system may use the same device name, so
       a simple open may have portability issues. getpt() is a GNU thing, so that may be no better.
       Pick one. */
    if ((s->master_fd = open("/dev/ptmx", O_RDWR)) < 0)
    //if ((s->master_fd = getpt()) < 0)
    {
        span_log(&s->logging, SPAN_LOG_ERROR, "Failed to initialize UNIX98 master pty - %s\n", strerror(errno));
        return -1;
    }
    /*endif*/
    if (grantpt(s->master_fd) < 0)
    {
        span_log(&s->logging, SPAN_LOG_ERROR, "Failed to grant access to slave pty - %s\n", strerror(errno));
        return -1;
    }
    /*endif*/
    if (unlockpt(s->master_fd) < 0)
    {
        span_log(&s->logging, SPAN_LOG_ERROR, "Failed to unlock slave pty - %s\n", strerror(errno));
        return -1;
    }
    /*endif*/
    if (tcgetattr(s->master_fd, &termios) < 0)
    {
        span_log(&s->logging, SPAN_LOG_ERROR, "Failed to get pty configuration - %s\n", strerror(errno));
        return -1;
    }
    /*endif*/
    /* Configure as a non-canonical raw tty */
    cfmakeraw(&termios);
    cfsetispeed(&termios, B115200);
    cfsetospeed(&termios, B115200);
    if (tcsetattr(s->master_fd, TCSANOW, &termios) < 0)
    {
        span_log(&s->logging, SPAN_LOG_ERROR, "tcsetattr: %s\n", strerror(errno));
        return -1;
    }
    /*endif*/
    pseudo_terminal_check_termios(s);

    if (fcntl(s->master_fd, F_SETFL, fcntl(s->master_fd, F_GETFL, 0) | O_NONBLOCK))
    {
        span_log(&s->logging, SPAN_LOG_ERROR, "Cannot set up non-blocking read on %s\n", ttyname(s->master_fd));
        return -1;
    }
    /*endif*/

    /* ptsname() is not thread safe. Use ptsname_r() */
    if (ptsname_r(s->master_fd, name, 128) < 0)
    {
        span_log(&s->logging, SPAN_LOG_ERROR, "Failed to obtain slave pty filename\n");
        return -1;
    }
    /*endif*/
    s->stty = strdup(name);

    /* When the last slave side user closes a pty we get EIO reports. So, open the slave side and do nothing with
       it. The real slave users will never be the last to close the pty, and we will not get these errors. */
#if 0
    if ((s->slave_fd = open(s->stty, O_RDWR)) < 0)
    {
        span_log(&s->logging, SPAN_LOG_ERROR, "Failed to open slave pty %s\n", s->stty);
        return -1;
    }
    /*endif*/
#endif

    s->devlink = make_message("%s/%d", pseudo_terminal_device_root_name, s->slot);

    /* Remove any stale link which might be present */
    unlink(s->devlink);

    if (symlink(s->stty, s->devlink))
    {
        span_log(&s->logging, SPAN_LOG_ERROR, "Failed to create %s symbolic link - %s\n", s->devlink, strerror(errno));
        return -1;
    }
    /*endif*/
    /* Set the initial status of the pseudo modem */
    s->dtr = false;
#endif
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) pseudo_terminal_restart(pseudo_terminal_state_t *s)
{
    int ret;

    pseudo_terminal_release(s);
    ret = pseudo_terminal_prepare(s);
    if (ret < 0)
        pseudo_terminal_release(s);
    /*endif*/
    return ret;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(pseudo_terminal_state_t *) pseudo_terminal_init(pseudo_terminal_state_t *s)
{
    int ret;
    bool alloced;

    alloced = false;
    if (s == NULL)
    {
        if ((s = (pseudo_terminal_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
        /*endif*/
        alloced = true;
    }
    /*endif*/
    
    memset(s, 0, sizeof(*s));

    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "PTY");

#if defined(WIN32)
    s->master_fd = INVALID_HANDLE_VALUE;
    s->slave_fd = INVALID_HANDLE_VALUE;
#else
    s->master_fd = -1;
    s->slave_fd = -1;
#endif
    s->slot = next_slot++;

    ret = pseudo_terminal_prepare(s);
    if (ret < 0)
    {
        if (alloced)
            pseudo_terminal_free(s);
        else
            pseudo_terminal_release(s);
        /*endif*/
        return NULL;
    }
    /*endif*/
    return s;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
