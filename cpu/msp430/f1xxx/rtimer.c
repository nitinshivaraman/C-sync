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
 *         MSP430-specific rtimer code
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

#include "contiki.h"
#include "sys/energest.h"
#include "net/rime/rime.h"
#include <string.h>
#include "net/c-sync/c-sync.h"

#define DEBUG 1
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

static uint32_t coarse_count;
static double avg_rate;

// offsets @ reference coarse_offset_ref and fine_offset_ref
static uint32_t coarse_offset_ref;
static uint32_t fine_offset_ref;
static uint32_t fine_offset;

static uint32_t scheduler_fine_offset_ref;

typedef enum {
  SCHEDULER_FREE = 0,
  SCHEDULER_OCCUPIED = 1,
} scheduler_state_t;

static scheduler_state_t scheduler_state;


/*---------------------------------------------------------------------------*/
void
rtimer_init(void)
{
  coarse_count = 0;
  avg_rate = 0;

  coarse_offset_ref = 0;
  fine_offset_ref = 0;

  scheduler_state = SCHEDULER_FREE;

  rtimer_coarse_schedule_ref = 0;
  rtimer_fine_schedule_ref = 0;


  fine_offset = RTIMER_FINE_MAX;
  scheduler_fine_offset_ref = fine_offset;

  /* Realtime Capture timers */
  TBCCTL3 = CM_3 | CCIS_2 | CAP;  // Timer B capture for stamp requests
  TACCTL1 = CM_3 | CCIS_2 | CAP;  // Timer A capture for stamp requests

  memset(rt, 0, sizeof(rt));
}

/*---------------------------------------------------------------------------*/
void
rtimer_lf_overflow(void)
{
  while(scheduler_state);
  coarse_count++;
  rtimer_lf_update();
}

/*---------------------------------------------------------------------------*/
void
rtimer_lf_update(void)
{
  uint32_t time_coarse = 0xFFFFFFFF;
  uint32_t time_fine = 0xFFFFFFFF;
  uint8_t schedule_ta = 0;
  rtimer_id_t timer;
  rtimer_id_t timer_use = NUM_OF_RTIMERS;
  
  for(timer = 0; timer < NUM_OF_RTIMERS; timer++)
  {
    if(rt[timer].state == RTIMER_SCHEDULED)
    {
      if(rt[timer].time_coarse_hw <= time_coarse)
      {
        time_coarse = rt[timer].time_coarse_hw;
        if(rt[timer].time_fine_hw <= time_fine)
        {
          time_fine = rt[timer].time_fine_hw;
          timer_use = timer;
          schedule_ta = 1;
        }
      }
    }
  }

  if(schedule_ta)
  {
    //TBCCTL3 ^= CCIS0;
    //PRINTF("TBCCR0 %u, TBCCR3 %u\n", TBCCR0, TBCCR3);

    TACCTL2 |= CCIE;
    TACCR2 = rt[timer_use].ta - RTIMER_AB_UPDATE;
    TACCTL1 ^= CCIS0;

    if((rt[timer_use].ta < TACCR0 + RTIMER_AB_UPDATE) || TACCR1 == TACCR2)
    {
      TACCTL2 &= ~CCIE;
      //PRINTF("TACCR0 %u,TACCR1 %u, TACCR2 %u\n", TACCR0, TACCR1, TACCR2);
      RTIMER_LF_CALLBACK();
      
    }
  }
  else
  {
    TACCTL2 &= ~CCIE;
    //PRINTF("No rtimers active, now %lu\n", RTIMER_HF_TO_MS(RTIMER_NOW()));
  }

  
}

/*---------------------------------------------------------------------------*/
uint16_t
rtimer_now(void)
{  
  TACCTL1 ^= CCIS0;
  return TACCR1;
}

uint32_t
rtimer_now_fine(void)
{
  rtimer_clock_t ta, tb, ta_compare, tb_compare;
  double my_clock_rate;
  uint32_t now_my_fine;

  clock_priority = CLOCK_OCCUPIED;
  while(clock_state);

  clock_state = CLOCK_OCCUPIED; 

  do
  {
    my_clock_rate = clock_get_rate();

    ta_compare = TACCR0 - RTIMER_AB_UPDATE;
    tb_compare = TBCCR0;
    TBCCTL3 ^= CCIS0;
    TACCTL1 ^= CCIS0;

    ta = TACCR1;
    tb = TBCCR3;

    clock_state = CLOCK_FREE;

    now_my_fine = rtimer_stamps_to_now(ta, tb, ta_compare, tb_compare, my_clock_rate);
  } while(now_my_fine == 0);

  clock_priority = CLOCK_FREE;

  return now_my_fine;
}


