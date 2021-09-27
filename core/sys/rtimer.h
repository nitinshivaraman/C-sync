/*
 * Copyright (c) 2005, Swedish Institute of Computer Science
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
 *         Header file for the real-time timer module.
 * \author
 *         Adam Dunkels <adam@sics.se>
 *
 */

/** \addtogroup sys
 * @{ */

/**
 * \defgroup rt Real-time task scheduling
 *
 * The real-time module handles the scheduling and execution of
 * real-time tasks (with predictable execution times).
 *
 * @{
 */

#ifndef RTIMER_H_
#define RTIMER_H_

#include "contiki-conf.h"


struct rtimer;
/* prototype of a rtimer callback function */
typedef char (*rtimer_callback_t)(struct rtimer *rt);



#ifndef RTIMER_CLOCK_DIFF
typedef unsigned short rtimer_clock_t;
#define RTIMER_CLOCK_DIFF(a,b)     ((signed short)((a)-(b)))
#endif /* RTIMER_CLOCK_DIFF */
#define RTIMER_CLOCK_LT(a, b)      (RTIMER_CLOCK_DIFF((a),(b)) < 0)

#define RTIMER_NOW() rtimer_now()
#define RTIMER_COARSE_NOW() rtimer_coarse_now()
#define RTIMER_FINE_OFFSET() rtimer_fine_offset()
#define RTIMER_AVG_RATE() rtimer_avg_rate()

#define RTIMER_SCHEDULE_SAFETY_MARGIN 2000
//#define RTIMER_SCHEDULE_SAFETY_MARGIN 8192

#define RTIMER_COARSE_FINE_SHIFT (16 + RTIMER_AB_RESOLUTION_SHIFT)
#define RTIMER_FINE_MAX ((1UL << (16 + RTIMER_AB_RESOLUTION_SHIFT)) - 1)
#define RTIMER_FINE_OVERFLOW ((0xFFFFFFFF) ^ RTIMER_FINE_MAX)

#define RTIMER_OFFSET_MIN RTIMER_FINE_MAX * 0.5
#define RTIMER_OFFSET_MAX RTIMER_FINE_MAX * 1.5

#define RTIMER_SECOND RTIMER_CONF_SECOND
#define RTIMER_ARCH_SECOND RTIMER_CONF_SECOND // just for use in code basis, not used in modified files
#define RTIMER_HF_TO_MS(t)          ((t) / (int32_t)(RTIMER_SECOND / 1000))
#define RTIMER_LF_TO_MS(t)          ((t * 1000) / (RTIMER_LF_SECOND))

typedef enum {
  RTIMER_JUST_EXPIRED = 0,
  RTIMER_INACTIVE = 1,
  RTIMER_SCHEDULED = 2,
  RTIMER_SINGLEPASS = 3,
} rtimer_state_t;

typedef enum {
  RTIMER_0 = 0,
  RTIMER_1,
  NUM_OF_RTIMERS
} rtimer_id_t;

typedef enum {
  RTIMER_DATE = 0,
  RTIMER_INTERVAL_NOW = 1,
  RTIMER_INTERVAL_REF = 2,
} rtimer_scheduletype_t;

/**
 * @brief rtimer struct
 */
typedef struct rtimer {
  uint32_t time_coarse_lg;
  uint32_t time_fine_lg; 
  uint32_t time_coarse_hw;
  uint32_t time_fine_hw; 
  rtimer_clock_t ta;
  rtimer_clock_t tb;
  rtimer_callback_t func;
  rtimer_state_t state;   /* internal state of the rtimer */
} rtimer_t;

rtimer_t rt[NUM_OF_RTIMERS];     /* rtimer structs */

uint32_t rtimer_coarse_schedule_ref;
uint32_t rtimer_fine_schedule_ref;

typedef struct timesync_frame {
  uint32_t coarse_now;
  uint32_t fine_offset;
  double clock_rate;
  double avg_rate;
  rtimer_clock_t ta;
  rtimer_clock_t tb;
  rtimer_clock_t ta_compare;
  rtimer_clock_t tb_compare;
  rtimer_clock_t hw_mac_timestamp;
} timesync_frame_t;


/**
 * \brief      Initialize the real-time scheduler.
 *
 *             This function initializes the real-time scheduler and
 *             must be called at boot-up, before any other functions
 *             from the real-time scheduler is called.
 */
void rtimer_init(void);
void rtimer_lf_overflow(void);
void rtimer_lf_update(void);

uint16_t rtimer_now(void);
uint32_t rtimer_now_fine(void);
uint32_t rtimer_stamps(void);
uint32_t rtimer_stamps_to_now(rtimer_clock_t ta, rtimer_clock_t tb, rtimer_clock_t ta_compare, rtimer_clock_t tb_compare, double clock_rate);
uint32_t rtimer_coarse_now(void);

uint32_t rtimer_fine_offset(void);
double rtimer_avg_rate(void);
void rtimer_set_avg_rate(double rate);
void rtimer_adjust_coarse_count(int32_t diff);
void rtimer_adjust_fine_offset(int32_t diff);

void rtimer_update_offset(uint32_t time_coarse, uint32_t time_fine);
uint32_t rtimer_estimate_offset(uint32_t time_coarse, uint32_t time_fine);

void rtimer_hwdate_to_lgdate(uint32_t *time_coarse, uint32_t* time_fine, uint32_t offset);
void rtimer_lgdates_to_lg_interval(uint32_t* time_coarse, uint32_t* time_fine, uint32_t coarse_then, uint32_t fine_then);
uint32_t rtimer_lginterval_to_hwdate(uint32_t* time_coarse, uint32_t* time_fine, uint8_t use_offset_ref);
uint32_t rtimer_lgdate_to_hwdate(uint32_t* time_coarse, uint32_t* time_fine);
uint32_t rtimer_lg_now(void);

