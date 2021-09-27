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
 *        Resilient Clustering in Contiki
 * \author
 *         Nitin Shivaraman <nitin.shivaraman@tum-create.edu.sg>
 */
#include "contiki-conf.h"

#include "net/rime/rime.h"
#include "dev/cc2420/cc2420.h"
#include "netstack.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "dev/leds.h"
#include "apps/powertrace/powertrace.h"
#include "sys/energest.h"

#include <stdio.h>
#include <stdlib.h>


    // Signal strength for neighbours

#define MAX_DEGREE                 64     // Maximum number of neighbour nodes for each node
#define LOGICAL_CHANNEL 11


/**
 *  Enum for state transitions
 */
typedef enum
{
  DISCOVERY = 0,
  ELECTION_REVELATION = 2,
  ELECTION_DECLARATION = 4,
  CONNECTION_REVELATION = 6,
  CONNECTION_DECLARATION = 8,
  CONSENSUS_CONVERGENCE = 10,
  CONSENSUS_REVELATION = 12,
  CONSENSUS_SYNCHRONIZATION = 14,
  IDLE = 16,
  BYZANTINE_CONSENSUS = 18
} state_t;

typedef enum
{
  DISC_TO_EREV = 1,
  CONVERGENCE_PROACTIVE = 11
} instr_t;

typedef enum
{
  CM = 0, // COMMONER
  CH = 1, // CLUSTER HEAD
  CB = 2,  // CLUSTER BRIDGE
} role_t;

/**
 *  Neighbour array
 */
typedef struct neighbour {
  struct    neighbour *next;     /// << The ->next pointer
  uint16_t  addr;                  /// << The ID of the neighbour 
  uint8_t   degree;
  role_t    role;
  state_t   state;

  uint8_t   jumped;
  uint8_t   synced;

  double    relative_rate;
  uint32_t  last_n_coarse;
  uint32_t  last_lg_n_fine;
  uint32_t  last_my_coarse;
  uint32_t  last_hw_my_fine;
  int32_t   coarse_diff;
  int32_t   fine_diff;
} neighbour_t;

typedef struct cluster {
  role_t role;
  list_t *CHs_list;
  list_t *CBs_list;
  list_t *blacklist;
  struct memb *m_CH;
  struct memb *m_CB;
  struct memb *m_bl;
} cluster_t;

typedef struct CHB {
  struct chb_t *next;
  uint16_t addr;
  uint8_t degree;
  uint8_t cons_slot;
  uint16_t n_CHB_addr_A;
  uint16_t n_CHB_addr_B;
} chb_t;

typedef struct BL {
  struct bl_t *next;
  uint16_t addr;
} bl_t;

uint16_t my_addr;
uint8_t  my_degree;
state_t  my_state;
uint8_t  my_placing;
struct cluster my_cluster;

uint8_t my_proactive_slot;
uint8_t my_cons_slot;
uint8_t this_sync_slot;
uint8_t my_slot_ack;
uint8_t my_sync_border;
uint8_t cons_ctrl_counter;

uint8_t ref_n_CHB_degree;
uint16_t ref_n_CHB_addr;
uint8_t synced_counter;

// Used for byzantine consensus
uint8_t msg_count;

void csync_print_status(void);
struct neighbour* add_to_neighbour(uint16_t addr, uint8_t state, uint8_t degree, timesync_frame_t *syncframe);
void reset_c_gtsp(void);
void soft_reset(void);
void csync_update_placing(void);
uint8_t csync_CHB_placing(void);
void csync_trusted_synchronization(neighbour_t *n, uint16_t c_addr, double cons_rate);
uint8_t csync_all_synced(void);
uint8_t check_mod_neighbours(uint16_t n_id);

uint8_t handle_lists(struct announcement *a, struct announcement_value *a_value, struct neighbour *n);
void init_consensus_convergence(void);
void init_consensus_synchronization(void);
void update_consensus_synchronization(void);

void update_neighbour_role(uint16_t addr, role_t role_sender);

PROCESS_NAME(c_gtsp_process);

struct announcement discovery_announcement;
struct announcement revelation_announcement;
struct announcement declaration_announcement;
struct announcement convergence_announcement;
struct announcement synchronization_announcement;

void received_discovery_announcement(struct announcement *a, const linkaddr_t *from,
           uint16_t id, struct announcement_value *a_value, timesync_frame_t *syncframe, annstate_t last_event);
void received_revelation_announcement(struct announcement *a, const linkaddr_t *from,
            uint16_t id, struct announcement_value *a_value, timesync_frame_t *syncframe, annstate_t last_event);
void received_declaration_announcement(struct announcement *a, const linkaddr_t *from,
            uint16_t id, struct announcement_value *a_value, timesync_frame_t *syncframe, annstate_t last_event);
void received_convergence_announcement(struct announcement *a, const linkaddr_t *from,
            uint16_t id, struct announcement_value *a_value, timesync_frame_t *syncframe, annstate_t last_event);
void received_synchronization_announcement(struct announcement *a, const linkaddr_t *from,
            uint16_t id, struct announcement_value *a_value, timesync_frame_t *syncframe, annstate_t last_event);

void gtsp_recv(neighbour_t *n, timesync_frame_t *syncframe, uint8_t new_neighbour);
void gtsp_update_rtimer(list_t neighbour_list);

inline char enter_election_revelation(rtimer_t *rt);
inline char enter_election_declaration(rtimer_t *rt);
inline char enter_connection_revelation(rtimer_t *rt);
inline char enter_connection_declaration(rtimer_t *rt);
inline char enter_convergence(rtimer_t *rt);
inline char enter_consensus_revelation(rtimer_t *rt);
inline char enter_consensus_synchronization(rtimer_t *rt);
inline char enter_byzantine_consensus(rtimer_t *rt);
inline char enter_idle(rtimer_t *rt);
inline char enter_discovery(rtimer_t *rt);