/*---------------------------------------------------------------------------*/
uint32_t
rtimer_stamps_to_now(rtimer_clock_t ta, rtimer_clock_t tb, rtimer_clock_t ta_compare, rtimer_clock_t tb_compare, double clock_rate)
{

  uint32_t t_out = (uint32_t)ta << RTIMER_AB_RESOLUTION_SHIFT;

  //PRINTF("tb %u, tb_compare %u\n", tb, tb_compare);

  while(tb_compare > tb)
  {
    tb += RTIMER_AB_RESOLUTION;
    tb_compare += RTIMER_AB_RESOLUTION;
  }
  

  tb = tb - tb_compare; 

  //PRINTF("tb %u\n", tb);

  tb = (uint16_t)(tb * clock_rate);

  //PRINTF("tb %u\n", tb);
  //PRINTF("ta %u, ta_compare %u\n", ta, ta_compare);

  while(ta_compare < ta)
  {
    ta_compare++;
    tb -= (uint16_t)(RTIMER_AB_RESOLUTION); 
  }
  

  if(tb > 65535 - RTIMER_AB_RESOLUTION)
  {
    tb = 65535 - tb;
    // PRINTF("tb %u\n", tb);
    //PRINTF("ta %u, tb %u\n", ta, tb);
    return t_out - tb;
  }

  if(tb >= RTIMER_AB_RESOLUTION)
  {
    //PRINTF("\ntb %u", tb);
    return 0;
  }

  //PRINTF("ta %u, tb %u\n", ta, tb);
  return t_out + tb;
}

/*---------------------------------------------------------------------------*/
uint32_t
rtimer_coarse_now(void)
{
  return coarse_count;
}

/*---------------------------------------------------------------------------*/
uint32_t
rtimer_fine_offset(void)
{
  return fine_offset;
}
/*---------------------------------------------------------------------------*/
double
rtimer_avg_rate(void)
{
  return avg_rate;
}
/*---------------------------------------------------------------------------*/
void
rtimer_set_avg_rate(double rate)
{
  avg_rate = rate;
}

/*---------------------------------------------------------------------------*/
void 
rtimer_adjust_coarse_count(int32_t diff)
{
  while(scheduler_state);
  coarse_count -= diff;
}

/*---------------------------------------------------------------------------*/
void 
rtimer_adjust_fine_offset(int32_t diff)
{
  while(scheduler_state);
  fine_offset -= diff;
  if(fine_offset < RTIMER_OFFSET_MIN)
  {
    coarse_count--;
    fine_offset += RTIMER_FINE_MAX;
  }
  else if(fine_offset > RTIMER_OFFSET_MAX)
  {
    coarse_count++;
    fine_offset -= RTIMER_FINE_MAX;
  }

  diff = rtimer_diff(scheduler_fine_offset_ref, fine_offset);

  if(diff < -200 || diff > 200)
  {
    if(rt[RTIMER_0].state == RTIMER_SCHEDULED)
    {
      rtimer_schedule(RTIMER_0, RTIMER_DATE, rt[RTIMER_0].time_coarse_lg, rt[RTIMER_0].time_fine_lg, rt[RTIMER_0].func);
    }
    if(rt[RTIMER_1].state == RTIMER_SCHEDULED)
    {
      rtimer_schedule(RTIMER_1, RTIMER_DATE, rt[RTIMER_1].time_coarse_lg, rt[RTIMER_1].time_fine_lg, rt[RTIMER_1].func);
    }
  }
  scheduler_fine_offset_ref = fine_offset;

}


/*---------------------------------------------------------------------------*/
void
rtimer_update_offset(uint32_t time_coarse, uint32_t time_fine)
{
  uint32_t delta_coarse;
  uint32_t delta_fine;

  uint32_t interval;

  delta_coarse = time_coarse - coarse_offset_ref;

  if(fine_offset_ref < time_fine)
  {
    delta_fine = time_fine - fine_offset_ref;
  }
  else
  {
    delta_fine = time_fine + (RTIMER_FINE_MAX - fine_offset_ref);
    delta_coarse--;
  }

  interval = (delta_coarse << RTIMER_COARSE_FINE_SHIFT) + delta_fine;

  fine_offset += (uint32_t)(interval * avg_rate);

  if(fine_offset < RTIMER_OFFSET_MIN || fine_offset > RTIMER_OFFSET_MAX)
  {
     // if fine_offset grows too big or too small, adjust coarse_offset in next lf_overflow
    // set the flag for this here, OR BETTER, DO IT IMMEDIATELY AND UPDATE RTIMER-SCHEDULING
    // THIS IS TO DO
    // PRINTF(", fine_offset %lu", fine_offset);
    // PRINTF("ERROR IN rtimer_update_offset\n");
  }

  coarse_offset_ref = time_coarse;
  fine_offset_ref = time_fine;
}