int8_t rtimer_compare(rtimer_id_t timer, uint32_t time_coarse_lg, uint32_t time_fine_lg);
uint8_t rtimer_schedule(rtimer_id_t timer, rtimer_scheduletype_t interval, uint32_t time_coarse, uint32_t time_fine, rtimer_callback_t func);

void rtimer_sync_send(timesync_frame_t* syncframe);

int32_t rtimer_diff(int32_t time_a, int32_t time_b);


/*---------------------------------------------------------------------------*/
#define RTIMER0_HF_CALLBACK(void) \
  if(((rt[RTIMER_0].ta <= TACCR0 - RTIMER_AB_UPDATE) || ((rt[RTIMER_0].ta <= TACCR0 - 2 * RTIMER_AB_UPDATE) && rt[RTIMER_0].tb > RTIMER_AB_UPDATE_RESOLUTION / 2)) && rt[RTIMER_0].state == RTIMER_SCHEDULED) \
  { \
    rt[RTIMER_0].state = RTIMER_JUST_EXPIRED; \
    P2DIR ^= 0x40; \
    TBCCTL4 &= ~CCIE; \
    rtimer_lf_update(); \
    rt[RTIMER_0].func(&rt[RTIMER_0]); \
    rt[RTIMER_0].state = RTIMER_INACTIVE; \
  }

/*---------------------------------------------------------------------------*/
#define RTIMER1_HF_CALLBACK(void) \
  if(((rt[RTIMER_1].ta <= TACCR0 - RTIMER_AB_UPDATE) || ((rt[RTIMER_1].ta <= TACCR0 - 2 * RTIMER_AB_UPDATE) && rt[RTIMER_1].tb > RTIMER_AB_UPDATE_RESOLUTION / 2)) && rt[RTIMER_1].state == RTIMER_SCHEDULED) \
  { \
    rt[RTIMER_1].state = RTIMER_JUST_EXPIRED; \
    TBCCTL5 &= ~CCIE; \
    rtimer_lf_update(); \
    rt[RTIMER_1].func(&rt[RTIMER_1]); \
    rt[RTIMER_1].state = RTIMER_INACTIVE; \
  }


#define RTIMER_LF_CALLBACK(void) \
  if(rt[RTIMER_0].state == RTIMER_SCHEDULED && (TACCR2 == rt[RTIMER_0].ta - RTIMER_AB_UPDATE) && rt[RTIMER_0].time_coarse_hw <= RTIMER_COARSE_NOW()) \
  { \
    TBCCR4 = TBCCR0 + (uint16_t)((rt[RTIMER_0].tb + RTIMER_AB_UPDATE_RESOLUTION) / clock_get_rate()); \
    TBCCTL4 |= CCIE; \
  } \
  if(rt[RTIMER_1].state == RTIMER_SCHEDULED && (TACCR2 == rt[RTIMER_0].ta - RTIMER_AB_UPDATE) && rt[RTIMER_1].time_coarse_hw <= RTIMER_COARSE_NOW()) \
  { \
    TBCCR5 = TBCCR0 + (uint16_t)((rt[RTIMER_1].tb + RTIMER_AB_UPDATE_RESOLUTION) / clock_get_rate()); \
    TBCCTL5 |= CCIE; \
  }
















/* Do the math in 32bits to save precision.
 * Round to nearest integer rather than truncate. */
#define US_TO_RTIMERTICKS(US)  ((US) >= 0 ?                        \
                               (((int32_t)(US) * (RTIMER_SECOND) + 500000) / 1000000L) :      \
                               ((int32_t)(US) * (RTIMER_SECOND) - 500000) / 1000000L)

#define RTIMERTICKS_TO_US(T)   ((T) >= 0 ?                     \
                               (((int32_t)(T) * 1000000L + ((RTIMER_SECOND) / 2)) / (RTIMER_SECOND)) : \
                               ((int32_t)(T) * 1000000L - ((RTIMER_SECOND) / 2)) / (RTIMER_SECOND))

/* A 64-bit version because the 32-bit one cannot handle T >= 4295 ticks.
   Intended only for positive values of T. */
#define RTIMERTICKS_TO_US_64(T)  ((int32_t)(((int64_t)(T) * 1000000 + ((RTIMER_SECOND) / 2)) / (RTIMER_SECOND)))

/**
 * \brief      Get the time that a task last was executed
 * \param task The task
 * \return     The time that a task last was executed
 *
 *             This function returns the time that the task was last
 *             executed. This typically is used to get a periodic
 *             execution of a task without clock drift.
 *
 * \hideinitializer
 */
#define RTIMER_TIME(task) ((task)->time)

/* RTIMER_GUARD_TIME is the minimum amount of rtimer ticks between
   the current time and the future time when a rtimer is scheduled.
   Necessary to avoid accidentally scheduling a rtimer in the past
   on platforms with fast rtimer ticks. Should be >= 2. */
#ifdef RTIMER_CONF_GUARD_TIME
#define RTIMER_GUARD_TIME RTIMER_CONF_GUARD_TIME
#else /* RTIMER_CONF_GUARD_TIME */
#define RTIMER_GUARD_TIME (RTIMER_SECOND >> 14)
#endif /* RTIMER_CONF_GUARD_TIME */



/** @} */
/** @} */

#endif /* RTIMER_H_ */