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
 */

#include "contiki.h"
#include "sys/energest.h"
#include "sys/clock.h"
#include "sys/etimer.h"
#include "dev/watchdog.h"
#include "isr_compat.h"
#include "dev/leds.h"
#include "net/c-sync/c-sync.h"

#include <stdio.h>

/* Default functions and definitions for event timer handling */
#define ETIMER_RESOLUTION (RTIMER_SECOND / CLOCK_SECOND)
#define MAX_TICKS (~((clock_time_t)0) / 2)
#define CLOCK_LT(a, b) ((int16_t)((a)-(b)) < 0)

/*  */
static volatile unsigned long seconds;
static volatile clock_time_t count;

extern volatile uint8_t cc2420_sfd_counter;
extern volatile uint16_t cc2420_sfd_start_time;
extern volatile uint16_t cc2420_sfd_end_time;


#if (F_CPU / RTIMER_HF_SECOND == 1)
#define TB_DIV_REG ID_0
#define TB_DIV 1
#elif (F_CPU / RTIMER_HF_SECOND == 2)
#define TB_DIV_REG ID_1
#define TB_DIV 2
#elif (F_CPU / RTIMER_HF_SECOND == 4)
#define TB_DIV_REG ID_2
#define TB_DIV 4
#elif (F_CPU / RTIMER_HF_SECOND == 8)
#define TB_DIV_REG ID_3
#define TB_DIV 8
#else
#error F_CPU and RTIMER_HF_SECOND are not matching
#endif

#define LFXT1CLK 32768u   /* LF crystal oscillator @ 32768Hz */
#define ACLK_DIV_REG DIVA_3 /* Max division for 512 Hz hardware clock (LFXT1CLK/64) */
#define ACLK_DIV 8
#define TA_DIV_REG ID_3

/* Number of SMLCK ticks @TIMER_B (after divider) per ACLK cycle, used for DCO calibration */
#define DELTA (MSP430_CPU_SPEED / TB_DIV) / (LFXT1CLK / ACLK_DIV)

void msp430_sync_dco(void);
static inline uint16_t read_tbr(void);

double last_rate;
uint16_t last_tbcrr0;




void
clock_init(void)
{
  
  BCSCTL1 = XT2OFF | ACLK_DIV_REG;
  BCSCTL2 = 0x00; // BCSCTL2 = DCOR; WHATS UP WITH THE EXTERNAL RESTISTOR ON TMOTE SKY? 

  BCSCTL1 |= DIVA1 + DIVA0;             /* ACLK = LFXT1CLK/8 */  
  uint16_t i;                     /* Wait for clocks do settle */
  for(i = 0xffff; i > 0; i--) {         /* Delay for XTAL to settle */
    asm("nop");
  }
  
  TBCTL = TBCLR;
  TBCTL = TBSSEL_2 | TB_DIV_REG | TBIE;        /* SMCLK, continous mode */
  TBCTL |= MC_2;
  msp430_sync_dco();

  /* TA1 in compare mode */
  TBCCTL2 = CCIE;                 /* Enable TACCR1_CCIFG interrupt */
  TBCCR2 = ETIMER_RESOLUTION;   

  /* SFD timestamping on TB1 */
  CC2420_SFD_PORT(SEL) = BV(CC2420_SFD_PIN);
  TBCCTL1 = CM_3 | CAP;
  TBCCTL1 |= CCIE;

  /* Link TIMER_A and TIMER_B */
  TBCCTL0 = CM_3 | CCIS_2 | CAP;
  TACCTL0 = CCIE;
  TACCR0 = RTIMER_AB_UPDATE;

  /* Select ACLK, divider = 8, enable TAIFG interrupt */
  TACTL = TACLR;
  TACTL = TASSEL_1 | TA_DIV_REG | TAIE;  
  TACTL |= MC_2;

  clock_state = CLOCK_FREE;
  clock_priority = CLOCK_FREE;
  last_rate = 1;
  last_tbcrr0 = 0;
  seconds = 0;
  count = 0;

  P2DIR |= 0x40;

  rtimer_init();
}

/*---------------------------------------------------------------------------*/
double 
clock_get_rate(void)
{
  double new_rate;
  do {
    new_rate = ((double)RTIMER_AB_UPDATE_RESOLUTION) / ((double)(TBCCR0 - last_tbcrr0));
  } while(0.7 > new_rate || new_rate > 1.3);

  return new_rate;
}
/*---------------------------------------------------------------------------*/
uint16_t 
clock_get_last_tbccr0(void)
{
  return last_tbcrr0;
}

