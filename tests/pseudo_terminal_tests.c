/*
 * SpanDSP - a series of DSP components for telephony
 *
 * pseudo_terminal_tests.c - pseudo terminal handling tests.
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

#include <inttypes.h>
#include <stdlib.h>

#if defined(WIN32)
#include <windows.h>
#else
#if defined(__APPLE__)
#include <util.h>
#elif defined(__FreeBSD__)
#include <libutil.h>
#include <termios.h>
#include <sys/socket.h>
#else
#include <pty.h>
#endif
#include <sys/ioctl.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#endif

#include "spandsp.h"

#include "spandsp/t30_fcf.h"

#include "spandsp-sim.h"

#undef SPANDSP_EXPOSE_INTERNAL_STRUCTURES

#include "pseudo_terminals.h"

static int master(void)
{
    struct timeval tmo;
    fd_set rset;
    fd_set eset;
    pseudo_terminal_state_t pty[10];
    int active[10];
    char buf[1024];
    int len;
    int i;
    int ret;
    static int seq[10] = {0};
    logging_state_t *logging;

    for (i = 0;  i < 10;  i++)
    {
        if (pseudo_terminal_init(&pty[i]) == NULL)
        {
            printf("Failure\n");
            exit(2);
        }
        /*endif*/
        printf("%s %s\n", pty[i].devlink, pty[i].stty);
        logging = pseudo_terminal_get_logging_state(&pty[i]);
        span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG);
        active[i] = true;
    }
    /*endfor*/

    for (;;)
    {
        tmo.tv_sec = 1;
        tmo.tv_usec= 0;

        FD_ZERO(&rset);
        FD_ZERO(&eset);
        for (i = 0;  i < 10;  i++)
        {
            if (active[i])
            {
                FD_SET(pty[i].master_fd, &rset);
                FD_SET(pty[i].master_fd, &eset);
            }
            /*endif*/
        }
        /*endfor*/
        if ((ret = select(256, &rset, NULL, &eset, &tmo)) < 0)
        {
            if (errno == EINTR)
                continue;
            /*endif*/
            printf("select: %s\n", strerror(errno));
            return ret;
        }
        /*endif*/
        if (ret == 0)
        {
            /* If things are quiet, check if the termios have changed, as none of the
               read, write or exception options will get kicked by a termios update */
            for (i = 0;  i < 10;  i++)
            {
                if (pseudo_terminal_check_termios(&pty[i]) < 0)
                {
                    seq[i]++;
                }
                /*endif*/
            }
            /*endfor*/
            continue;
        }
        /*endif*/
        for (i = 0;  i < 10;  i++)
        {
            if (FD_ISSET(pty[i].master_fd, &rset))
            {
                //if (active[i])
                {
                    if (pseudo_terminal_check_termios(&pty[i]) < 0)
                    {
                        seq[i]++;
                    }
                    /*endif*/
                    errno = 0;
                    len = read(pty[i].master_fd, buf, 4);
                    if (len >= 0)
                    {
                        buf[len] = '\0';
                        printf("%d %d '%s' %s\n", i, len, buf, strerror(errno));
                        buf[0] = 'A';
                        buf[1] = 'B';
                        buf[2] = 'C' + i;
                        buf[3] = 'D' + seq[i];
                        write(pty[i].master_fd, buf, 4);
                        //printf("wait\n");
                        //sleep(1);
                    }
                    else
                    {
                        if (errno == EAGAIN)
                        {
                            /* This is harmless */
                        }
                        else if (errno == EIO)
                        {
                            /* This happens when the last slave closes */
                            if (pseudo_terminal_check_termios(&pty[i]) < 0)
                            {
                            }
                            /*endif*/
                            if (pty[i].termios.c_cflag & HUPCL)
                            {
                                printf("Restarting %d\n", i);
                                pseudo_terminal_restart(&pty[i]);
                            }
                            /*endif*/
                            errno = 0;
                            //active[i] = false;
                        }
                        else
                        {
                            printf("Error %s\n", strerror(errno));
                        }
                        /*endif*/
                    }
                    /*endif*/
                }
                /*endif*/
            }
            /*endfor*/
            if (FD_ISSET(pty[i].master_fd, &eset))
            {
                printf("XXXXX\n");
            }
            /*endif*/
        }
        /*endfor*/
    }
    /*endfor*/

    for (i = 0;  i < 10;  i++)
    {
        if (pseudo_terminal_release(&pty[i]))
        {
            printf("Failure\n");
            exit(2);
        }
        /*endif*/
    }
    /*endfor*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int slave(void)
{
    struct termios termios;
    int fd[10];
    char name[64];
    char response[64];
    char link[1024];
    int len;
    int i;
    int j;

    for (i = 0;  i < 10;  i++)
    {
        sprintf(name, "%s/%d", pseudo_terminal_device_root_name, i);
        if ((fd[i] = open(name, O_RDWR)) < 0)
        {
            printf("Failed to open %s\n", name);
            exit(2);
        }
        /*endif*/
        len = readlink(name, link, 1024);
        if (len >= 0)
            link[len] = '\0';
        /*endif*/
        printf("%s %s\n", name, link);
        if (tcgetattr(fd[i], &termios) < 0)
        {
            printf("tcgetattr: %s\n", strerror(errno));
            return -1;
        }
        /*endif*/
        cfsetispeed(&termios, B9600);
        cfsetospeed(&termios, B9600);
        termios.c_cflag &= ~HUPCL;

        if (tcsetattr(fd[i], TCSANOW, &termios) < 0)
        {
            printf("tcsetattr: %s\n", strerror(errno));
            return -1;
        }
        /*endif*/
    }
    /*endfor*/
    printf("All open\n");
    for (j = 0;  j < 10;  j++)
    {
        for (i = 0;  i < 10;  i++)
        {
            write(fd[i], "FRED", 4);
            len = read(fd[i], response, 4);
            if (len > 0)
                printf("%d %d '%s'\n", i, len, response);
            else
                printf("%d %s\n", i, strerror(errno));
            /*endif*/
        }
        /*endfor*/
    }
    /*endfor*/
    printf("All exchanged\n");

    for (i = 0;  i < 10;  i++)
    {
        if (tcgetattr(fd[i], &termios) < 0)
        {
            printf("tcgetattr: %s\n", strerror(errno));
            return -1;
        }
        /*endif*/
        cfsetispeed(&termios, B0);
        cfsetospeed(&termios, B0);
        termios.c_cflag |= HUPCL;
        if (tcsetattr(fd[i], TCSANOW, &termios) < 0)
        {
            printf("tcsetattr: %s\n", strerror(errno));
            return -1;
        }
        /*endif*/
        if (close(fd[i]))
        {
            printf("Failed to close %d\n", i);
            exit(2);
        }
        /*endif*/
    }
    /*endfor*/

    return 0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int opt;
    bool calling_party;

    calling_party = false;
    while ((opt = getopt(argc, argv, "ac")) != -1)
    {
        switch (opt)
        {
        case 'a':
            calling_party = false;
            break;
        case 'c':
            calling_party = true;
            break;
        default:
            //usage();
            exit(2);
            break;
        }
        /*endswitch*/
    }
    /*endwhile*/

    if (calling_party)
        slave();
    else
        master();
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
