/*
 * Copyright (c) 2006, Swedish Institute of Computer Science.
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
 *         An example announcement back-end, based on the polite primitive
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

/**
 * \addtogroup rimepoliteannouncement
 * @{
 */

#include "contiki.h"
#include "sys/cc.h"
#include "lib/list.h"
#include "net/rime/rime.h"
#include "net/rime/announcement.h"
#include "net/rime/ipolite.h"

#if NETSIM
#include "ether.h"
#endif

#include <string.h>
#include <stdio.h>
#include <stddef.h>

#define DEBUG 1
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

struct announcement_data {
  uint16_t id;
  struct announcement_value a_value;
};

#define ANNOUNCEMENT_MSG_HEADERLEN 2
struct announcement_msg {
  uint16_t num;
  struct announcement_data data[];
};


static struct polite_announcement_state {
  struct ipolite_conn c;
  clock_time_t interval;
  annstate_t last_event;
  uint8_t send_dups;
  uint8_t max_send_dups;
} c;

/*---------------------------------------------------------------------------*/
static void
send_adv(clock_time_t interval)
{
  struct announcement_msg *adata;
  timesync_frame_t *syncframe;
  struct announcement *a;

  packetbuf_clear();
  adata = packetbuf_dataptr();
  adata->num = 0;
  for(a = announcement_list(); a != NULL; a = list_item_next(a)) {
    if(a->has_value)
    {
      adata->data[adata->num].id = a->id;
      adata->data[adata->num].a_value = a->a_value;
      adata->num++;
    }
  }

  uint16_t announcement_datalength = ANNOUNCEMENT_MSG_HEADERLEN +
          sizeof(struct announcement_data) * adata->num;
          
  syncframe = packetbuf_dataptr() + announcement_datalength;

  packetbuf_set_datalen(announcement_datalength + sizeof(timesync_frame_t));



  if(adata->num > 0) {

  // PRINTF("\n%u: sending neighbor advertisement with: instr %u, degree %u, date_coarse %lu, date_fine %lu",
  //  linkaddr_node_addr.u16, adata->data[0].a_value.instr, adata->data[0].a_value.degree, adata->data[0].a_value.date_coarse, adata->data[0].a_value.date_fine);

    ipolite_send(&c.c, interval, packetbuf_datalen(), syncframe);
  }
}
/*---------------------------------------------------------------------------*/
static void
adv_packet_received(struct ipolite_conn *ipolite, const linkaddr_t *from)
{
  struct announcement_msg adata;
  timesync_frame_t syncframe;
  struct announcement_data data;
  uint8_t *ptr;
  int i;

  ptr = packetbuf_dataptr();

  /* Copy number of announcements */
  memcpy(&adata, ptr, sizeof(struct announcement_msg));

  uint16_t announcement_datalength = ANNOUNCEMENT_MSG_HEADERLEN + 
        adata.num * sizeof(struct announcement_data);

  memcpy(&syncframe, ptr + announcement_datalength, sizeof(timesync_frame_t));
  
  // PRINTF("\n%u: pa_received from %u with: instr %u, degree %u, date_coarse %lu, date_fine %lu",
	 // linkaddr_node_addr.u16, from->u16, adata.data[0].a_value.instr, adata.data[0].a_value.degree, (uint32_t)adata.data[0].a_value.date_coarse, adata.data[0].a_value.date_fine);

  if(announcement_datalength + sizeof(timesync_frame_t) > packetbuf_datalen()) {
    /* The number of announcements is too large - corrupt packet has
       been received. */
    //PRINTF("\nadata.num way out there: %d", adata.num);
    return;
  }

  ptr += ANNOUNCEMENT_MSG_HEADERLEN;
  for(i = 0; i < adata.num; ++i) {
    /* Copy announcements */
    memcpy(&data, ptr, sizeof(struct announcement_data));
    announcement_heard(from, data.id, &(data.a_value), &syncframe, c.last_event);

    ptr += sizeof(struct announcement_data);
  }
}

/*---------------------------------------------------------------------------*/
static void
adv_packet_dropped(struct ipolite_conn *ipolite)
{
  c.last_event = DROPPED;
}

/*---------------------------------------------------------------------------*/
static void
adv_packet_sent(struct ipolite_conn *ipolite)
{
  c.send_dups++;
  if(c.send_dups < c.max_send_dups)
  {
    send_adv(c.interval + POLITE_INTERVAL * c.send_dups);
  }
  else
  {
    c.last_event = SENT;
  }
}

/*---------------------------------------------------------------------------*/
static void
observer_pa(struct announcement *a)
{
  c.send_dups = 0;
  send_adv(c.interval);
}
/*---------------------------------------------------------------------------*/
static const struct ipolite_callbacks ipolite_callbacks =
  {adv_packet_received, adv_packet_sent, adv_packet_dropped};
/*---------------------------------------------------------------------------*/
void
polite_announcement_init(uint16_t channel, clock_time_t interval, uint8_t max_send_dups, uint8_t max_recv_dups)
{
  ipolite_open(&c.c, channel, max_recv_dups, &ipolite_callbacks);
  c.last_event = PENDING;
  c.interval = interval;
  c.send_dups = 0;
  c.max_send_dups = max_send_dups;

  announcement_register_observer_callback(observer_pa);
}
/*---------------------------------------------------------------------------*/
void
polite_announcement_stop(void)
{
  ipolite_cancel(&c.c);
  ipolite_close(&c.c);
}

/*---------------------------------------------------------------------------*/
void
polite_announcement_cancel(void)
{
  c.last_event = PENDING;
  c.send_dups = 0xFF;
  ipolite_cancel(&c.c);
}

/*---------------------------------------------------------------------------*/
void 
polite_announcement_set_interval(clock_time_t interval)
{
  c.interval = interval;
}

/*---------------------------------------------------------------------------*/
void
polite_announcement_set_max_send_dups(uint8_t max_send_dups)
{
  c.max_send_dups = max_send_dups;
}

/*---------------------------------------------------------------------------*/
void 
polite_announcement_set_max_recv_dups(uint8_t max_recv_dups)
{
  ipolite_set_max_dups(&c.c, max_recv_dups);
}

/*---------------------------------------------------------------------------*/
/** @} */