/*---------------------------------------------------------------------------*/
uint32_t
rtimer_estimate_offset(uint32_t time_coarse, uint32_t time_fine)
{
  uint32_t delta_coarse;
  uint32_t delta_fine;

  uint32_t interval;

  delta_coarse = time_coarse - coarse_offset_ref;

  if(fine_offset_ref < time_fine)
  {
    delta_fine = time_fine - fine_offset_ref;
  }
  else
  {
    delta_fine = time_fine + (RTIMER_FINE_MAX - fine_offset_ref);
    delta_coarse--;
  }

  interval = (delta_coarse << RTIMER_COARSE_FINE_SHIFT) + delta_fine;

  return fine_offset + (uint32_t)interval * avg_rate;
}

/*---------------------------------------------------------------------------*/
void
rtimer_hwdate_to_lgdate(uint32_t *time_coarse, uint32_t* time_fine, uint32_t offset)
{
  if(offset > RTIMER_FINE_MAX)
  {
    (*time_coarse)++;
    offset -= RTIMER_FINE_MAX;
  }

  (*time_fine) += offset;

  if(RTIMER_FINE_MAX < *time_fine)
  {
    (*time_coarse)++;
    (*time_fine) -= RTIMER_FINE_MAX;
  }
}

/*---------------------------------------------------------------------------*/
void
rtimer_lgdates_to_lg_interval(uint32_t* time_coarse, uint32_t* time_fine, uint32_t coarse_then, uint32_t fine_then)
{
  uint32_t delta_coarse;
  uint32_t delta_fine;

  delta_coarse = (*time_coarse) - coarse_then;

  if(fine_then < *time_fine)
  {
    delta_fine = (*time_fine) - fine_then;
  }
  else
  {
    delta_fine = (*time_fine) + (RTIMER_FINE_MAX - fine_then);
    delta_coarse--;
  }

  (*time_coarse) = 0;
  (*time_fine) = (delta_coarse << RTIMER_COARSE_FINE_SHIFT) + delta_fine;
}

/*---------------------------------------------------------------------------*/
uint32_t
rtimer_lginterval_to_hwdate(uint32_t* time_coarse, uint32_t* time_fine, uint8_t interval)
{
  (*time_coarse) = 0;
  (*time_fine) = (uint32_t)((*time_fine) * (1 + avg_rate));

  if(RTIMER_FINE_MAX < *time_fine)
  {
    (*time_coarse) += (*time_fine) / RTIMER_FINE_MAX;
    (*time_fine) = (*time_fine) % RTIMER_FINE_MAX;
  }

  uint32_t coarse_then;
  uint32_t fine_then;
  
  rtimer_clock_t ta, tb, ta_compare, tb_compare;
  double my_clock_rate;

  if(interval == RTIMER_INTERVAL_REF)
  {
    coarse_then = rtimer_coarse_schedule_ref;
    fine_then = rtimer_fine_schedule_ref;
  }
  else
  {
    clock_priority = CLOCK_OCCUPIED;
    while(clock_state);

    clock_state = CLOCK_OCCUPIED; 

    do
    {
      my_clock_rate = clock_get_rate();

      ta_compare = TACCR0 - RTIMER_AB_UPDATE;
      tb_compare = TBCCR0;
      TBCCTL3 ^= CCIS0;
      TACCTL1 ^= CCIS0;

      ta = TACCR1;
      tb = TBCCR3;

      clock_state = CLOCK_FREE; 

      coarse_then = RTIMER_COARSE_NOW();
      fine_then = rtimer_stamps_to_now(ta, tb, ta_compare, tb_compare, my_clock_rate);
    } while(fine_then == 0);

    clock_priority = CLOCK_FREE;
  }

  (*time_fine) += fine_then;
  if(RTIMER_FINE_MAX < *time_fine)
  {
    (*time_coarse)++;
    (*time_fine) -= RTIMER_FINE_MAX;
  }
#if RTIMER_FINE_MAX > RTIMER_FINE_OVERFLOW
  else if(*time_fine < fine_then)
  {
    (*time_coarse)++;
    (*time_fine) += RTIMER_FINE_OVERFLOW;
  }
#endif /* RTIMER_FINE_MAX > RTIMER_FINE_OVERFLOW */

  (*time_coarse) += coarse_then;
  //PRINTF("now: %lu, fine: %lu\n", fine_then, (*time_fine));

  if(interval == RTIMER_INTERVAL_REF)
  {
    clock_priority = CLOCK_OCCUPIED;
    while(clock_state);

    clock_state = CLOCK_OCCUPIED; 

    do
    {
      my_clock_rate = clock_get_rate();

      ta_compare = TACCR0 - RTIMER_AB_UPDATE;
      tb_compare = TBCCR0;
      TBCCTL3 ^= CCIS0;
      TACCTL1 ^= CCIS0;

      ta = TACCR1;
      tb = TBCCR3;

      clock_state = CLOCK_FREE; 

      fine_then = rtimer_stamps_to_now(ta, tb, ta_compare, tb_compare, my_clock_rate);
    } while(fine_then == 0);

    clock_priority = CLOCK_FREE;
    return fine_then;
  }
  else
  {
    return fine_then;
  }
}

