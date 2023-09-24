/*
 * The code in this file was derived from sources taken from (1) HylaFAX+ on
 * 13 June 2022. That source states that it was derived from (2) GitHub user,
 * "mrwicks", on 9 Oct 2018.  That source, itself, was derived from work by
 * "Amlendra" published at Aticleworld on 21 May 2017 (3).  That work, then,
 * references programs (4) Copyright (c) 2000 Sean Walton and Macmillan
 * Publishers (The "Linux Socket Programming" book) and are licensed under
 * the GPL.
 *
 * 1. https://hylafax.sourceforge.net
 * 2. https://github.com/mrwicks/miscellaneous/tree/master/tls_1.2_example
 * 3. https://aticleworld.com/ssl-server-client-using-openssl-in-c/
 * 4. http://www.cs.utah.edu/~swalton/listings/sockets/programs/
 *
 * It is, therefore, presumed that this work is either under the* public
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
//#ifndef __sslfax_H
//#define __sslfax_H
//
//#include <errno.h>
//#include <unistd.h>
//#include <string.h>
//#include <arpa/inet.h>
//#include <sys/socket.h>
//#include <sys/types.h>
//#include <netinet/in.h>
//#include <resolv.h>
//#include <stdio.h>
//#include <netdb.h>
//#include <openssl/ssl.h>
//#include <openssl/err.h>
//
//#include "Str.h"
//
//struct SSLFaxProcess {
//    SSL_CTX *ctx;
//    SSL *ssl;
//    int server;
//    int client;
//    fxStr emsg;		// error message
//};
//
//struct SSLFax {
//public:
//    int getAddressFamily(fxStr& address);
//    SSLFaxProcess null();
//    SSLFaxProcess startServer(fxStr info, fxStr pemFile);
//    SSLFaxProcess startClient(fxStr info, fxStr passcode, const uint8_t *bitrev, long ms);
//    void acceptClient(SSLFaxProcess& sfp, fxStr passcode, int modemFd, long ms);
//    bool acceptClient1(SSLFaxProcess& sfp, long ms, bool sustain = false);
//    void acceptClient2(SSLFaxProcess& sfp, fxStr passcode, int modemFd, long ms, bool sustain = false);
//    void cleanup(SSLFaxProcess& sfp, bool sustain = false);
//    int pending(SSLFaxProcess& sfp);
//    int read(SSLFaxProcess& sfp, void *buf, size_t count, int modemFd, long ms, bool sustain = false, bool carryon = false);
//    int write(SSLFaxProcess& sfp, const uint8_t *buf, unsigned int count, const uint8_t *bitrev, int modemFd, long ms, bool eod, bool sustain = false);
//};
//
//#endif

/*! \file */

#if !defined(_SPANDSP_PRIVATE_SSLFAX_H_)
#define _SPANDSP_PRIVATE_SSLFAX_H_

#include <openssl/ssl.h>
#include <openssl/err.h>

/*!
    SSL Fax connection descriptor. This defines the state of a single
    instance of an SSL Fax connection.
*/
struct sslfax_state_s
{
    /*! \brief The remote SSL Fax URL, if known, else NULL. */
    char* url;
    SSL_CTX *ctx;
    SSL *ssl;
    int server;
    int client;
    int rcp_count;
    int ecm_ones;
    int ecm_bitpos;
    uint8_t ecm_byte;
    bool doread;
    int signal;
    bool do_underflow;
    bool cleanup;

    get_byte_func_t get_phase;

    /*! \brief A user specified opaque pointer passed to the put, get, and hdlc routines. */
    void *msg_user_data;
    /*! \brief The callback function used to get bytes to be transmitted. */
    get_msg_func_t get_msg;
    /*! \brief The callback function used to put bytes received. */
    put_msg_func_t put_msg;
    /*! \brief The callback function used to accept HDLC frames. */
    hdlc_frame_handler_t hdlc_accept;
    /*! \brief The callback function used for HDLC underflow indication. */
    hdlc_underflow_handler_t hdlc_tx_underflow;
    /*! \brief Whether or not the data represents HDLC or not. */
    bool tx_use_hdlc;
    bool rx_use_hdlc;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

#endif
/*- End of file ------------------------------------------------------------*/
