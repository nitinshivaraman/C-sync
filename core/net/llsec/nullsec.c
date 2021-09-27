/*
 * Copyright (c) 2013, Hasso-Plattner-Institut.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         Insecure link layer security driver.
 * \author
 *         Konrad Krentz <konrad.krentz@gmail.com>
 */

/**
 * \addtogroup nullsec
 * @{
 */

#include "net/llsec/nullsec.h"
#include "net/mac/frame802154.h"
#include "net/netstack.h"
#include "net/packetbuf.h"
#include "dev/cc2420/cc2420.h"
#include "sys/rtimer.h"

#include "net/c-sync/c-sync.h"

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

static uint8_t key[16] = { 0x00 , 0x01 , 0x02 , 0x03 ,
                  0x04 , 0x05 , 0x06 , 0x07 ,
                  0x08 , 0x09 , 0x0A , 0x0B ,
                  0x0C , 0x0D , 0x0E , 0x0F };

static uint8_t block[AES_128_BLOCK_SIZE];

static uint8_t do_encrypt_decrypt = 0;

/*---------------------------------------------------------------------------*/
static void
init(void)
{



	memset(block, 0, AES_128_BLOCK_SIZE);

	if(packetbuf_attr(PACKETBUF_ATTR_CSYNC_CONN_DOAES))
	{
		
		AES_128.set_key(key);
		uint8_t i;

		PRINTF("\nA %u, B %u", packetbuf_attr(PACKETBUF_ATTR_CSYNC_CONN_AES_XORPLAINTEXT_A), packetbuf_attr(PACKETBUF_ATTR_CSYNC_CONN_AES_XORPLAINTEXT_B));

		uint32_t joined_xor = (uint32_t)packetbuf_attr(PACKETBUF_ATTR_CSYNC_CONN_AES_XORPLAINTEXT_A) *
							  (uint32_t)packetbuf_attr(PACKETBUF_ATTR_CSYNC_CONN_AES_XORPLAINTEXT_B);

		for(i = 0; i < 16; i+=4)
		{
			memcpy(&block[i], (uint8_t *)&joined_xor, 4);
		}
		AES_128.encrypt(block);

		PRINTF("\nblock ");
		for(i = 0; i < 16; i++) {
		  PRINTF("%02x ", block[i]);
		}

		do_encrypt_decrypt = 1;
	}
	else
	{
		do_encrypt_decrypt = 0;
	}

}
/*---------------------------------------------------------------------------*/
static void
send(mac_callback_t sent, void *ptr)
{
	if(do_encrypt_decrypt)
	{
		uint8_t i;
		uint8_t* data = (uint8_t*)packetbuf_hdrptr(); 

		PRINTF("\nsend_xor AES %u", packetbuf_attr(PACKETBUF_ATTR_CSYNC_CONN_DOAES));
		// for(i = 0; i < packetbuf_totlen() - sizeof(timesync_frame_t); i++) {
		//   PRINTF("%02x ", data[i]);
		// }

		for(i = 0; i < packetbuf_totlen() - sizeof(timesync_frame_t); i++) {
			data[i] = data[i] ^ block[i];
		}
	}

  	NETSTACK_MAC.send(sent, ptr);
}
/*---------------------------------------------------------------------------*/
static void
input(void)
{

	if(do_encrypt_decrypt)
	{
		uint8_t i;
		uint8_t* data = (uint8_t*)packetbuf_dataptr();
		PRINTF("\nrecv_xor AES %u", packetbuf_attr(PACKETBUF_ATTR_CSYNC_CONN_DOAES));
		// for(i = 0; i < packetbuf_datalen() - sizeof(timesync_frame_t); i++) {
		//   PRINTF("%02x ", data[i]);
		// }

		for(i = 0; i < packetbuf_datalen() - sizeof(timesync_frame_t); i++) {
			data[i] = data[i] ^ block[i];
		}

		// PRINTF("\nrecv ");
		// for(i = 0; i < packetbuf_datalen() - sizeof(timesync_frame_t); i++) {
		//   PRINTF("%02x ", data[i]);
	}


	NETSTACK_NETWORK.input();
}
/*---------------------------------------------------------------------------*/
const struct llsec_driver nullsec_driver = {
  "nullsec",
  init,
  send,
  input
};
/*---------------------------------------------------------------------------*/

/** @} */