uint32_t
rtimer_lgdate_to_hwdate(uint32_t* time_coarse, uint32_t* time_fine)
{  
  uint32_t coarse_now = RTIMER_COARSE_NOW();
  uint32_t fine_now = rtimer_now_fine();
  rtimer_coarse_schedule_ref = coarse_now;
  rtimer_fine_schedule_ref = fine_now;

  rtimer_update_offset(coarse_now, fine_now);
  rtimer_hwdate_to_lgdate(&coarse_now, &fine_now, fine_offset);

  //PRINTF("tc %lu, tf %lu, cn %lu, fn %lu\n", *time_coarse, *time_fine, coarse_now, fine_now);

  rtimer_lgdates_to_lg_interval(time_coarse, time_fine, coarse_now, fine_now);

  //PRINTF("lginterval %lu\n", RTIMER_HF_TO_MS(*time_fine));

  return rtimer_lginterval_to_hwdate(time_coarse, time_fine, RTIMER_INTERVAL_REF);
}



uint32_t
rtimer_lg_now(void)
{
  uint32_t coarse_now = RTIMER_COARSE_NOW();
  uint32_t fine_now = rtimer_now_fine();
  rtimer_update_offset(coarse_now, fine_now);
  rtimer_hwdate_to_lgdate(&coarse_now, &fine_now, fine_offset);
  return (coarse_now << RTIMER_COARSE_FINE_SHIFT) + fine_now;
  //return fine_now;
}

/*---------------------------------------------------------------------------*/
int8_t 
rtimer_compare(rtimer_id_t timer, uint32_t time_coarse_lg, uint32_t time_fine_lg)
{
  if(rt[timer].state < RTIMER_SCHEDULED)
  {
    return 1;
  }
  else
  {
    if(rt[timer].time_coarse_lg == time_coarse_lg)
    {
      if(rt[timer].time_fine_lg > time_fine_lg)
      {
        return -1;
      }
      else if(rt[timer].time_fine_lg < time_fine_lg)
      {
        return 1;
      }
      else
      {
        return 0; // equality
      }
    }
    else
    {
      if(rt[timer].time_coarse_lg > time_coarse_lg)
      {
        return -1;
      }
      else
      {
        return 1;
      }
    }
  }
}

