/*
 * Copyright (c) 2008, Swedish Institute of Computer Science.
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
 *         Implementation of the announcement primitive
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

/**
 * \addtogroup rimeannouncement
 * @{
 */

#include "net/rime/announcement.h"
#include "lib/list.h"
#include "sys/cc.h"

LIST(announcements);

static announcement_observer observer_callback;

/*---------------------------------------------------------------------------*/
void
announcement_init(void)
{
  list_init(announcements);
}
/*---------------------------------------------------------------------------*/
void
announcement_register(struct announcement *a, uint16_t id,
		      announcement_callback_t callback)
{
  a->id = id;
  a->has_value = 0;
  
  announcement_set_degree(a, 0);
  announcement_set_instr(a, 0);
  announcement_set_date_coarse(a, 0);
  announcement_set_date_fine(a, 0);
  announcement_set_ref_addr(a, 0);
  announcement_set_cons_rate(a, 1);

  a->callback = callback;

  list_add(announcements, a);
}
/*---------------------------------------------------------------------------*/
void
announcement_remove(struct announcement *a)
{
  list_remove(announcements, a);
}

/*---------------------------------------------------------------------------*/
void
announcement_add_value(struct announcement *a)
{
  a->has_value = 1;
}
/*---------------------------------------------------------------------------*/
void
announcement_remove_value(struct announcement *a)
{
  a->has_value = 0;
}

/*---------------------------------------------------------------------------*/
void
announcement_bump(struct announcement *a)
{
  if(observer_callback) {
    observer_callback(a);
  }
}
/*---------------------------------------------------------------------------*/
void
announcement_register_observer_callback(announcement_observer callback)
{
  observer_callback = callback;
}

/*---------------------------------------------------------------------------*/
void
announcement_heard(const linkaddr_t *from, uint16_t id, struct announcement_value *a_value, timesync_frame_t *syncframe, annstate_t last_event)
{
  struct announcement *a;
  for(a = list_head(announcements); a != NULL; a = list_item_next(a)) {
    if(a->id == id) {
      if(a->callback != NULL) {
        a->callback(a, from, id, a_value, syncframe, last_event);
      }
      return;
    }
  }
}

/*---------------------------------------------------------------------------*/
struct announcement *
announcement_list(void)
{
  return list_head(announcements);
}



/*---------------------------------------------------------------------------*/
void
announcement_set_instr(struct announcement *a, uint8_t instr)
{
  a->a_value.instr = instr;
}
/*---------------------------------------------------------------------------*/
uint8_t 
announcement_get_instr(struct announcement *a)
{
  return a->a_value.instr;
}
/*---------------------------------------------------------------------------*/
void
announcement_set_degree(struct announcement *a, uint8_t degree)
{
  a->a_value.degree = degree;
}
/*---------------------------------------------------------------------------*/
void 
announcement_set_date_coarse(struct announcement *a, uint32_t date_coarse)
{
  a->a_value.date_coarse = date_coarse;
}
/*---------------------------------------------------------------------------*/
uint32_t 
announcement_get_date_coarse(struct announcement *a)
{
  return a->a_value.date_coarse;
}

/*---------------------------------------------------------------------------*/
void announcement_set_date_fine(struct announcement *a, uint32_t date_fine)
{
  a->a_value.date_fine = date_fine;
}
/*---------------------------------------------------------------------------*/
uint32_t 
announcement_get_date_fine(struct announcement *a)
{
  return a->a_value.date_fine;
}
/*---------------------------------------------------------------------------*/
void announcement_set_ref_addr(struct announcement *a, uint16_t ref_addr)
{
  a->a_value.ref_addr = ref_addr;
}
/*---------------------------------------------------------------------------*/
uint16_t 
announcement_get_ref_addr(struct announcement *a)
{
  return a->a_value.ref_addr;
}

/*---------------------------------------------------------------------------*/
void 
announcement_set_cons_rate(struct announcement *a, double cons_rate)
{
  a->a_value.cons_rate = cons_rate;
}
/*---------------------------------------------------------------------------*/
double 
announcement_get_cons_rate(struct announcement *a)
{
  return a->a_value.cons_rate;
}
/*---------------------------------------------------------------------------*/
/** @} */
