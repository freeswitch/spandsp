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
/*! \file */

#if !defined(_SPANDSP_SSLFAX_H_)
#define _SPANDSP_SSLFAX_H_

#if defined(SPANDSP_SUPPORT_SSLFAX)
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

typedef struct sslfax_state_s sslfax_state_t;

#if defined(__cplusplus)
extern "C"
{
#endif

SPAN_DECLARE(sslfax_state_t *) sslfax_init(sslfax_state_t *s);

SPAN_DECLARE(bool) sslfax_start_client(sslfax_state_t *s);

SPAN_DECLARE(void) sslfax_cleanup(sslfax_state_t *s, bool sustain);

SPAN_DECLARE(int) sslfax_tx(sslfax_state_t *s, int16_t amp[], int len);

SPAN_DECLARE(int) sslfax_rx(sslfax_state_t *s, const int16_t amp[], int len);

SPAN_DECLARE(void) sslfax_setup(sslfax_state_t *s,
                                span_put_msg_func_t put_msg,
                                span_get_msg_func_t get_msg,
                                hdlc_frame_handler_t hdlc_accept,
                                hdlc_underflow_handler_t hdlc_tx_underflow,
                                bool tx_use_hdlc,
                                bool rx_use_hdlc,
                                span_get_byte_func_t get_phase,
                                void *user_data);

SPAN_DECLARE(int) sslfax_write(sslfax_state_t *s, const uint8_t *buf, unsigned int count, int modem_fd, long int ms, bool filter, bool sustain);

SPAN_DECLARE(int) sslfax_read(sslfax_state_t *s, void *buf, size_t count, int modem_fd, long ms, bool sustain, bool carryon);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