/*---------------------------------------------------------------------------*/
uint8_t
rtimer_schedule(rtimer_id_t timer,
                rtimer_scheduletype_t interval,
                uint32_t time_coarse,
                uint32_t time_fine,
                rtimer_callback_t func)
{
  if(timer < NUM_OF_RTIMERS)
  {
    if(time_coarse == 0 && time_fine == 0)
    {
      return 0;
    }
    else if(rt[timer].state == RTIMER_SINGLEPASS)
    {
      rt[timer].state = RTIMER_SCHEDULED;
      return 1;
    }

    rt[timer].func = func;
    rtimer_clock_t ta;
    rtimer_clock_t tb;

    uint32_t time_coarse_mod = time_coarse;
    uint32_t time_fine_mod = time_fine;

    uint32_t quick_ref;

    scheduler_state = SCHEDULER_OCCUPIED;

    if(interval == RTIMER_DATE)
    {
      //PRINTF("\ntime_coarse %lu, time_fine %lu", time_coarse, time_fine);

      //PRINTF("lgdate %lu\n", RTIMER_HF_TO_MS((time_coarse << RTIMER_COARSE_FINE_SHIFT) + time_fine));

      quick_ref = rtimer_lgdate_to_hwdate(&time_coarse_mod, &time_fine_mod);

      if(time_coarse_mod < RTIMER_COARSE_NOW() || (time_coarse_mod == RTIMER_COARSE_NOW() && time_fine_mod < quick_ref + RTIMER_SCHEDULE_SAFETY_MARGIN))
      {
        scheduler_state = SCHEDULER_FREE;
        return 0;
      }

      scheduler_state = SCHEDULER_FREE;

      rt[timer].time_coarse_lg = time_coarse;
      rt[timer].time_fine_lg = time_fine;
      rt[timer].time_coarse_hw = time_coarse_mod;
      rt[timer].time_fine_hw = time_fine_mod;

      ta = (rtimer_clock_t)((time_fine_mod >> RTIMER_AB_UPDATE_SHIFT) << (RTIMER_AB_UPDATE_SHIFT - RTIMER_AB_RESOLUTION_SHIFT));
      tb = ((rtimer_clock_t)time_fine_mod  << (16 - RTIMER_AB_UPDATE_SHIFT)) >> (16 - RTIMER_AB_UPDATE_SHIFT); //if tb is very small, maybe add a safety buffer
    }
    else
    {
      quick_ref = rtimer_lginterval_to_hwdate(&time_coarse_mod, &time_fine_mod, interval);
      // if(my_state == CONVERGENCE)
      // {
      //   PRINTF("\n %lu, %lu, %lu, %lu, %d", time_coarse_mod, RTIMER_COARSE_NOW(), time_fine_mod, quick_ref, RTIMER_SCHEDULE_SAFETY_MARGIN);
      // }
      if(time_coarse_mod < RTIMER_COARSE_NOW() || (time_coarse_mod == RTIMER_COARSE_NOW() && time_fine_mod < quick_ref + RTIMER_SCHEDULE_SAFETY_MARGIN))
      {
        scheduler_state = SCHEDULER_FREE;
        return 0;
      }

      scheduler_state = SCHEDULER_FREE;

      rt[timer].time_coarse_hw = time_coarse_mod;
      rt[timer].time_fine_hw = time_fine_mod;

      ta = (rtimer_clock_t)((time_fine_mod >> RTIMER_AB_UPDATE_SHIFT) << (RTIMER_AB_UPDATE_SHIFT - RTIMER_AB_RESOLUTION_SHIFT));
      tb = ((rtimer_clock_t)time_fine_mod  << (16 - RTIMER_AB_UPDATE_SHIFT)) >> (16 - RTIMER_AB_UPDATE_SHIFT); 

      uint32_t offset = rtimer_estimate_offset(time_coarse_mod, time_fine_mod);
      rtimer_hwdate_to_lgdate(&time_coarse_mod, &time_fine_mod, offset);
      rt[timer].time_coarse_lg = time_coarse_mod;
      rt[timer].time_fine_lg = time_fine_mod;

      //PRINTF("\ntime_coarse %lu, time_fine %lu", time_coarse, time_fine);


    }


    //PRINTF("schedule ta %u, tb %u\n", ta, tb);

    rt[timer].ta = ta;
    rt[timer].tb = tb;


    rt[timer].state = RTIMER_SCHEDULED;
    rtimer_lf_update();

    return 1;

  }
  else 
  {
    PRINTF("\ninvalid rtimer ID %u", timer);
    return 0;
  }
}


/*---------------------------------------------------------------------------*/
void
rtimer_sync_send(timesync_frame_t *syncframe)
{

  uint32_t now_my_fine;  

  do
  {
    while(clock_state || clock_priority);

    clock_state = CLOCK_OCCUPIED;
    syncframe->clock_rate = clock_get_rate();

    syncframe->ta_compare = TACCR0 - RTIMER_AB_UPDATE;
    syncframe->tb_compare = TBCCR0;
    TBCCTL3 ^= CCIS0;
    TACCTL1 ^= CCIS0;

    syncframe->ta = TACCR1;
    syncframe->tb = TBCCR3;

    clock_state = CLOCK_FREE;

    syncframe->coarse_now = RTIMER_COARSE_NOW();
    syncframe->avg_rate = RTIMER_AVG_RATE();

    now_my_fine = rtimer_stamps_to_now(syncframe->ta, syncframe->tb, syncframe->ta_compare, syncframe->tb_compare, syncframe->clock_rate);

    
  } while(now_my_fine == 0);

  rtimer_update_offset(syncframe->coarse_now, now_my_fine);
  syncframe->fine_offset = RTIMER_FINE_OFFSET();

  syncframe->hw_mac_timestamp = 0;
}

/*---------------------------------------------------------------------------*/
int32_t
rtimer_diff(int32_t time_a, int32_t time_b)
{
  return time_a - time_b;
}


