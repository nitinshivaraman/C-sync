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
 *         Header file for the announcement primitive
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

/**
 * \addtogroup rime
 * @{
 */

/**
 * \defgroup rimeannouncement Announcements
 * @{
 *
 * The Announcement primitive does local area announcements. An
 * announcement is an (ID, value) tuple that is disseminated to local
 * area neighbors. An application or protocol can explicitly listen to
 * announcements from neighbors. When an announcement is heard, a
 * callback is invoked.
 *
 * Announcements can be used for a variety of network mechanisms such
 * as neighbor discovery, node-level service discovery, or routing
 * metric dissemination.
 *
 * Application programs and protocols register announcements with the
 * announcement module. An announcement back-end, implemented by the
 * system, takes care of sending out announcements over the radio, as
 * well as collecting announcements heard from neighbors.
 *
 */

#ifndef ANNOUNCEMENT_H_
#define ANNOUNCEMENT_H_

#include "net/linkaddr.h"
#include "sys/rtimer.h"

struct announcement;
struct announcement_value;

typedef enum
{
  PENDING = 0,
  DROPPED = 1,
  SENT = 2,
} annstate_t;

typedef void (*announcement_callback_t)(struct announcement *a,
					const linkaddr_t *from,
					uint16_t id, struct announcement_value *a_val, timesync_frame_t *syncframe, annstate_t last_event);

struct announcement_value {
  uint8_t   instr;
  uint8_t   degree; // or cons_slot in CONS_CTRL phases
  uint32_t  date_coarse;
  uint32_t  date_fine;
  uint16_t  ref_addr;
  double    cons_rate;
};



/**
 * \brief      Representation of an announcement.
 *
 *             This structure holds the state of an announcement. It
 *             is an opaque structure with no user-visible elements.
 */
struct announcement {
  struct    announcement *next;
  uint16_t  id;
  uint8_t   has_value;
  struct    announcement_value a_value;
  announcement_callback_t callback;
};

void announcement_init(void);
void announcement_register(struct announcement *a,
			   uint16_t id,
			   announcement_callback_t callback);
void announcement_remove(struct announcement *a);

void announcement_add_value(struct announcement *a);
void announcement_remove_value(struct announcement *a);

typedef void (* announcement_observer)(struct announcement *a); // Maybe change input arguments, depending on usecases in the future
void announcement_register_observer_callback(announcement_observer observer);
void announcement_bump(struct announcement *a); // Filter of input parameters for announcement_observer. Currently just forwards *as

void announcement_heard(const linkaddr_t *from, uint16_t id, struct announcement_value *a_value, timesync_frame_t *syncframe, annstate_t last_event);

struct announcement *announcement_list(void);

void announcement_set_instr(struct announcement *a, uint8_t instr);
uint8_t announcement_get_instr(struct announcement *a);
void announcement_set_degree(struct announcement *a, uint8_t degree);
void announcement_set_date_coarse(struct announcement *a, uint32_t date_coarse);
uint32_t announcement_get_date_coarse(struct announcement *a);
void announcement_set_date_fine(struct announcement *a, uint32_t date_fine);
uint32_t announcement_get_date_fine(struct announcement *a);
void announcement_set_ref_addr(struct announcement *a, uint16_t ref_addr);
uint16_t announcement_get_ref_addr(struct announcement *a);
void announcement_set_cons_rate(struct announcement *a, double cons_rate);
double announcement_get_cons_rate(struct announcement *a);

#endif /* ANNOUNCE_H_ */

/** @} */
/** @} */