/*---------------------------------------------------------------------------*/
ISR(TIMERA0, timera0)
{
  ENERGEST_ON(ENERGEST_TYPE_IRQ);
  
  last_tbcrr0 = TBCCR0;
  TBCCTL0 ^= CCIS0;
  TACCR0 += RTIMER_AB_UPDATE;
  TACCTL0 &= ~CCIFG;

  ENERGEST_OFF(ENERGEST_TYPE_IRQ);
}
/*---------------------------------------------------------------------------*/
ISR(TIMERA1, timera1)
{
  ENERGEST_ON(ENERGEST_TYPE_IRQ);

  switch(TAIV)
  {
    case 4:
      RTIMER_LF_CALLBACK();
    break;

    case 10:
      rtimer_lf_overflow();
    break;

    default:
    break;
  }

  ENERGEST_OFF(ENERGEST_TYPE_IRQ);
}

/*---------------------------------------------------------------------------*/

ISR(TIMERB1, timerb1)
{
  ENERGEST_ON(ENERGEST_TYPE_IRQ);

  switch(TBIV)
  {
    case 2: /* SFD interrupt for timestamping radio packets */
      if(CC2420_SFD_IS_1) {
        cc2420_sfd_counter++;
        cc2420_sfd_start_time = TBCCR1;
      } else {
        cc2420_sfd_counter = 0;
        cc2420_sfd_end_time = TBCCR1;
      }
    break;
    case 4: /* TBCCR2 etimer interrupt handler */
      /* Make sure interrupt time is future */
      while(!CLOCK_LT(read_tbr(), TBCCR2)) {
        TBCCR2 += ETIMER_RESOLUTION;
        ++count;
        /* Make sure the CLOCK_CONF_SECOND is a power of two, to ensure
           that the modulo operation below becomes a logical and and not
           an expensive divide. Algorithm from Wikipedia:
           http://en.wikipedia.org/wiki/Power_of_two */
        #if (CLOCK_CONF_SECOND & (CLOCK_CONF_SECOND - 1)) != 0
        #error CLOCK_CONF_SECOND must be a power of two (i.e., 1, 2, 4, 8, 16, 32, 64, ...).
        #error Change CLOCK_CONF_SECOND in contiki-conf.h.
        #endif
        if(count % CLOCK_CONF_SECOND == 0) {
          ++seconds;
          energest_flush();
        }
      }
      if(etimer_pending() &&
         (etimer_next_expiration_time() - count - 1) > MAX_TICKS) {
        etimer_request_poll();
        LPM4_EXIT;
      }
    break;

    case 8:
      RTIMER0_HF_CALLBACK();
    break;

    case 10:
      RTIMER1_HF_CALLBACK();
    break;
  }

  ENERGEST_OFF(ENERGEST_TYPE_IRQ);
}



/*---------------------------------------------------------------------------*/


static inline uint16_t
read_tbr(void)
{
  TBCCTL3 ^= CCIS0;
  return TBCCR3;
}

void
clock_delay(unsigned int i)
{
  while(i--) {
    _NOP();
  }
}

clock_time_t
clock_time(void)
{
  clock_time_t t1, t2;
  do {
    t1 = count;
    t2 = count;
  } while(t1 != t2);
  return t1;
}

unsigned long
clock_seconds(void)
{
  unsigned long t1, t2;
  do {
    t1 = seconds;
    t2 = seconds;
  } while(t1 != t2);
  return t1;
}

void
msp430_sync_dco(void) {

  unsigned int compare, oldcapture = 0;
  TBCCTL6 = CCIS_1 | CM_1 | CAP;      /* Define CCR2, CAP, ACLK */
	
  while(1) {
    while((TBCCTL6 & CCIFG) != CCIFG);    /* Wait until capture occured! */

    TBCCTL6 &= ~CCIFG;                    /* Capture occured, clear flag */
    compare = TBCCR6;                     /* Get current captured SMCLK */
    compare = compare - oldcapture;     /* SMCLK difference */
    oldcapture = TBCCR6;                  /* Save current captured SMCLK */

    if(DELTA == compare) {
      break;                            /* if equal, leave "while(1)" */
    } else if(DELTA < compare) {        /* DCO is too fast, slow it down */
      DCOCTL--;
      if(DCOCTL == 0xFF) {              /* Did DCO role under? */
        BCSCTL1--;
      }
    } else {                            /* -> Select next lower RSEL */
      DCOCTL++;
      if(DCOCTL == 0x00) {              /* Did DCO role over? */
        BCSCTL1++;
      }
                                        /* -> Select next higher RSEL  */
    }
  }
  TBCCTL6 = 0;                            /* Stop CCR2 function */
}