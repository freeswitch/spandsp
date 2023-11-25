/*
 * Although highly modified and altered, the code in this file was originally
 * derived from sources taken from (1) HylaFAX+ on 13 June 2022 which states
 * that its source was derived from (2) GitHub user, "mrwicks", on 9 Oct 2018.
 * That source, itself, was derived from work by "Amlendra" published at
 * Aticleworld on 21 May 2017 (3).  That work, then, references programs (4)
 * Copyright (c) 2000 Sean Walton and Macmillan Publishers (The "Linux Socket
 * Programming" book) and are licensed under the GPL.
 *
 * 1. http://hylafax.sourceforge.net
 * 2. https://github.com/mrwicks/miscellaneous/tree/master/tls_1.2_example
 * 3. https://aticleworld.com/ssl-server-client-using-openssl-in-c/
 * 4. http://www.cs.utah.edu/~swalton/listings/sockets/programs/
 *
 * It is, therefore, presumed that this work is either under the public
 * domain or is licensed under the GPL.  A copy of the GPL is as follows...
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <string.h>
#include <sys/time.h>
#include <inttypes.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#else
#include "spandsp/stdbool.h"
#endif

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/async.h"
#include "spandsp/logging.h"
#include "spandsp/crc.h"
#include "spandsp/hdlc.h"
#include "spandsp/ssl_fax.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/ssl_fax.h"

#if defined(SPANDSP_SUPPORT_SSLFAX)
static void ShowCerts(sslfax_state_t *s)
{
    X509 *cert = SSL_get_peer_certificate(s->ssl);    /* get the server's certificate */
    span_log(&s->logging, SPAN_LOG_FLOW, "SSL Fax connection with %s encryption.\n", SSL_get_cipher(s->ssl));
    if (cert != NULL)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Server certificates: Subject: \"%s\", Issuer: \"%s\"\n",
                X509_NAME_oneline(X509_get_subject_name (cert), 0, 0),
                X509_NAME_oneline(X509_get_issuer_name (cert), 0, 0));
        X509_free(cert);                     /* free the malloc'ed certificate copy */
    }
    else
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Info: No client certificates configured.\n");
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static int OpenConnection(sslfax_state_t *s, const char *hostname, uint16_t port)
{
    int sd;
    struct hostent *host;
    struct sockaddr_in addr;

    if ((host = gethostbyname(hostname)) == NULL)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Problem with resolving host \"%s\".\n", hostname);
        return 0;
    }
    sd = socket(PF_INET, SOCK_STREAM, 0);
    if (fcntl(sd, F_SETFL, fcntl(sd, F_GETFL, 0) | O_NONBLOCK) == -1)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Unable to set SSL Fax socket to non-blocking.\n");
        return 0;
    }
    bzero(&addr, sizeof (addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = * (long*) (host->h_addr);

    if (connect(sd, (struct sockaddr*)&addr, sizeof (addr)) != 0)
    {
        if (errno == EINPROGRESS)
        {
            /* Now we wait for the connection to complete. */
            fd_set sfd;
            FD_ZERO(&sfd);
            FD_SET(sd, &sfd);
            struct timeval tv;
            tv.tv_sec = 2;
            tv.tv_usec = 0;
            if (!select(sd+1, NULL, &sfd, NULL, &tv))
            {
                close(sd);
                span_log(&s->logging, SPAN_LOG_FLOW, "Timeout waiting for SSL Fax connect completion.\n");
                return 0;
            }
            else
            {
                int code;
                socklen_t codelen = sizeof(code);
                if (!getsockopt(sd, SOL_SOCKET, SO_ERROR, &code, &codelen))
                {
                    if (!code)
                    {
                        /* Connect completed */
                        return sd;
                    }
                    else
                    {
                        span_log(&s->logging, SPAN_LOG_FLOW, "SSL Fax connection failed.  Error: %s\n", strerror(code));
                        close(sd);
                        return 0;
                    }
                }
                else
                {
                    close(sd);
                    span_log(&s->logging, SPAN_LOG_FLOW, "Unable to query the SSL Fax connection status.\n");
                    return 0;
                }
            }
        }
        span_log(&s->logging, SPAN_LOG_FLOW, "Unable to connect to SSL Fax receiver \"%s\" at port %d (%s)\n", hostname, port, strerror(errno));
        close(sd);
        return 0;
    }
    return sd;
}
/*- End of function --------------------------------------------------------*/

static SSL_CTX *InitCTX(void)
{
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    OpenSSL_add_all_algorithms();     /* Load cryptos, et.al. */
    SSL_load_error_strings();         /* Bring in and register error messages */
#if defined(SPANDSP_SUPPORT_FLEXSSL)
    method = TLS_client_method();     /* Create new client-method instance */
#else
    method = TLSv1_2_client_method(); /* Create new client-method instance */
#endif
    ctx = SSL_CTX_new(method);        /* Create new context */
    return ctx;
}
/*- End of function --------------------------------------------------------*/

static char *ssl_err_string(void)
{
    BIO *bio = BIO_new(BIO_s_mem());
    ERR_print_errors(bio);
    char *buf = NULL;
    size_t len = BIO_get_mem_data(bio, &buf);
    char *ret = (char *) calloc(1, 1 + len);
    if (ret) memcpy(ret, buf, len);
    BIO_free(bio);
    return ret;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) sslfax_setup(sslfax_state_t *s,
                                span_put_msg_func_t put_msg,
                                span_get_msg_func_t get_msg,
                                hdlc_frame_handler_t hdlc_accept,
                                hdlc_underflow_handler_t hdlc_tx_underflow,
                                bool tx_use_hdlc,
                                bool rx_use_hdlc,
                                span_get_byte_func_t get_phase,
                                void *user_data)
{
    s->put_msg = put_msg;
    s->get_msg = get_msg;
    s->hdlc_accept = hdlc_accept;
    s->hdlc_tx_underflow = hdlc_tx_underflow;
    s->tx_use_hdlc = tx_use_hdlc;
    s->rx_use_hdlc = rx_use_hdlc;
    s->get_phase = get_phase;
    s->user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

/*
 * This is where we produce len samples of audio to transmit.
 * As long as the SSL Fax connection remains active we stay silent.
 * Since we're not bound by time constraints we just send off all
 * the data at once because this function only gets invoked every
 * len samples of time.  This may lead us to taking longer than
 * len samples of time to perform, but it should be okay since
 * the audio is just silent, anyway.
 */
SPAN_DECLARE(int) sslfax_tx(sslfax_state_t *s, int16_t amp[], int len)
{
    uint8_t buf[2];
    bool sent = false;

    memset(amp, 0, len*sizeof(int16_t));

    if (! s->server  ||  !s->get_msg  ||  !s->hdlc_tx_underflow)
        return 0;

    if (s->do_underflow)
    {
        s->do_underflow = false; /* hdlc_tx_underflow may trigger another, so set it to false before. */
        if (s->hdlc_tx_underflow)
            s->hdlc_tx_underflow(s->user_data);
    }
    if (s->signal)
    {
        s->signal--;
        if (s->signal  &&  s->tx_use_hdlc)
        {
            /* Set up for underflow indication before next signal. */
            s->do_underflow = true;
        }
        if (! s->signal  &&  s->cleanup)
        {
            sslfax_cleanup(s, false);
        }
        return 0;
    }

    if (s->get_msg)
    {
        if (! s->tx_use_hdlc)
        {
            while (s->get_msg  &&  s->get_msg(s->user_data, buf, 1) == 1)
            {
                sent = true;
                sslfax_write(s, buf, 1, 0, 60000, true, false);
            }
            if (sent)
            {
                buf[0] = 0x10;  /* DLE */
                buf[1] = 0x03;  /* ETX */
                sslfax_write(s, buf, 2, 0, 60000, false, false);
                s->signal = 1;
                return 0;
            }
        }
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

/*
 * Received audio comes to us here.
 * As long as the SSL Fax connection remains active we deliberately ignore
 * what is going on in the audio.
 */
SPAN_DECLARE(int) sslfax_rx(sslfax_state_t *s, const int16_t amp[], int len)
{
    uint8_t rbuf[1];
    int bitpos = 0;
    int ones = 0;
    bool skipbit;
    unsigned int startbit;
    unsigned int j;
    uint16_t bit;

    if (!s->server  ||  !s->put_msg  ||  !s->hdlc_accept  ||  !s->get_phase)
        return 0;
    /*endif*/

    uint8_t buf[265];
    int pos = 0;
    int r;

    if (s->get_phase(s->user_data) == 7) /* T30_PHASE_C_ECM_RX */
    {
        /* We need to read the data stream, unstuff the zero bits, and break into frames on flags. */
        skipbit = false;
        buf[pos] = 0;
        for (;;)
        {
            r = sslfax_read(s, &rbuf[0], 1, 0, (pos ? 3000 : 0), false, false);
            if (r < 1)
                break;
            /*endif*/
            if (rbuf[0] == 0x10)
            {
                r = sslfax_read(s, &rbuf[0], 1, 0, 3000, false, false);
                if (r < 1)
                    break;
                /*endif*/
                if (rbuf[0] == 0x03)
                {
                    if (s->hdlc_accept)
                        s->hdlc_accept(s->user_data, NULL, SIG_STATUS_CARRIER_DOWN, true);
                    /*endif*/
                    break;
                }
                /*endif*/
            }
            /*endif*/
            startbit = skipbit  ?  1  :  0;
            skipbit = false;
            for (j = startbit;  j < 8;  j++)
            {
                bit = (rbuf[0] & (1 << j)) != 0  ?  1  :  0;
                if (bit == 1)
                    ones++;
                if (!(ones == 5  &&  bit == 0))   /* So, not transparent stuffed zero-bits */
                {
                    buf[pos] |= (bit << bitpos);
                    bitpos++;
                    if (bitpos == 8)            /* So, a fully populated byte */
                    {
                        if (++pos > 265)
                        {
                            span_log(&s->logging, SPAN_LOG_FLOW, "Invalid long ECM frame received via SSL Fax.\n");
                            return 0;
                        }
                        bitpos = 0;
                        buf[pos] = 0;
                    }
                }
                if (bit == 0)
                    ones = 0;
                /*endif*/
                if (ones == 6)                  /* So, a flag. Skip the trailing bit. Post the frame. */
                {
                    if (j == 7)
                    {
                        skipbit = true;
                    }
                    /*endif*/
                    j++;

                    if (s->hdlc_accept  &&  pos > 2)
                        s->hdlc_accept(s->user_data, buf, pos-2, crc_itu16_check(buf, pos));
                    /*endif*/
                    ones = 0;
                    pos = 0;
                    bitpos = 0;
                    buf[pos] = 0;
                }
                /*endif*/
            }
            /*endfor*/
        }
        /*endfor*/
        return 0;
    }
    /*endif*/

    for (;;)
    {
        do
        {
            r = sslfax_read(s, &buf[pos], 1, 0, (pos  ?  3000  :  0), false, false);
        }
        while (r > 0  &&  s->rx_use_hdlc  &&  pos == 0  &&  buf[pos] == 0x00);  /* Zero data may follow non-ECM Phase C after RTC. */
        if (r < 1)
            break;
        /*endif*/
        if (buf[pos] == 0x10)
        {
            r = sslfax_read(s, &buf[pos], 1, 0, 3000, false, false);
            if (r < 1)
                break;
            /*endif*/
            if (buf[pos] == 0x03)
            {
                if (! s->rx_use_hdlc)
                {
                    if (s->put_msg)
                        s->put_msg(s->user_data, NULL, SIG_STATUS_CARRIER_DOWN);
                    /*endif*/
                    return 0;
                }
                else
                {
                    if (pos == 0)
                    {
                        /* Was likely just zero data following non-ECM phase C after RTC. */
                        return 0;
                    }
                    /*endif*/
                    if (s->hdlc_accept  &&  pos > 2)
                        s->hdlc_accept(s->user_data, buf, pos-2, crc_itu16_check(buf, pos));
                    /*endif*/
                    if ((pos > 0)  &&  (buf[1] != 0x03))  /* 0x03 == CONTROL_FIELD_NON_FINAL_FRAME */
                    {
                        if (s->hdlc_accept)
                            s->hdlc_accept(s->user_data, NULL, SIG_STATUS_CARRIER_DOWN, true);
                        /*endif*/
                        return 0;
                    }
                    /*endif*/
                }
                /*endif*/
            }
            /*endif*/
        }
        /*endif*/
        if (! s->rx_use_hdlc)
        {
            if (s->put_msg)
                s->put_msg(s->user_data, &buf[pos], 1);
            /*endif*/
            pos--;
        }
        /*endif*/
        if (++pos > 265)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Invalid long frame received via SSL Fax.\n");
            break;
        }
        /*endif*/
    }
    /*endfor*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) sslfax_read(sslfax_state_t *s, void *buf, size_t count, int modem_fd, long ms, bool sustain, bool carryon)
{
    /*
     * We cannot just use select() on the socket to see if there is data waiting
     * to be read because the SSL encryption and decryption operates somewhat
     * independently of the socket activity. Likewise SSL_pending() will not
     * help us here as it only tells us about any data already in the buffer.
     * There really is no way around just calling SSL_read() and letting it
     * work its magic.  That is why we have it set to non-blocking I/O and are
     * prepared to then use select() if it returns an error indicating
     * SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE.
     *
     * With non-blocking sockets, SSL_ERROR_WANT_READ means "wait for the socket
     * to be readable, then call this function again."; conversely,
     * SSL_ERROR_WANT_WRITE means "wait for the socket to be writeable, then
     * call this function again.".
     *
     * We do this same thing with SSL_connect() and SSL_accept(), also.
     *
     * In the event that we do turn to a select() then here we also monitor the
     * modem for activity, since that would indicate failure of the SSL Fax
     * communication.
     *
     * The special modem_fd value of "0" tells us to not monitor the modem.
     * This is necessary because we can't select() a modem file descriptor if
     * it's at an EOF (it will always be readable).  The modem file descriptor
     * will be at an EOF if it is in command mode after an "OK" after a command
     * completed.  We can only select() it when we're waiting for a response.
     */

    int sslfd;
    int cerror;
    int ret;
    struct timeval start;      /* We need to monitor how much time this all takes from the start. */

    sslfd = s->client  ?  s->client  :  s->server;
    gettimeofday(&start, 0);
    do
    {
        cerror = 0;
        ret = SSL_read(s->ssl, buf, count);
        if (ret <= 0)
        {
            cerror = SSL_get_error(s->ssl, ret);
            if (cerror == SSL_ERROR_WANT_READ  ||  cerror == SSL_ERROR_WANT_WRITE)
            {
                if (!ms)
                    return (0);
                /*endif*/

                int selret;
                fd_set rfds;
                FD_ZERO(&rfds);
                if (modem_fd)
                    FD_SET(modem_fd, &rfds);
                /*endif*/
                struct timeval curTime;
                struct timeval tv;
                gettimeofday(&curTime, 0);
                tv.tv_sec = (int) ms / 1000;
                tv.tv_usec = (ms % 1000)*1000;
                timersub(&curTime, &start, &curTime);
                timersub(&tv, &curTime, &tv);
                if (cerror == SSL_ERROR_WANT_READ)
                {
                    /*  wait for the socket to be readable  */
                    FD_SET(sslfd, &rfds);
                    selret = select((modem_fd > sslfd)  ?  modem_fd + 1  :  sslfd + 1, &rfds, NULL, NULL, &tv);
                }
                else
                {
                    /*  SSL_ERROR_WANT_WRITE, wait for the socket to be writable  */
                    fd_set wfds;
                    FD_ZERO(&wfds);
                    FD_SET(sslfd, &wfds);
                    selret = select((modem_fd > sslfd)  ?  modem_fd + 1 : sslfd + 1, &rfds, &wfds, NULL, &tv);
                }
                /*endif*/
                if (!selret)
                {
                    span_log(&s->logging, SPAN_LOG_FLOW, "Timeout waiting for SSL Fax read (wanting to %s).\n", (cerror == SSL_ERROR_WANT_READ ? "read" : "write"));
                    sslfax_cleanup(s, sustain);
                    return (0);
                }
                else if (selret < 0)
                {
                    span_log(&s->logging, SPAN_LOG_FLOW, "Error waiting for SSL Fax read (wanting to %s): %s\n", (cerror == SSL_ERROR_WANT_READ ? "read" : "write"), strerror(errno));
                    sslfax_cleanup(s, sustain);
                    return (0);
                }
                /*endif*/
                if (modem_fd  &&  FD_ISSET(modem_fd, &rfds))
                {
                    /*  The modem got a signal.  This probably means that SSL Fax is not happening.  */
                    if (!carryon)
                    {
                        span_log(&s->logging, SPAN_LOG_FLOW, "Modem has data when waiting for SSL Fax read.  Terminating SSL Fax.\n");
                        sslfax_cleanup(s, sustain);
                    }
                    /*endif*/
                    return (-1);
                }
                /*endif*/
            }
            /*endif*/
        }
        /*endif*/
    }
    while (cerror == SSL_ERROR_WANT_READ  ||  cerror == SSL_ERROR_WANT_WRITE);
    if (ret <= 0)
    {
        if (cerror == SSL_ERROR_SYSCALL)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Unable to read from SSL Fax connection (syscall).  Error %d: %s\n", ret, strerror(ret));
        }
        else
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Unable to read from SSL Fax connection.  Error %d: %s\n", cerror, ssl_err_string());
        }
        /*endif*/
        sslfax_cleanup(s, sustain);
        return (-2);
    }
    /*endif*/
    return (ret);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) sslfax_write(sslfax_state_t *s, const uint8_t *buf, unsigned int count, int modem_fd, long ms, bool filter, bool sustain)
{
    /*
     * Similar approach here as with read() above; however...
     *
     * Because SSL Fax doesn't use carrier loss as a signal it uses
     * <DLE><ETX> as an end-of-data signal.  Therefore, we're required
     * here to "filter" DLEs (by doubling them) except for the end-of-
     * data signal; the receiver will be required to "un-filter" them
     * (by removing doubles and watching for the end-of-data signal).
     * So, we process buf one byte at a time.
     */
    unsigned int pos;
    bool isDLE = false;
    int cerror;
    int ret = 0;
    int sslfd = s->client ? s->client : s->server;
    struct timeval start;      /* We need to monitor how much time this all takes from the start. */

    gettimeofday(&start, 0);
    for (pos = 0;  pos < count;  pos++)
    {
        do
        {
            cerror = 0;
            ret = SSL_write(s->ssl, &buf[pos], 1);
            if (ret <= 0)
            {
                cerror = SSL_get_error(s->ssl, ret);
                if (cerror == SSL_ERROR_WANT_READ  ||  cerror == SSL_ERROR_WANT_WRITE)
                {
                    int selret;
                    fd_set rfds;
                    FD_ZERO(&rfds);
                    if (modem_fd)
                        FD_SET(modem_fd, &rfds);
                    /*endif*/
                    struct timeval curTime;
                    struct timeval tv;

                    gettimeofday(&curTime, 0);
                    tv.tv_sec = (int) ms/1000;
                    tv.tv_usec = (ms%1000)*1000;
                    timersub(&curTime, &start, &curTime);
                    timersub(&tv, &curTime, &tv);
                    if (cerror == SSL_ERROR_WANT_READ)
                    {
                        /*  wait for the socket to be readable */
                        FD_SET(sslfd, &rfds);
                        selret = select((modem_fd > sslfd)  ?  modem_fd + 1  :  sslfd + 1, &rfds, NULL, NULL, &tv);
                    }
                    else
                    {
                        /*   SSL_ERROR_WANT_WRITE, wait for the socket to be writable  */
                        fd_set wfds;
                        FD_ZERO(&wfds);
                        FD_SET(sslfd, &wfds);
                        selret = select((modem_fd > sslfd)  ?  modem_fd + 1  :  sslfd + 1, &rfds, &wfds, NULL, &tv);
                    }
                    /*endif*/
                    if (selret == 0)
                    {
                        span_log(&s->logging, SPAN_LOG_FLOW, "Timeout waiting for SSL Fax write (wanting to %s).\n", (cerror == SSL_ERROR_WANT_READ ? "read" : "write"));
                        sslfax_cleanup(s, sustain);
                        return (0);
                    }
                    /*endif*/
                    if (selret < 0)
                    {
                        span_log(&s->logging, SPAN_LOG_FLOW, "Error waiting for SSL Fax write (wanting to %s): %s\n", (cerror == SSL_ERROR_WANT_READ ? "read" : "write"), strerror(errno));
                        sslfax_cleanup(s, sustain);
                        return (0);
                    }
                    /*endif*/
                    if (modem_fd  &&  FD_ISSET(modem_fd, &rfds))
                    {
                        /*  The modem got a signal.  This probably means that SSL Fax is not happening.  */
                        span_log(&s->logging, SPAN_LOG_FLOW, "Modem has data when waiting for SSL Fax write.  Terminating SSL Fax.\n");
                        sslfax_cleanup(s, sustain);
                        return (-1);
                    }
                    /*endif*/
                }
                /*endif*/
            }
            /*endif*/
        }
        while (cerror == SSL_ERROR_WANT_READ  ||  cerror == SSL_ERROR_WANT_WRITE);
        if (ret <= 0)
        {
            if (cerror == SSL_ERROR_SYSCALL)
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "Unable to write to SSL Fax connection (syscall).  Error %d: %s\n", ret, strerror(ret));
            }
            else
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "Unable to write to SSL Fax connection.  Error %d: %s\n", cerror, ssl_err_string());
            }
            /*endif*/
            sslfax_cleanup(s, sustain);
            return (-2);
        }
        /*endif*/
        if (filter  &&  buf[pos] == 16  &&  !isDLE)
        {
            /*  We need to duplicate this DLE.  We do that by forcing the loop to repeat this byte once. */
            pos--;
            isDLE = true;
        }
        else
        {
            isDLE = false;
        }
        /*endif*/
    }
    /*endfor*/
    return (count);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(bool) sslfax_start_client(sslfax_state_t *s)
{
    char *host;
    char *port;
    char *passcode;
    char *b;
    struct timeval start;
    int cerror;
    int ret;

    span_log(&s->logging, SPAN_LOG_FLOW, "Starting SSL Fax client, URL: %s\n", s->url);

    /* Initialize the SSL library */
    SSL_library_init();

    s->ctx = InitCTX();
    if (s->ctx == NULL)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Could not initialize OpenSSL client CTX\n");
        return false;
    }
    /*endif*/

    /* The SSL Fax URL is of the format <passcode>@<host>:<port>, Example: s8V6q7at1B@[192.168.0.31]:10000 */

    passcode = span_alloc(strlen(s->url) + 1);
    strcpy(passcode, s->url);
    port = strrchr(passcode, ':');
    host = strchr(passcode, '@');
    if (host  &&  port  &&  (port > host + 1)  &&  host[1] == '[')
    {
        host[0] = '\0';
        host++;
        b = strchr(host, ']');
        if (b)
            b[0] = '\0';
        /*endif*/
    }
    /*endif*/
    if (!port  ||  !host  ||  (port < host + 2)  ||  (host < passcode + 1))
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Could not parse SSL Fax URL: \"%s\"\n", s->url);
        span_free(passcode);
        return false;
    }
    /*endif*/
    port[0] = '\0';
    host[0] = '\0';
    port++;
    host++;

    /* We need to monitor how much time this all takes from the start. */
    gettimeofday(&start, 0);

    s->server = OpenConnection(s, host, atoi(port));
    if (s->server <= 0)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Could not open connection to SSL Fax URL: \"%s\", OpenConnection returned %d\n", s->url, s->server);
        sslfax_cleanup(s, false);
        span_free(passcode);
        return false;
    }
    /*endif*/
    s->ssl = SSL_new(s->ctx);        /* get new SSL state with context */
    SSL_set_fd(s->ssl, s->server);   /* attach the socket descriptor */

    do
    {
        cerror = 0;
        ret = SSL_connect(s->ssl);    /* perform the connection */
        if (ret <= 0)
        {
            cerror = SSL_get_error(s->ssl, ret);
            /*
             * SSL_connect() can fail with SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE
             * because we're using a non-blocking socket.  These conditions
             * probably mean that the server has an open socket but that it
             * hasn't yet started its SSL_accept() - in other words, we may
             * just be a bit ahead of the receiver.  So, according to the
             * SSL_connect() man page we then will need to use a select()
             * on the socket for read or write and then re-run SSL_connect().
             * We are under a time constraint, however.  So, we have to
             * also watch for that.
             */
            if (cerror == SSL_ERROR_WANT_READ  ||  cerror == SSL_ERROR_WANT_WRITE)
            {
                fd_set sfd;
                FD_ZERO(&sfd);
                FD_SET(s->server, &sfd);
                struct timeval curTime;
                struct timeval tv;
                gettimeofday(&curTime, 0);
                tv.tv_sec = 2;
                tv.tv_usec = 0;
                timersub(&curTime, &start, &curTime);
                timersub(&tv, &curTime, &tv);
                if (cerror == SSL_ERROR_WANT_READ)
                {
                    if (!select(s->server + 1, &sfd, NULL, NULL, &tv))
                    {
                        span_log(&s->logging, SPAN_LOG_FLOW, "Timeout waiting for SSL Fax connection (wanting to read).\n");
                        sslfax_cleanup(s, false);
                        span_free(passcode);
                        return false;
                    }
                    /*endif*/
                }
                else
                {
                    /*  SSL_ERROR_WANT_WRITE */
                    if (!select(s->server + 1, NULL, &sfd, NULL, &tv))
                    {
                        span_log(&s->logging, SPAN_LOG_FLOW, "Timeout waiting for SSL Fax connection (wanting to write).\n");
                        sslfax_cleanup(s, false);
                        span_free(passcode);
                        return false;
                    }
                    /*endif*/
                }
                /*endif*/
            }
            /*endif*/
        }
        /*endif*/
    }
    while (cerror == SSL_ERROR_WANT_READ  ||  cerror == SSL_ERROR_WANT_WRITE);
    if (ret <= 0)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Unable to connect to \"%s\".  Error %d: %s\n", s->url, cerror, ssl_err_string());
        sslfax_cleanup(s, false);
        span_free(passcode);
        return (false);
    }
    /*  Now send the passcode.  */
    if (sslfax_write(s, (const uint8_t *) passcode, strlen(passcode), 0, 1000, false, false) <= 0)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "SSL Fax passcode write failed.\n");
        sslfax_cleanup(s, false);
        span_free(passcode);
        return (false);
    }
    ShowCerts(s);
    span_free(passcode);
    return true;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(sslfax_state_t *) sslfax_init(sslfax_state_t *s)
{
    if (s == NULL)
    {
        if ((s = (sslfax_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
        /*endif*/
    }
    /*endif*/
    memset(s, 0, sizeof(*s));
    s->url = NULL;
    s->ctx = NULL;
    s->ssl = NULL;
    s->server = 0;
    s->client = 0;
    s->rcp_count = 0;
    s->ecm_ones = 0;
    s->ecm_bitpos = 0;
    s->ecm_byte = 0;
    s->user_data = NULL;
    s->get_msg = NULL;
    s->put_msg = NULL;
    s->hdlc_accept = NULL;
    s->tx_use_hdlc = false;
    s->rx_use_hdlc = false;
    s->signal = 0;
    s->do_underflow = false;
    s->cleanup = false;
    s->get_phase = NULL;

    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "SSL Fax");
    span_log_set_level(&s->logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);

    span_log(&s->logging, SPAN_LOG_FLOW, "Initialize\n");

    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) sslfax_cleanup(sslfax_state_t *s, bool sustain)
{
    char buf[1];
    bool done;
    fd_set sfd;
    struct timeval tv;

    if (s->signal)
    {
        /* We can't shut down yet, as sslfax_tx still has signals pending. */
        s->cleanup = true;
        return;
    }

//    int times = 3;
//    while (times--  &&  s->hdlc_tx_underflow  &&  s->user_data  &&  s->do_underflow)
//    {
//        s->do_underflow = false; /* hdlc_tx_underflow may trigger another, so set it to false before. */
//        if (s->hdlc_tx_underflow)
//            s->hdlc_tx_underflow(s->user_data);
//    }

    s->rcp_count = 0;
    s->ecm_ones = 0;
    s->ecm_bitpos = 0;
    s->ecm_byte = 0;
    s->user_data = NULL;
    s->get_msg = NULL;
    s->put_msg = NULL;
    s->hdlc_accept = NULL;
    s->tx_use_hdlc = false;
    s->rx_use_hdlc = false;
    s->ssl = NULL;
    s->signal = 0;
    s->do_underflow = false;
    s->cleanup = true;
    s->get_phase = NULL;

    if (s->url)
    {
        span_free(s->url);
        s->url = NULL;
    }

    if (!sustain)
    {
        if (s->ctx)
        {
            ERR_free_strings();     /* Free memory from SSL_load_error_strings */
            EVP_cleanup();          /* Free memory from OpenSSL_add_all_algorithms */
            SSL_CTX_free(s->ctx);   /* Release context */
        }
        s->ctx = NULL;
        if (s->server)
        {
            /*
             * This is the client.  We want the client-side to shut down
             * first so that the server-side is not left with TIME_WAIT.
             * We'll get the TIME_WAIT on the client-side, and that's okay.
             */
             shutdown(s->server, SHUT_RDWR);
             close(s->server);
        }
        s->server = 0;
    }
    if (s->client)
    {
        /*
         * This is the server.  We want to avoid TIME_WAIT, and so we
         * wait up to 5 seconds for the client to shut down, and if
         * they don't, then we'll RST the connection using SO_LINGER.
         */
        fcntl(s->client, F_SETFL, fcntl(s->server, F_GETFL, 0) &~ O_NONBLOCK);  /* we want the read() below to block. */

        done = false;
        FD_ZERO(&sfd);
        FD_SET(s->client, &sfd);
        do
        {
            tv.tv_sec = 5;
            tv.tv_usec = 0;
            if (!select(s->client + 1, &sfd, NULL, NULL, &tv))
            {
                /* The client did not shut down first.  RST the connection. */
                struct linger ling;
                ling.l_onoff = 1;
                ling.l_linger = 0;
                setsockopt(s->client, SOL_SOCKET, SO_LINGER, (char *) &ling, sizeof(ling));
                done = true;
            }
            else
            {
                done = (read(s->client, buf, 1) <= 0);
            }
            /*endif*/
        }
        while (!done);
        close(s->client);
    }
    s->client = 0;
    return;
}
/*- End of function --------------------------------------------------------*/
#endif
/*- End of file ------------------------------------------------------------*/
