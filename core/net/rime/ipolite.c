/*
 * Copyright (c) 2007, Swedish Institute of Computer Science.
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
 *         Ipolite Anonymous best effort local area BroadCast (ipolite)
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

/**
 * \addtogroup rimeipolite
 * @{
 */

#include "sys/cc.h"
#include "net/rime/rime.h"
#include "net/rime/ipolite.h"
#include "lib/random.h"

#include <string.h>

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

/*---------------------------------------------------------------------------*/
static void
recv(struct broadcast_conn *broadcast, const linkaddr_t *from)
{
  struct ipolite_conn *ipc_conn = (struct ipolite_conn *)broadcast;

  if(ipc_conn->q != NULL &&
     packetbuf_datalen() == queuebuf_datalen(ipc_conn->q) &&
     memcmp(packetbuf_dataptr(), queuebuf_dataptr(ipc_conn->q),
	    MIN(ipc_conn->hdrsize, packetbuf_datalen() - sizeof(timesync_frame_t))) == 0) {
    /* We received a copy of our own packet, so we increase the
       duplicate counter. If it reaches its maximum, do not send out
       our packet. */
    ipc_conn->dups++;
    if(ipc_conn->dups >= ipc_conn->maxdups) {
      queuebuf_free(ipc_conn->q);
      ipc_conn->q = NULL;
      ctimer_stop(&ipc_conn->t);
      PRINTF("\ndrop packet");
      if(ipc_conn->cb->dropped) {
        ipc_conn->cb->dropped(ipc_conn);
      }
    }
  }
  if(ipc_conn->cb->recv) {
    ipc_conn->cb->recv(ipc_conn, from);
  }
}
/*---------------------------------------------------------------------------*/
static void
sent(struct broadcast_conn *broadcast, int status, int num_tx)
{
  struct ipolite_conn *ipc_conn = (struct ipolite_conn *)broadcast;
  if(ipc_conn->cb->sent) {
    ipc_conn->cb->sent(ipc_conn);
  }
}
/*---------------------------------------------------------------------------*/
static void
send(void *ptr)
{
  struct ipolite_conn *c = ptr;


  
  if(c->q != NULL) {
    queuebuf_to_packetbuf(c->q);
    queuebuf_free(c->q);
    c->q = NULL;
    PRINTF("\n%d.%d: ipolite: send queuebuf %p",
     linkaddr_node_addr.u8[0],linkaddr_node_addr.u8[1],
     c->q);
    rtimer_sync_send(c->syncframe);
    packetbuf_set_attr(PACKETBUF_ATTR_PACKET_TYPE,
                   PACKETBUF_ATTR_PACKET_TYPE_TIMESTAMP);

    broadcast_send(&c->c);
  }
}
/*---------------------------------------------------------------------------*/
static const struct broadcast_callbacks broadcast = { recv, sent };
/*---------------------------------------------------------------------------*/
void
ipolite_open(struct ipolite_conn *c, uint16_t channel, uint8_t dups,
	  const struct ipolite_callbacks *cb)
{
  broadcast_open(&c->c, channel, &broadcast);
  c->cb = cb;
  c->maxdups = dups;
  PRINTF("\nipolite open channel %d", channel);
}
/*---------------------------------------------------------------------------*/
void
ipolite_close(struct ipolite_conn *c)
{
  broadcast_close(&c->c);
  ctimer_stop(&c->t);
  if(c->q != NULL) {
    queuebuf_free(c->q);
    c->q = NULL;
  }
}
/*---------------------------------------------------------------------------*/
int
ipolite_send(struct ipolite_conn *c, clock_time_t interval, uint8_t hdrsize, timesync_frame_t *syncframe)
{
  if(c->q != NULL) {
    /* If we are already about to send a packet, we cancel the old one. */
    PRINTF("\n%u: ipolite_send: cancel old send",
	   linkaddr_node_addr.u16);
    queuebuf_free(c->q);
    c->q = NULL;
    ctimer_stop(&c->t);
  }
  c->dups = 0;
  c->hdrsize = hdrsize;
  if(interval == 0) {
    PRINTF("\n%u: ipolite_send: interval 0",
	   linkaddr_node_addr.u16);

    rtimer_sync_send(syncframe);
    packetbuf_set_attr(PACKETBUF_ATTR_PACKET_TYPE,
                   PACKETBUF_ATTR_PACKET_TYPE_TIMESTAMP);

    if(broadcast_send(&c->c)) {
      return 1;
    }

  } else {
    c->q = queuebuf_new_from_packetbuf();
    if(c->q != NULL) {
      c->syncframe = syncframe;

      clock_time_t t = interval + (random_rand() % (interval));
      //printf("\nctimer_set %u", (unsigned)t);

      ctimer_set(&c->t,
		    t,
		    send, c);
      return 1;
    }
    PRINTF("\n%u: ipolite_send: could not allocate queue buffer\n",
	   linkaddr_node_addr.u16);
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
void
ipolite_cancel(struct ipolite_conn *c)
{
  ctimer_stop(&c->t);
  if(c->q != NULL) {
    queuebuf_free(c->q);
    c->q = NULL;
  }
}

/*---------------------------------------------------------------------------*/
void ipolite_set_max_dups(struct ipolite_conn *c, uint8_t max_dups)
{
  c->maxdups = max_dups;
}

/*---------------------------------------------------------------------------*/
/** @} */
