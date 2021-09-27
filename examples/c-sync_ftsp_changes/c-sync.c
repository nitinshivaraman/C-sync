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



#include "net/c-sync/c-sync.h"

#define DEBUG 1
#if DEBUG
#include <stdio.h>
#include <stdlib.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif


LIST(neighbour_list);
MEMB(neighbour_memb, struct neighbour, MAX_DEGREE);


#if MOD_NEIGHBOURS
LIST(mod_neighbour_list);
MEMB(mod_neighbour_memb, struct neighbour, 25); // we only have 15 skymotes
uint8_t check_mod_neighbours(uint16_t n_id);
#endif /*MOD_NEIGHBOURS*/

LIST(CHs_list);
MEMB(CHs_memb, struct CHB, NUM_CH_MAX);
LIST(CBs_list);
MEMB(CBs_memb, struct CHB, NUM_CB_MAX);

LIST(blacklist);
MEMB(bl_memb, struct BL, NUM_BL_MAX);

PROCESS(c_gtsp_process, "Clustering with embedded GTSP");
AUTOSTART_PROCESSES(&c_gtsp_process);

static struct CHB *ch;
static uint8_t temp_state;
static double my_cons_rate = 1.0;
static uint16_t sync_LC_addr = 0;

#if (TEST_GTSP || TEST_FTSP)
#else
static uint8_t saved_proactive_slot = 1;
static uint8_t saved_cons_slot = 1;
static uint8_t saved_slot_ack = 0;
static uint8_t saved_sync_border = 0;
#endif
static uint8_t soft_reset_count = 0;

inline char
enter_election_revelation(rtimer_t *rt)
{
    broadcast_announcement_stop();
    announcement_remove_value(&discovery_announcement);
    
    rtimer_coarse_schedule_ref = rt[RTIMER_0].time_coarse_hw;
    rtimer_fine_schedule_ref = rt[RTIMER_0].time_fine_hw;

    my_state = ELECTION_REVELATION;
    sync_LC_addr = my_addr; // Initialization before Consensus Sync
    process_poll(&c_gtsp_process);
    return 0;
}

/*---------------------------------------------------------------------------*/
inline char 
enter_election_declaration(rtimer_t *rt)
{ 
    polite_announcement_cancel();
    announcement_remove_value(&revelation_announcement);

    csync_update_placing();
    polite_announcement_set_interval(POLITE_INTERVAL * my_placing);
    
    rtimer_coarse_schedule_ref = rt[RTIMER_0].time_coarse_hw;
    rtimer_fine_schedule_ref = rt[RTIMER_0].time_fine_hw; 

#if TEST_BYZ
    if((my_addr == 64 || my_addr == 72))
        my_cluster.role = CH;    
#endif

    my_state = ELECTION_DECLARATION;

    process_poll(&c_gtsp_process);
    return 0;
}

/*---------------------------------------------------------------------------*/
inline char
enter_connection_revelation(rtimer_t *rt)
{
    polite_announcement_cancel();
    NETSTACK_RADIO.off();
    announcement_remove_value(&declaration_announcement);

    rtimer_coarse_schedule_ref = rt[RTIMER_0].time_coarse_hw;
    rtimer_fine_schedule_ref = rt[RTIMER_0].time_fine_hw; 

    my_state = CONNECTION_REVELATION;

    process_poll(&c_gtsp_process);
    return 0;
}

/*---------------------------------------------------------------------------*/
inline char
enter_connection_declaration(rtimer_t *rt)
{
    polite_announcement_cancel();
    announcement_remove_value(&revelation_announcement);

    packetbuf_set_attr(PACKETBUF_ATTR_CSYNC_CONN_DOAES, 0);
    NETSTACK_LLSEC.init();

    rtimer_coarse_schedule_ref = rt[RTIMER_0].time_coarse_hw;
    rtimer_fine_schedule_ref = rt[RTIMER_0].time_fine_hw; 

    my_state = CONNECTION_DECLARATION;

    process_poll(&c_gtsp_process);
    return 0;
}

/*---------------------------------------------------------------------------*/
inline char 
enter_convergence(rtimer_t *rt)
{
    polite_announcement_cancel();
    announcement_remove_value(&declaration_announcement);

    rtimer_coarse_schedule_ref = rt[RTIMER_0].time_coarse_hw;
    rtimer_fine_schedule_ref = rt[RTIMER_0].time_fine_hw; 
    
    my_state = CONSENSUS_CONVERGENCE;

    process_poll(&c_gtsp_process);
    return 0;
}

/*---------------------------------------------------------------------------*/
static inline char 
enter_convergence_slot(rtimer_t *rt)
{
    polite_announcement_cancel();
    if(my_cons_slot < NUM_CONS_SLOTS)
    {
        process_poll(&c_gtsp_process);
        return 0;
    }
    enter_consensus_revelation(rt);
    return 0;
}

/*---------------------------------------------------------------------------*/
inline char 
enter_consensus_revelation(rtimer_t *rt)
{
    polite_announcement_cancel();
    announcement_remove_value(&convergence_announcement);
    polite_announcement_set_interval(POLITE_INTERVAL * my_placing);
    NETSTACK_RADIO.on();

    rtimer_coarse_schedule_ref = rt[RTIMER_0].time_coarse_hw;
    rtimer_fine_schedule_ref = rt[RTIMER_0].time_fine_hw; 
    
    my_state = CONSENSUS_REVELATION;

    process_poll(&c_gtsp_process);
    return 0;
}


/*---------------------------------------------------------------------------*/
inline char 
enter_consensus_synchronization(rtimer_t *rt)
{
    polite_announcement_cancel();
    announcement_remove_value(&revelation_announcement);

    rtimer_coarse_schedule_ref = rt[RTIMER_0].time_coarse_hw;
    rtimer_fine_schedule_ref = rt[RTIMER_0].time_fine_hw; 

    my_state = CONSENSUS_SYNCHRONIZATION;

    process_poll(&c_gtsp_process);
    return 0;
}

/*---------------------------------------------------------------------------*/
/**************************NEW**************************/
inline char
enter_byzantine_consensus(rtimer_t *rt)
{
    if(my_sync_border)
        temp_state = BYZANTINE_CONSENSUS;
    else
        temp_state = my_state;
    process_poll(&c_gtsp_process);
    return 0;
}

/*---------------------------------------------------------------------------*/
static inline char
enter_synchronization_slot(rtimer_t *rt)
{
    polite_announcement_cancel();
    if(ref_n_CHB_degree - 1 < NUM_CONS_SLOTS)
    {
        process_poll(&c_gtsp_process);
        return 0;
    }
    enter_idle(rt);
    return 0;
}

/*---------------------------------------------------------------------------*/
inline char 
enter_idle(rtimer_t *rt)
{
#if (TEST_GTSP || TEST_FTSP)
    broadcast_announcement_stop();
    // announcement_remove_value(&discovery_announcement);

    rtimer_coarse_schedule_ref = rt[RTIMER_0].time_coarse_hw;
    rtimer_fine_schedule_ref = rt[RTIMER_0].time_fine_hw;
    NETSTACK_RADIO.off();
    PRINTF("Inside enter_idle\n");
    
    my_state = IDLE;

    process_poll(&c_gtsp_process);
    return 0;
#endif //TEST_GTSP

    polite_announcement_stop();
    announcement_remove_value(&synchronization_announcement);
    NETSTACK_RADIO.off();

    rtimer_coarse_schedule_ref = rt[RTIMER_0].time_coarse_hw;
    rtimer_fine_schedule_ref = rt[RTIMER_0].time_fine_hw;

    leds_off(LEDS_GREEN); 

    if(cons_ctrl_counter + 1 >= NUM_CONS_CTRL_ITERATIONS)
    {
        leds_off(LEDS_RED);
        leds_off(LEDS_BLUE);
    }

    my_state = IDLE;

    process_poll(&c_gtsp_process);
    return 0;
}

/*---------------------------------------------------------------------------*/
inline char 
enter_discovery(rtimer_t *rt)
{
#if (TEST_GTSP || TEST_FTSP)
    broadcast_announcement_stop();
    rtimer_coarse_schedule_ref = rt[RTIMER_0].time_coarse_hw;
    rtimer_fine_schedule_ref = rt[RTIMER_0].time_fine_hw; 
    process_poll(&c_gtsp_process);
    my_state = DISCOVERY;
    return 0;
#endif
    polite_announcement_stop();
    rtimer_coarse_schedule_ref = rt[RTIMER_0].time_coarse_hw;
    rtimer_fine_schedule_ref = rt[RTIMER_0].time_fine_hw; 

    process_poll(&c_gtsp_process);
    return 0;
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(c_gtsp_process, ev, data)
{  

    /* Dummy delay code added for Indriya testbed for allowing print statements*/
    int c; 
    // if(my_addr >= 1 && my_addr <= 15)
    //     for (c = 0; c <= 23500; c++);
    // else if(my_addr >= 16 && my_addr <= 25)
    //     for (c = 0; c <= 17500; c++);
    // else if(my_addr >= 26 && my_addr <= 39)
    //     for (c = 0; c <= 21800; c++);
    // else if(my_addr >= 40 && my_addr <= 48)
    //     for (c = 0; c <= 22400; c++);
    // else if(my_addr >= 49 && my_addr <= 61)
    //     for (c = 0; c <= 20500; c++);
    // else if(my_addr >= 62 && my_addr <= 78)
    //     for (c = 0; c <= 18500; c++);
    for (c = 1; c <= 19000; c++); 

    PROCESS_EXITHANDLER(broadcast_announcement_stop());
    PROCESS_BEGIN();


    // DISCOVERY
    reset_c_gtsp(); 
    
    while(1)
    { 

        soft_reset();
        
        csync_print_status();

        PROCESS_YIELD();
        // ELECTION_REVELATION

        while(my_state < CONSENSUS_CONVERGENCE)
        {
            switch(my_state)
            {
                case ELECTION_REVELATION:
                {
                    if(rtimer_schedule(RTIMER_0, RTIMER_INTERVAL_REF, 0, REGULAR_SLOT_INTERVAL, enter_election_declaration))
                    {
                        polite_announcement_init(LOGICAL_CHANNEL, 0, PA_RESILIENCE_MAX_SEND_DUPS, PA_REGULAR_MAX_RECV_DUPS);
                        csync_print_status();
                        announcement_set_instr(&revelation_announcement, my_state);
                        announcement_set_degree(&revelation_announcement, my_degree);
                        announcement_set_date_coarse(&revelation_announcement, rt[RTIMER_0].time_coarse_lg); //PRELIMINARY
                        announcement_set_date_fine(&revelation_announcement, rt[RTIMER_0].time_fine_lg); //PRELIMINARY
                        announcement_set_ref_addr(&revelation_announcement, my_addr);
                        announcement_set_cons_rate(&revelation_announcement, TRUE);
                        announcement_add_value(&revelation_announcement);
                        announcement_bump(&revelation_announcement);
                        break;
                    }
                }
                case ELECTION_DECLARATION:
                {
                    if(rtimer_schedule(RTIMER_0, RTIMER_INTERVAL_REF, 0, REGULAR_SLOT_INTERVAL, enter_connection_revelation))
                    {
                        csync_print_status();
                        announcement_set_instr(&declaration_announcement, my_state);
                        announcement_set_degree(&declaration_announcement, my_degree);
                        announcement_set_date_coarse(&declaration_announcement, rt[RTIMER_0].time_coarse_lg); //PRELIMINARY
                        announcement_set_date_fine(&declaration_announcement, rt[RTIMER_0].time_fine_lg); //PRELIMINARY
                        announcement_set_ref_addr(&declaration_announcement, my_addr);
                        announcement_set_cons_rate(&declaration_announcement, TRUE);
                        announcement_add_value(&declaration_announcement);
                        announcement_bump(&declaration_announcement);
                        break;
                    }
                }
                case CONNECTION_REVELATION:
                {
                    if(rtimer_schedule(RTIMER_0, RTIMER_INTERVAL_REF, 0, REGULAR_SLOT_INTERVAL, enter_connection_declaration))
                    {
                        csync_print_status();
                        if(list_length(*my_cluster.CHs_list) > 1 && my_cluster.role == CM)
                        {
                            NETSTACK_RADIO.on();

                            my_cluster.role = CB;
                            my_placing -= csync_CHB_placing();
                            polite_announcement_set_interval(POLITE_INTERVAL * my_placing);

                            announcement_set_instr(&revelation_announcement, my_state);
                            announcement_set_degree(&revelation_announcement, my_degree);
                            announcement_set_date_coarse(&revelation_announcement, rt[RTIMER_0].time_coarse_lg); //PRELIMINARY
                            announcement_set_date_fine(&revelation_announcement, rt[RTIMER_0].time_fine_lg); //PRELIMINARY
                            announcement_set_cons_rate(&revelation_announcement, TRUE);

                            ch = list_head(*my_cluster.CHs_list);
                            ref_n_CHB_addr = ch->addr;
                            ref_n_CHB_degree = ch->degree;
                            packetbuf_set_attr(PACKETBUF_ATTR_CSYNC_CONN_AES_XORPLAINTEXT_A, ch->addr);
                            ch = list_item_next(ch);
                            ref_n_CHB_addr += ch->addr;
                            ref_n_CHB_degree += ch->degree;

                            packetbuf_set_attr(PACKETBUF_ATTR_CSYNC_CONN_AES_XORPLAINTEXT_B, ch->addr);
                            packetbuf_set_attr(PACKETBUF_ATTR_CSYNC_CONN_DOAES, 1);
                            NETSTACK_LLSEC.init();

                            announcement_set_ref_addr(&revelation_announcement, ref_n_CHB_addr); //PRELIMINARY
                            announcement_add_value(&revelation_announcement);
                            announcement_bump(&revelation_announcement);
                        }
                    }
                    if(rt[RTIMER_0].func == enter_convergence)
                    {
                        rt[RTIMER_0].state = RTIMER_SINGLEPASS;
                    }
                    else
                        break;
                }

                case CONNECTION_DECLARATION:
                {
                    if(rtimer_schedule(RTIMER_0, RTIMER_INTERVAL_REF, 0, REGULAR_SLOT_INTERVAL, enter_convergence))
                    {
                        csync_print_status();
                        if(my_cluster.role == CH)
                        {
                            NETSTACK_RADIO.on();
                        }
                        else if(my_cluster.role == CB)
                        {
                            announcement_set_instr(&declaration_announcement, my_state);
                            announcement_set_degree(&declaration_announcement, ref_n_CHB_degree);
                            announcement_set_date_coarse(&declaration_announcement, rt[RTIMER_0].time_coarse_lg); //PRELIMINARY
                            announcement_set_date_fine(&declaration_announcement, rt[RTIMER_0].time_fine_lg); //PRELIMINARY
                            announcement_set_ref_addr(&declaration_announcement, ref_n_CHB_addr); //PRELIMINARY
                            announcement_set_cons_rate(&declaration_announcement, TRUE);
                            announcement_add_value(&declaration_announcement);
                            announcement_bump(&declaration_announcement);
                            break;
                        }
                    }
                }

                default:
                    break;
            }
            PROCESS_YIELD();
        }

        // while(cons_ctrl_counter < NUM_CONS_CTRL_ITERATIONS)
        // {

#if (TEST_GTSP || TEST_FTSP)
        while((my_state == IDLE) && (cons_ctrl_counter < NUM_CONS_CTRL_ITERATIONS))
        {
            if(cons_ctrl_counter < NUM_CONS_CTRL_ITERATIONS-1)
            {
                if(rtimer_schedule(RTIMER_0, RTIMER_INTERVAL_REF, 0, IDLE_SLOT_INTERVAL, enter_idle))
                {
                    PRINTF("Entered Idle phase\n");
                    csync_print_status();
#if IDLE_BROADCAST
                    NETSTACK_RADIO.on();
                    announcement_set_instr(&discovery_announcement, my_state);
#if TEST_FTSP
                    announcement_set_degree(&discovery_announcement, my_degree);
#else
                    announcement_set_degree(&discovery_announcement, cons_ctrl_counter);
#endif
                    announcement_set_date_coarse(&discovery_announcement, rt[RTIMER_0].time_coarse_lg); //PRELIMINARY
                    announcement_set_date_fine(&discovery_announcement, rt[RTIMER_0].time_fine_lg); //PRELIMINARY
                    announcement_set_cons_rate(&discovery_announcement, TRUE);
                    announcement_set_ref_addr(&discovery_announcement, 70);
                    announcement_add_value(&discovery_announcement);
                    broadcast_announcement_init(LOGICAL_CHANNEL, IDLE_MIN_INTERVAL, IDLE_MIN_INTERVAL, IDLE_MAX_INTERVAL);
#endif /*IDLE_BROADCASTS*/

                    PROCESS_YIELD();
                        
#if IDLE_BROADCAST
                    broadcast_announcement_stop();
                    announcement_remove_value(&discovery_announcement);
                    PRINTF("\n");
#endif /*IDLE_BROADCASTS*/
                }
            }
            else
            {
                if(rtimer_schedule(RTIMER_0, RTIMER_INTERVAL_REF, 0, IDLE_SLOT_INTERVAL, enter_discovery))
                {
                    PRINTF("Entered Idle phase for last time\n");
                    csync_print_status();
#if IDLE_BROADCAST
                    NETSTACK_RADIO.on();
                    announcement_set_instr(&discovery_announcement, my_state);
#if TEST_FTSP
                    announcement_set_degree(&discovery_announcement, my_degree);
#else
                    announcement_set_degree(&discovery_announcement, cons_ctrl_counter);
#endif
                    announcement_set_date_coarse(&discovery_announcement, rt[RTIMER_0].time_coarse_lg); //PRELIMINARY
                    announcement_set_date_fine(&discovery_announcement, rt[RTIMER_0].time_fine_lg); //PRELIMINARY
                    announcement_set_cons_rate(&discovery_announcement, TRUE);
                    announcement_set_ref_addr(&discovery_announcement, 70);
                    announcement_add_value(&discovery_announcement);
                    broadcast_announcement_init(LOGICAL_CHANNEL, IDLE_MIN_INTERVAL, IDLE_MIN_INTERVAL, IDLE_MAX_INTERVAL);
#endif /*IDLE_BROADCASTS*/

                    PROCESS_YIELD();
                        
#if IDLE_BROADCAST
                    broadcast_announcement_stop();
                    announcement_remove_value(&discovery_announcement);
                    PRINTF("\n");
#endif /*IDLE_BROADCASTS*/
                }
                
            }
            cons_ctrl_counter++;
        }


#else
            
        leds_on(LEDS_GREEN);
        if(cons_ctrl_counter > 0)
        {
            my_state = CONSENSUS_CONVERGENCE;
            if(my_cluster.role > CM)
            {
                NETSTACK_RADIO.on();
            }
            my_proactive_slot = 1;
            my_cons_slot = 1;
            my_slot_ack = 0;
            this_sync_slot = 1;
            my_sync_border = 0;
            for(ch = list_head(*my_cluster.CHs_list); ch != NULL; ch = list_item_next(ch))
            {
                ch->cons_slot = 0;
            }
            polite_announcement_init(LOGICAL_CHANNEL, my_placing, PA_RESILIENCE_MAX_SEND_DUPS, PA_REGULAR_MAX_RECV_DUPS);
        }

        if(rtimer_schedule(RTIMER_0, RTIMER_INTERVAL_REF, 0, CONS_CTRL_SLOT_INTERVAL*NUM_CONS_SLOTS, enter_consensus_revelation))
        {
            csync_print_status();
            announcement_set_date_coarse(&convergence_announcement, rt[RTIMER_0].time_coarse_lg); //PRELIMINARY
            announcement_set_date_fine(&convergence_announcement, rt[RTIMER_0].time_fine_lg); //PRELIMINARY
            announcement_set_cons_rate(&convergence_announcement, TRUE);
            announcement_add_value(&convergence_announcement);
            if(my_cluster.role == CH)
            {
                init_consensus_convergence();
                my_placing = csync_CHB_placing();
                polite_announcement_set_interval(POLITE_INTERVAL * my_placing);
            }
            else if(my_cluster.role == CB)
            {
                polite_announcement_set_interval(0);
            }

            if(my_cluster.role > CM)
            {
                int8_t n_CH_count = 0;
                while(my_cons_slot <= NUM_CONS_SLOTS)
                {
                    if(my_cluster.role == CH)
                    {

                        n_CH_count = list_length(*my_cluster.CHs_list);

                        if(n_CH_count == 0)
                        {
                            NETSTACK_RADIO.off();
                            break;
                        }

                        for(ch = list_head(*my_cluster.CHs_list); ch != NULL; ch = list_item_next(ch))
                        {
                            if(ch->cons_slot != 0)
                            {
                                n_CH_count--;
                            }
                        }
                    }


                    if(rtimer_schedule(RTIMER_0, RTIMER_INTERVAL_REF, 0, CONS_CTRL_SLOT_INTERVAL*my_cons_slot, enter_convergence_slot))
                    {
                        //PRINTF(" role %u, ack %u, n_CH_count %u, cons_slot %u, my_proactive_slot %u", my_cluster.role, my_slot_ack, n_CH_count, my_cons_slot, my_proactive_slot);
                        if(my_cluster.role == CB)
                        {
                            my_slot_ack = 0;
                        }
                        else if(my_cluster.role == CH && !my_slot_ack && (n_CH_count <= 1 || my_cons_slot == my_proactive_slot))
                        {
                            PRINTF(" slot %u", my_cons_slot);
                            if(n_CH_count > 1)
                            {
                                announcement_set_instr(&convergence_announcement, CONVERGENCE_PROACTIVE);
                                polite_announcement_set_interval(POLITE_INTERVAL * my_placing + POLITE_PROACTIVE_OFFSET);
                                PRINTF(" PROACTIVE");
                            }
                            announcement_set_degree(&convergence_announcement, my_cons_slot);
                            announcement_bump(&convergence_announcement);
                            //PRINTF(" BUMP");
                        }

                        if(my_cons_slot < NUM_CONS_SLOTS)
                        {
                            PROCESS_YIELD();

                            if(my_cluster.role == CB)
                            {
                                for(ch = list_head(*my_cluster.CHs_list); ch != NULL; ch = list_item_next(ch))
                                {
                                    my_slot_ack *= ch->cons_slot;
                                }
                            }
                            if(my_slot_ack)
                            {
                                NETSTACK_RADIO.off();
                                if(rtimer_schedule(RTIMER_0, RTIMER_INTERVAL_REF, 0, CONS_CTRL_SLOT_INTERVAL*NUM_CONS_SLOTS, enter_consensus_revelation))
                                {
                                    break;
                                }
                                else
                                {
                                    PRINTF(" failed B");
                                    my_cons_slot++;
                                }
                            }
                            else
                            {
                                my_cons_slot++;
                            }
                        }
                        else
                        {
                            break; //slot 10
                        }

                    }
                    else
                    {
                        PRINTF(" failed A");
                        my_cons_slot++;
                    }
                }
            }
        }

        if(my_cons_slot <= NUM_CONS_SLOTS)
        {
            PROCESS_YIELD();

            // CONSENSUS_REVELATION
            if(rtimer_schedule(RTIMER_0, RTIMER_INTERVAL_REF, 0, REGULAR_SLOT_INTERVAL, enter_consensus_synchronization))
            {
                csync_print_status();
                /* Let all the common nodes know the slot to wake up */
                if(my_cluster.role == CH)
                {
                    PRINTF("; my_slot_ack(%u)", my_slot_ack);
                    announcement_set_instr(&revelation_announcement, my_state);
                    announcement_set_degree(&revelation_announcement, my_cons_slot);
                    announcement_set_date_coarse(&revelation_announcement, rt[RTIMER_0].time_coarse_lg); //PRELIMINARY
                    announcement_set_date_fine(&revelation_announcement, rt[RTIMER_0].time_fine_lg); //PRELIMINARY
                    announcement_set_cons_rate(&revelation_announcement, TRUE);
                    announcement_add_value(&revelation_announcement);
                    announcement_bump(&revelation_announcement);
                }
                PROCESS_YIELD();
            }
        }

        
        while (cons_ctrl_counter < NUM_CONS_CTRL_ITERATIONS)
        {
            if(cons_ctrl_counter > 0)
            {
                my_state = CONSENSUS_SYNCHRONIZATION;
                if(my_cluster.role > CM)
                {
                    NETSTACK_RADIO.on();
                }
                my_proactive_slot = saved_proactive_slot;
                my_cons_slot = saved_cons_slot;
                my_slot_ack = saved_slot_ack;
                this_sync_slot = 1;
                my_sync_border = saved_sync_border;
                for(ch = list_head(*my_cluster.CHs_list); ch != NULL; ch = list_item_next(ch))
                {
                    ch->cons_slot = (NUM_CONS_SLOTS + 1) - ch->cons_slot;
                }
                polite_announcement_init(LOGICAL_CHANNEL, my_placing, PA_RESILIENCE_MAX_SEND_DUPS, PA_REGULAR_MAX_RECV_DUPS);
            }
            else
            {
                saved_proactive_slot = my_proactive_slot;
                saved_cons_slot = my_cons_slot;
                saved_slot_ack = my_slot_ack;
                saved_sync_border = my_sync_border;
                //saved_sync_slot = this_sync_slot;
            }

            // CONSENSUS_SYNCHRONIZATION
            if(rtimer_schedule(RTIMER_0, RTIMER_INTERVAL_REF, 0, CONS_CTRL_SLOT_INTERVAL*NUM_CONS_SLOTS, enter_idle))
            {
                init_consensus_synchronization();
                csync_print_status();

                if(my_cluster.role == CH)
                {
                    polite_announcement_set_interval(0);
                }
                else if(my_cluster.role == CB)
                {
                    polite_announcement_set_interval(POLITE_INTERVAL * my_placing);
                }

                announcement_set_instr(&synchronization_announcement, my_state);
                announcement_set_degree(&synchronization_announcement, my_cons_slot);
                announcement_set_date_coarse(&synchronization_announcement, rt[RTIMER_0].time_coarse_lg); //PRELIMINARY
                announcement_set_date_fine(&synchronization_announcement, rt[RTIMER_0].time_fine_lg); //PRELIMINARY
                announcement_set_cons_rate(&synchronization_announcement, TRUE);
                announcement_set_ref_addr(&synchronization_announcement, my_addr);
                announcement_add_value(&synchronization_announcement);

                

                NETSTACK_RADIO.off();

                //PRINTF(" my_cons_slot %u", my_cons_slot);

                while(ref_n_CHB_degree - 1 <= NUM_CONS_SLOTS)
                {
                    if(rtimer_schedule(RTIMER_0, RTIMER_INTERVAL_REF, 0, CONS_CTRL_SLOT_INTERVAL*(ref_n_CHB_degree - 1), enter_synchronization_slot))
                    {
                        if(ref_n_CHB_degree - 1 < NUM_CONS_SLOTS)
                        {
                            PROCESS_YIELD();

                            //PRINTF(" slot %u", ref_n_CHB_degree);
                            NETSTACK_RADIO.on();
                            
                            this_sync_slot = ref_n_CHB_degree;
                            if(my_sync_border && this_sync_slot == my_cons_slot)
                            {
                                //PRINTF(" BUMPB");
                                announcement_set_degree(&synchronization_announcement, my_cons_slot);
#if TEST_BYZ
                                // Testing Byzantine fault by injecting fault
                                if((my_addr == 64) && (my_cluster.role == CH))
                                {
                                    rtimer_adjust_fine_offset(10000);
                                    announcement_set_date_fine(&synchronization_announcement, rt[RTIMER_0].time_fine_lg);
                                }
#endif
                                //PRINTF("Sending sync message with my_cons_rate is %d\n", (uint16_t)(my_cons_rate * 1000));
                                announcement_set_cons_rate(&synchronization_announcement, my_cons_rate);
                                announcement_bump(&synchronization_announcement);
                                my_sync_border = 0;
                                /******* NEW **********/
#if TEST_BYZ
                                // if(my_addr != 69)
                                {
                                while ((msg_count < (my_degree/2 + 1)))
                                {
                                    // Nitin -test this chunk of code to verify functionality
                                    polite_announcement_set_interval(BYZANTINE_INTERVAL * my_placing);
                                    if(msg_count > 1)
                                    {
                                        PRINTF("Message count is > 1\n");
                                        announcement_set_instr(&synchronization_announcement, BYZANTINE_CONSENSUS);
                                        announcement_set_degree(&synchronization_announcement, my_cons_slot);
                                        announcement_set_date_coarse(&synchronization_announcement, rt[RTIMER_0].time_coarse_lg); //PRELIMINARY
                                        announcement_set_date_fine(&synchronization_announcement, rt[RTIMER_0].time_fine_lg); //PRELIMINARY
                                        announcement_set_cons_rate(&synchronization_announcement, my_cons_rate);
                                        announcement_add_value(&synchronization_announcement);

                                    }
                                    if(my_cluster.role == CB)
                                    {
                                        temp_state = BYZANTINE_CONSENSUS;
                                        announcement_set_instr(&synchronization_announcement, temp_state);
                                        announcement_bump(&synchronization_announcement);
                                    }
                                    PROCESS_YIELD();
                                    /* Do process poll in receive synchronization message */
                                    if((my_sync_border) && (msg_count < (my_degree/2 + 1)))
                                    {
                                        announcement_set_instr(&synchronization_announcement, BYZANTINE_CONSENSUS);
                                        announcement_set_degree(&synchronization_announcement, my_cons_slot);
                                        announcement_set_date_coarse(&synchronization_announcement, rt[RTIMER_0].time_coarse_lg); //PRELIMINARY
                                        announcement_set_date_fine(&synchronization_announcement, rt[RTIMER_0].time_fine_lg); //PRELIMINARY
                                        announcement_set_cons_rate(&synchronization_announcement, my_cons_rate);
                                        announcement_bump(&synchronization_announcement);
                                        PRINTF("INITIATED BYZANTINE BUMP\n");
                                    }
                                    PRINTF("BYANTINE PART COMPLETE");
                                }
                            }
#endif
                            }
                            /* Add another timer slot to check if message was sent */
                            else
                            {
                                //PRINTF(" LISTEN");
                            }

                        }
                        ref_n_CHB_degree++;
                    }
                    else
                    {
                        //PRINTF(" failed A %u", ref_n_CHB_degree);
                        ref_n_CHB_degree++;
                        NETSTACK_RADIO.on();
                    }

                    // UPDATE ref_n_CHB_degree here!!!!!
                }


                if(ref_n_CHB_degree - 2 <= NUM_CONS_SLOTS)
                {
                    PROCESS_YIELD();
                    // IDLE

                    //PRINTF("\n %lu, %lu", rtimer_coarse_schedule_ref, rtimer_fine_schedule_ref);
                    if(rtimer_schedule(RTIMER_0, RTIMER_DATE, announcement_get_date_coarse(&synchronization_announcement), announcement_get_date_fine(&synchronization_announcement) + IDLE_SLOT_INTERVAL, enter_discovery))
                    {
                        csync_print_status();
#if IDLE_BROADCAST
                        NETSTACK_RADIO.on();
                        announcement_set_instr(&discovery_announcement, my_state);
                        announcement_set_degree(&discovery_announcement, cons_ctrl_counter);
                        announcement_set_date_coarse(&discovery_announcement, rt[RTIMER_0].time_coarse_lg); //PRELIMINARY
                        announcement_set_date_fine(&discovery_announcement, rt[RTIMER_0].time_fine_lg); //PRELIMINARY
                        announcement_set_cons_rate(&discovery_announcement, TRUE);
                        announcement_set_ref_addr(&discovery_announcement, my_addr);
                        announcement_add_value(&discovery_announcement);
                        broadcast_announcement_init(LOGICAL_CHANNEL, IDLE_MIN_INTERVAL, IDLE_MIN_INTERVAL, IDLE_MAX_INTERVAL);
#endif /*IDLE_BROADCASTS*/

                        PROCESS_YIELD();
                        
#if IDLE_BROADCAST
                        broadcast_announcement_stop();
                        announcement_remove_value(&discovery_announcement);
                        PRINTF("\n");
#endif /*IDLE_BROADCASTS*/
                    }
                }
            }
            cons_ctrl_counter++;
        }
        #endif
    }
    PROCESS_END();
}

/*---------------------------------------------------------------------------*/
void reset_c_gtsp(void)
{


    ref_n_CHB_degree = 0;
    ref_n_CHB_addr = 0;   

    rtimer_coarse_schedule_ref = 0;
    rtimer_fine_schedule_ref = 0;
    memset(rt, 0, sizeof(rt));

    announcement_init();
    my_state = DISCOVERY;

    synced_counter= 0;

    my_addr = linkaddr_node_addr.u16;

// #if TEST_FTSP
//     return;
// #endif


    memb_init(&neighbour_memb);
    list_init(neighbour_list);

// #if TEST_FTSP
//     memb_init(&regression_memb);
//     list_init(&regression_list);
// #endif

#if MOD_NEIGHBOURS
    memb_init(&mod_neighbour_memb);
    list_init(mod_neighbour_list);
#endif /*MOD_NEIGHBOURS*/


    //cc2420_set_txpower(30);

    
#if MOD_NEIGHBOURS
    my_degree = check_mod_neighbours(my_addr);
#else /*MOD_NEIGHBOURS*/    
    my_degree = 0;
#endif /*MOD_NEIGHBOURS*/
    my_placing = 0;
    my_cluster.role = CH;
    soft_reset_count = 0;

    memb_init(&CHs_memb);
    memb_init(&CBs_memb);
    list_init(CHs_list);
    list_init(CBs_list);

    my_cluster.m_CH = &CHs_memb;
    my_cluster.m_CB = &CBs_memb;
    my_cluster.CHs_list = &CHs_list;
    my_cluster.CBs_list = &CBs_list;

    memb_init(&bl_memb);
    list_init(blacklist);
    my_cluster.m_bl = &bl_memb;
    my_cluster.blacklist = &blacklist;
    
    // my_proactive_slot = 1;
    // my_cons_slot = 1;
    // my_cons_rate = 1.0;
    // my_slot_ack = 0;
    // this_sync_slot = 1;
    // my_sync_border = 0;
    // cons_ctrl_counter = 0;      

    // NETSTACK_RADIO.set_value(RADIO_PARAM_CCA_THRESHOLD, RADIO_CCA_THRESHOLD);
    // NETSTACK_RADIO.on();

    // announcement_register(&discovery_announcement, 0, received_discovery_announcement);
    // announcement_add_value(&discovery_announcement);
    // announcement_register(&revelation_announcement, 1, received_revelation_announcement);
    // announcement_register(&declaration_announcement, 2, received_declaration_announcement);
    // announcement_register(&convergence_announcement, 3, received_convergence_announcement);
    // announcement_register(&synchronization_announcement, 4, received_synchronization_announcement);

    // broadcast_announcement_init(LOGICAL_CHANNEL, DISC_MIN_INTERVAL, DISC_MIN_INTERVAL, DISC_MAX_INTERVAL);
}


void 
soft_reset(void)
{

    announcement_init();
    my_state = DISCOVERY;

    rtimer_coarse_schedule_ref = 0;
    rtimer_fine_schedule_ref = 0;
    memset(rt, 0, sizeof(rt));

    synced_counter = 0;
    my_proactive_slot = 1;
    my_cons_slot = 1;
    my_cons_rate = 1.0;
    my_slot_ack = 0;
    this_sync_slot = 1;
    my_sync_border = 0;
    cons_ctrl_counter = 0;
    msg_count = 0;

    soft_reset_count++;

    NETSTACK_RADIO.set_value(RADIO_PARAM_CCA_THRESHOLD, RADIO_CCA_THRESHOLD);
    NETSTACK_RADIO.on();

#if (TEST_GTSP || TEST_FTSP)
    announcement_register(&discovery_announcement, 0, received_discovery_announcement);
    
#if TEST_FTSP
    if(my_addr == 70)
        my_degree = 1;
    else if (my_addr == 71)
        my_degree = 2;
    else if (my_addr == 62)
        my_degree = 3;
    else if (my_addr == 64)
        my_degree = 4;
    else if (my_addr == 65)
        my_degree = 5;
    else if (my_addr == 75)
        my_degree = 6;
    else if (my_addr == 72)
        my_degree = 7;
    else if (my_addr == 78)
        my_degree = 8;
    else if (my_addr == 49)
        my_degree = 9;
    announcement_set_degree(&discovery_announcement, my_degree);
#endif
    announcement_add_value(&discovery_announcement);
#else
    announcement_register(&discovery_announcement, 0, received_discovery_announcement);
    announcement_add_value(&discovery_announcement);
    announcement_register(&revelation_announcement, 1, received_revelation_announcement);
    announcement_register(&declaration_announcement, 2, received_declaration_announcement);
    announcement_register(&convergence_announcement, 3, received_convergence_announcement);
    announcement_register(&synchronization_announcement, 4, received_synchronization_announcement);
#endif

    broadcast_announcement_init(LOGICAL_CHANNEL, 10, 20, 20);
}



/*---------------------------------------------------------------------------*/
void
csync_print_status(void)
{
    PRINTF("\n%u ", my_addr);

    if(my_state == DISCOVERY)
    {
        PRINTF("-> D");
    }

    else if(my_state == ELECTION_REVELATION)
    {
        leds_init();
        leds_on(LEDS_GREEN);
        PRINTF("-> E_R");
    }

    else if(my_state == ELECTION_DECLARATION)
    {
        PRINTF("-> E_D");
    }

    else if(my_state == CONNECTION_REVELATION)
    {
        PRINTF("-> C_R");
        if(my_cluster.role == CM)
        {
            if(list_length(*my_cluster.CHs_list) > 1)
            {
                PRINTF(" as CB");
                for(ch = list_head(*my_cluster.CHs_list); ch != NULL; ch = list_item_next(ch))
                {
                    PRINTF(" | ->CH %u D%u", ch->addr, ch->degree);
                }
            }
            else
            {
                ch = list_head(*my_cluster.CHs_list);
                PRINTF(" as CM | CH %u D%u", ch->addr, ch->degree);
            }
        }
        else if(my_cluster.role == CH)
        {
            leds_on(LEDS_RED);
            PRINTF(" as CH D%u", my_degree);
        }
    }

    else if(my_state == CONNECTION_DECLARATION)
    {
        PRINTF("-> C_D");
    }

    else if(my_state == CONSENSUS_CONVERGENCE)
    {
        PRINTF("-> CONV");
        if(my_cluster.role == CM)
        {
            PRINTF(" as CM");
        }
        else if(my_cluster.role == CH)
        {
            PRINTF(" as CH");
            for(ch = list_head(*my_cluster.CBs_list); ch != NULL; ch = list_item_next(ch))
            {
                PRINTF(" | CB %u -> CH %u D%u", ch->addr, ch->n_CHB_addr_A, ch->degree);
            }
        }
        else if(my_cluster.role == CB)
        {
            leds_on(LEDS_BLUE);
            PRINTF(" as CB");
        }
    }

    else if(my_state == CONSENSUS_REVELATION)
    {
        PRINTF("-> CS_R");
        if(my_cluster.role == CM)
        {
            PRINTF(" as CM");
            return;
        }
        else if(my_cluster.role == CH)
        {
            PRINTF(" as CH S%u", my_cons_slot);
        }
        else if(my_cluster.role == CB)
        {
            PRINTF(" as CB");
        }
        for(ch = list_head(*my_cluster.CHs_list); ch != NULL; ch = list_item_next(ch))
        {
            PRINTF(" | CH %u S%u", ch->addr, ch->cons_slot);
        }
    }

    else if(my_state == CONSENSUS_SYNCHRONIZATION)
    {
        PRINTF("-> CS_S");
        if(my_cluster.role == CM)
        {
            PRINTF(" as CM");
        }
        else if(my_cluster.role == CH)
        {
            PRINTF(" as CH S%u", my_cons_slot);
        }
        else if(my_cluster.role == CB)
        {
            PRINTF(" as CB");
        }
        for(ch = list_head(*my_cluster.CHs_list); ch != NULL; ch = list_item_next(ch))
        {
            PRINTF(" | CH %u S%u", ch->addr, ch->cons_slot);
            if ((ch->cons_slot >= my_cons_slot) && (my_cluster.role == CH))
            {
                PRINTF("Potentially an LC, need to synchronize with it\n");
                sync_LC_addr = ch->addr;
            }
        }
    }

    else if(my_state == IDLE)
    {
        PRINTF("-> I");
        powertrace_print("");
        PRINTF("\n");
    }
    /**************************NEW**************************/
    else if(temp_state == BYZANTINE_CONSENSUS)
    {
        PRINTF("-> BYZ");
        if(my_cluster.role == CM)
        {
            PRINTF(" as CM");
        }
        else if(my_cluster.role == CH)
        {
            PRINTF(" as CH S%u", my_cons_slot);
        }
        else if(my_cluster.role == CB)
        {
            PRINTF(" as CB");
        }
    }
}

uint8_t
csync_all_synced(void)
{
    struct neighbour *n;

#if MOD_NEIGHBOURS
#if TEST_FTSP
    return is_synced();
#else
    for(n = list_head(mod_neighbour_list); n != NULL; n = list_item_next(n))
    {
        if(n->synced == 0 || n->state > 1)
        {
            return 0;
        }
    }
#endif
#else /*MOD_NEIGHBOURS*/
    for(n = list_head(neighbour_list); n != NULL; n = list_item_next(n))
    {
        if(n->synced == 0 || n->state > 1)
        {
            return 0;
        }
    }
#endif /*MOD_NEIGHBOURS*/
    return 1;
}

/*---------------------------------------------------------------------------*/
#if MOD_NEIGHBOURS && MOD_TYPE == 0
uint8_t 
check_mod_neighbours(uint16_t n_id)
{
    switch(my_addr)
    {
        case 31976:
            switch(n_id)
            {
                case 34116:
                    return 1;
                default:
                    return 0;
            }
        case 34116:
            switch(n_id)
            {
                case 31976: case 40260:
                    return 1;
                default:
                    return 0;
            }
        case 40260:
            switch(n_id)
            {
                case 31976: case 40779:
                    return 1;
                default:
                    return 0;
            }
        case 40779:
            switch(n_id)
            {
                case 40260: case 40921:
                    return 1;
                default:
                    return 0;
            }
        case 40921:
            switch(n_id)
            {
                case 40779: case 42479:
                    return 1;
                default:
                    return 0;
            }
        case 42479:
            switch(n_id)
            {
                case 40921: case 43887:
                    return 1;
                default:
                    return 0;
            }
        case 43887:
            switch(n_id)
            {
                case 42479: case 49144:
                    return 1;
                default:
                    return 0;
            }
        case 49144:
            switch(n_id)
            {
                case 43887: case 49334:
                    return 1;
                default:
                    return 0;
            }
        case 49334:
            switch(n_id)
            {
                case 49144: case 51598:
                    return 1;
                default:
                    return 0;
            }
        case 51598:
            switch(n_id)
            {
                case 49334: case 52492:
                    return 1;
                default:
                    return 0;
            }
        case 52492:
            switch(n_id)
            {
                case 51598:
                    return 1;
                default:
                    return 0;
            }

        default:
            return 0;
    }
}
#endif /*MOD_NEIGHBOURS && MOD_TYPE == 0*/
/*---------------------------------------------------------------------------*/
#if MOD_NEIGHBOURS && MOD_TYPE == 1
uint8_t 
check_mod_neighbours(uint16_t n_id)
{
    switch(my_addr)
    {
        // case 31976:
        //     switch(n_id)
        //     {
        //         case 40779: case 40260:
        //             return 1;
        //         default:
        //             return 0;
        //     }
        // case 40260:
        //     switch(n_id)
        //     {
        //         case 41457: case 40921: case 40779: case 31976:
        //             return 1;
        //         default:
        //             return 0;
        //     }
        // case 40779:
        //     switch(n_id)
        //     {
        //         case 40779:
        //             return 10;
        //         case 41457: case 40921: case 40260: case 31976:
        //             return 1;
        //         default:
        //             return 0;
        //     }
        // case 40921:
        //     switch(n_id)
        //     {
        //         case 42479: case 41457: case 40779: case 34116: case 40260: case 53183:
        //             return 1;
        //         default:
        //             return 0;
        //     }
        // case 41457:
        //     switch(n_id)
        //     {
        //         case 42479: case 40921: case 40779: case 34116: case 40260: case 53183: case 50664:
        //             return 1;
        //         default:
        //             return 0;
        //     }
        // case 50664:
        //     switch(n_id)
        //     {
        //         case 42479: case 41457: case 39170:
        //             return 1;
        //         default:
        //             return 0;
        //     }
        // case 42479:
        //     switch(n_id)
        //     {
        //         case 42479:
        //             return 10;
        //         case 49334: case 51598: case 41457: case 40921: case 34116: case 53183: case 50664: case 39170:
        //             return 1;
        //         default:
        //             return 0;
        //     }
        // case 53183:
        //     switch(n_id)
        //     {
        //         case 49334: case 51598: case 41457: case 40921: case 42479: case 34116:
        //             return 1;
        //         default:
        //             return 0;
        //     }
        // case 34116:
        //     switch(n_id)
        //     {
        //         case 34116:
        //             return 10;
        //         case 49334: case 51598: case 41457: case 40921: case 42479: case 53183:
        //             return 1;
        //         default:
        //             return 0;
        //     }
        // case 49334:
        //     switch(n_id)
        //     {
        //         case 42479: case 51598: case 52492: case 34116: case 43887: case 53183:
        //             return 1;
        //         default:
        //             return 0;
        //     }
        // case 39170:
        //     switch(n_id)
        //     {
        //         case 42479: case 41457: case 50664:
        //             return 1;
        //         default:
        //             return 0;
        //     }
        // case 51598:
        //     switch(n_id)
        //     {
        //         case 42479: case 49334: case 52492: case 34116: case 53183: case 39170:
        //             return 1;
        //         default:
        //             return 0;
        //     }
        // case 52492:
        //     switch(n_id)
        //     {
        //         case 52492:
        //             return 10;
        //         case 49144: case 43887: case 49334: case 51598:
        //             return 1;
        //         default:
        //             return 0;
        //     }
        // case 43887:
        //     switch(n_id)
        //     {
        //         case 52492: case 49144: case 51598: case 49334:
        //             return 1;
        //         default:
        //             return 0;
        //     }
        // case 49144:
        //     switch(n_id)
        //     {
        //         case 52492: case 43887:
        //             return 1;
        //         default:
        //             return 0;
        //     }

        case 40:
            switch(n_id)
            {
                case 48:
                    return 1;
                default:
                    return 0;
            }

        case 42:
            switch(n_id)
            {
                case 48: //case 44: 
                    return 1;
                default:
                    return 0;
            }

        case 44:
            switch(n_id)
            {
                case 48:   //case 45: case 72:
                    return 1;
                default:
                    return 0;
            }

        

        case 45:
            switch(n_id)
            {
                case 48: //case 72:  //case 44: 
                    return 1;
                default:
                    return 0;
            }


        case 46:
            switch(n_id)
            {
                case 48:
                    return 1;
                default:
                    return 0;
            }


        case 47:
            switch(n_id)
            {
                case 48:
                    return 1;
                default:
                    return 0;
            }

        

        case 48:
            switch(n_id)
            {
                case 40: case 46: case 44: case 47: case 45: case 42:
                    return 1;
                default:
                    return 0;
            }

        case 72:
            switch(n_id)
            {
                case 77: case 78: //case 75: case 76: case 45: 
                    return 1;
                default:
                    return 0;
            }

        case 78:
            switch(n_id)
            {
                case 72: //case 49:
                    return 1;
                default:
                    return 0;
            }

        case 77:
            switch(n_id)
            {
                case 72: //case 78:
                    return 1;
                default:
                    return 0;
            }


        case 75:
            switch(n_id)
            {
                case 64: //case 72:  case 76: //case 65: case 63: case 62: 
                    return 1;
                default:
                    return 0;
            }


        case 76:
            switch(n_id)
            {
                case 64: //case 72:  case 77: //case 62: case 69: case 74: 
                    return 1;
                default:
                    return 0;
            }

        case 70:
            switch(n_id)
            {
                case 64:  //case 71: case 66: case 62: 
                    return 1;
                default:
                    return 0;
            }


        case 71:
            switch(n_id)
            {
                case 64: //case 70: case 63: case 66: case 74: 
                    return 1;
                default:
                    return 0;
            }


        case 62:
            switch(n_id)
            {
                case 64:  //case 54: case 70: case 63: case 75: case 76:
                    return 1;
                default:
                    return 0;
            }

        case 63:
            switch(n_id)
            {
                case 64:  // case 71: case 75: case 62: case 66:
                    return 1;
                default:
                    return 0;
            }


        case 64:
            switch(n_id)
            {
                case 70: case 71: case 63: case 62: case 65: case 66: case 69: case 75: case 76:
                    return 1;
                default:
                    return 0;
            }


        case 65:
            switch(n_id)
            {
                case 64:  //case 74: 
                    return 1;
                default:
                    return 0;
            }

        case 66:
            switch(n_id)
            {
                case 64:  //case 71: case 70: case 63: 
                    return 1;
                default:
                    return 0;
            }

        case 69:
            switch(n_id)
            {
                case 64:   // case 74: case 76: 
                    return 1;
                default:
                    return 0;
            }



        case 74:
            switch(n_id)
            {
                case 64: // case 65: case 69: case 71: case 76: case 60:
                    return 1;
                default:
                    return 0;
            }


        case 58:
            switch(n_id)
            {
                case 49: case 61: //case 55: case 54:  //
                    return 1;
                default:
                    return 0;
            }

        case 55:
            switch(n_id)
            {
                case 49: //case 58:
                    return 1;
                default:
                    return 0;
            }

        case 49:
            switch(n_id)
            {
                case 55: case 58: case 53: //case 78:
                    return 1;
                default:
                    return 0;
            }


        case 53:
            switch(n_id)
            {
                case 49: //case 55: 
                    return 1;
                default:
                    return 0;
            }


        case 60:
            switch(n_id)
            {
                case 51: //case 74:
                    return 1;
                default:
                    return 0;
            }

        case 61:
            switch(n_id)
            {
                case 58:
                    return 1;
                default:
                    return 0;
            }


        case 15:
            switch(n_id)
            {
                case 12:
                    return 1;
                default:
                    return 0;
            }


        case 12:
            switch(n_id)
            {
                case 1: case 15: // case 54: case 9:
                    return 1;
                default:
                    return 0;
            }


        case 9:
            switch(n_id)
            {
                case 1: // case 12: case 10: case 13:
                    return 1;
                default:
                    return 0;
            }


        case 10:
            switch(n_id)
            {
                case 1: case 51: //case 9: 
                    return 1;
                default:
                    return 0;
            }


        case 13:
            switch(n_id)
            {
                case 1: case 5: //case 9:
                    return 1;
                default:
                    return 0;
            }


        case 5:
            switch(n_id)
            {
                case 13:
                    return 1;
                default:
                    return 0;
            }


        case 51:
            switch(n_id)
            {
                case 60: case 10: case 56:
                    return 1;
                default:
                    return 0;
            }

        case 56:
            switch(n_id)
            {
                case 51: case 64:
                    return 1;
                default:
                    return 0;
            }

        case 54:
            switch(n_id)
            {
                case 1: //case 58: //case 61:  //case 62: case 12: 
                    return 1;
                default:
                    return 0;
            }


        case 1:
            switch(n_id)
            {
                case 51: case 10: case 12: case 9: case 54:
                    return 1;
                default:
                    return 0;
            }


        case 17:
            switch(n_id)
            {
                case 19:
                    return 1;
                default:
                    return 0;
            }

        case 19:
            switch(n_id)
            {
                case 17:
                    return 1;
                default:
                    return 0;
            }

        case 27:
            switch(n_id)
            {
                case 29:
                    return 1;
                default:
                    return 0;
            }

        case 29:
            switch(n_id)
            {
                case 27:
                    return 1;
                default:
                    return 0;
            }

        case 22:
            switch(n_id)
            {
                case 24: case 18: case 23: case 35:
                    return 1;
                default:
                    return 0;
            }

        case 23:
            switch(n_id)
            {
                case 22: case 16: case 52:
                    return 1;
                default:
                    return 0;
            }

        case 16:
            switch(n_id)
            {
                case 23:
                    return 1;
                default:
                    return 0;
            }


        case 52:
            switch(n_id)
            {
                case 23:
                    return 1;
                default:
                    return 0;
            }

        case 24:
            switch(n_id)
            {
                case 22:
                    return 1;
                default:
                    return 0;
            }

        case 35:
            switch(n_id)
            {
                case 30:
                    return 1;
                default:
                    return 0;
            }

        case 18:
            switch(n_id)
            {
                case 22:
                    return 1;
                default:
                    return 0;
            }

        case 28:
            switch(n_id)
            {
                case 30:
                    return 1;
                default:
                    return 0;
            }

        case 30:
            switch(n_id)
            {
                case 35: case 28:
                    return 1;
                default:
                    return 0;
            }

        default:
            return 0;
    }
}
#endif /*MOD_NEIGHBOURS && MOD_TYPE == 1*/


/*---------------------------------------------------------------------------*/
#if MOD_NEIGHBOURS && MOD_TYPE == 7
uint8_t 
check_mod_neighbours(uint16_t n_id)
{
    switch(my_addr)
    {
        case 40:
            switch(n_id)
            {
                case 48:
                    return 1;
                default:
                    return 0;
            }

        case 42:
            switch(n_id)
            {
                case 48: case 44: 
                    return 1;
                default:
                    return 0;
            }

        case 44:
            switch(n_id)
            {
                case 48:  case 42: //case 72:
                    return 1;
                default:
                    return 0;
            }

        

        case 45:
            switch(n_id)
            {
                case 48: //case 72:  //case 44: 
                    return 1;
                default:
                    return 0;
            }


        case 46:
            switch(n_id)
            {
                case 48:
                    return 1;
                default:
                    return 0;
            }


        case 47:
            switch(n_id)
            {
                case 48:
                    return 1;
                default:
                    return 0;
            }

        

        case 48:
            switch(n_id)
            {
                case 40: case 46: case 44: case 47: case 45: case 42:
                    return 1;
                default:
                    return 0;
            }



        case 75:
            switch(n_id)
            {
                case 64: //case 72:  case 76: //case 65: case 63: case 62: 
                    return 1;
                default:
                    return 0;
            }


        case 76:
            switch(n_id)
            {
                case 64: //case 72:  case 77: //case 62: case 69: case 74: 
                    return 1;
                default:
                    return 0;
            }

        case 70:
            switch(n_id)
            {
                case 64:  case 71: case 66: //case 62: 
                    return 1;
                default:
                    return 0;
            }


        case 71:
            switch(n_id)
            {
                case 64: case 70: case 66: //case 63: case 74: 
                    return 1;
                default:
                    return 0;
            }


        case 62:
            switch(n_id)
            {
                case 64:  //case 54: case 70: case 63: case 75: case 76:
                    return 1;
                default:
                    return 0;
            }

        case 63:
            switch(n_id)
            {
                case 64:  // case 71: case 75: case 62: case 66:
                    return 1;
                default:
                    return 0;
            }


        case 64:
            switch(n_id)
            {
                case 70: case 71: case 63: case 62: case 65: case 66: case 69: case 75: case 76:
                    return 1;
                default:
                    return 0;
            }


        case 65:
            switch(n_id)
            {
                case 64:  //case 74: 
                    return 1;
                default:
                    return 0;
            }

        case 66:
            switch(n_id)
            {
                case 64:  case 71: case 70: //case 63: 
                    return 1;
                default:
                    return 0;
            }

        case 69:
            switch(n_id)
            {
                case 64: case 74: // case 76: 
                    return 1;
                default:
                    return 0;
            }



        case 74:
            switch(n_id)
            {
                case 64: case 69: //case 65: case 71: case 76: case 60:
                    return 1;
                default:
                    return 0;
            }


        case 58:
            switch(n_id)
            {
                case 49:  //case 55: case 54: case 61:
                    return 1;
                default:
                    return 0;
            }

        case 55:
            switch(n_id)
            {
                case 49: case 58:
                    return 1;
                default:
                    return 0;
            }

        case 49:
            switch(n_id)
            {
                case 55: case 58: case 53: //case 78:
                    return 1;
                default:
                    return 0;
            }


        case 53:
            switch(n_id)
            {
                case 49: //case 55: 
                    return 1;
                default:
                    return 0;
            }


        case 12:
            switch(n_id)
            {
                case 1:  //case 9: // case 54:  case 15:
                    return 1;
                default:
                    return 0;
            }


        case 9:
            switch(n_id)
            {
                case 1: case 13: // case 12: case 10: case 13:
                    return 1;
                default:
                    return 0;
            }


        case 10:
            switch(n_id)
            {
                case 1: case 51: //case 9: 
                    return 1;
                default:
                    return 0;
            }


        case 13:
            switch(n_id)
            {
                case 1: case 9: //case 5: //
                    return 1;
                default:
                    return 0;
            }



        case 54:
            switch(n_id)
            {
                case 1: //case 58: //case 61:  //case 62: case 12: 
                    return 1;
                default:
                    return 0;
            }


        case 1:
            switch(n_id)
            {
                case 51: case 10: case 12: case 9: case 54:
                    return 1;
                default:
                    return 0;
            }


        default:
            return 0;
    }
}
#endif /*MOD_NEIGHBOURS && MOD_TYPE == 7*/


/*---------------------------------------------------------------------------*/
#if MOD_NEIGHBOURS && MOD_TYPE == 6
uint8_t 
check_mod_neighbours(uint16_t n_id)
{
    switch(my_addr)
    {
        case 40:
            switch(n_id)
            {
                case 48:
                    return 1;
                default:
                    return 0;
            }
       

        case 45:
            switch(n_id)
            {
                case 48: case 72: 
                    return 1;
                default:
                    return 0;
            }


        

        case 48:
            switch(n_id)
            {
                case 40: case 45:
                    return 1;
                default:
                    return 0;
            }

        case 72:
            switch(n_id)
            {
                case 78: case 75: case 76: case 45: 
                    return 1;
                default:
                    return 0;
            }

        case 78:
            switch(n_id)
            {
                case 72: case 49:
                    return 1;
                default:
                    return 0;
            }



        case 75:
            switch(n_id)
            {
                case 64: case 72: // case 76: //case 65: case 63: case 62: 
                    return 1;
                default:
                    return 0;
            }


        case 76:
            switch(n_id)
            {
                case 64: case 72: // case 77: //case 62: case 69: case 74: 
                    return 1;
                default:
                    return 0;
            }


        case 71:
            switch(n_id)
            {
                case 66: case 70:
                    return 1;
                default:
                    return 0;
            }


        case 70:
            switch(n_id)
            {
                case 71:
                    return 1;
                default:
                    return 0;
            }


        case 64:
            switch(n_id)
            {
               case 66: case 75: case 76:
                    return 1;
                default:
                    return 0;
            }


        case 66:
            switch(n_id)
            {
                case 64: case 71:
                    return 1;
                default:
                    return 0;
            }




        case 58:
            switch(n_id)
            {
                case 56: case 49:
                    return 1;
                default:
                    return 0;
            }

        case 55:
            switch(n_id)
            {
                case 49:
                    return 1;
                default:
                    return 0;
            }



        case 49:
            switch(n_id)
            {
                case 55: case 58: case 78:
                    return 1;
                default:
                    return 0;
            }




        case 12:
            switch(n_id)
            {
                case 1: //case 15: // case 54: case 9:
                    return 1;
                default:
                    return 0;
            }


        case 10:
            switch(n_id)
            {
                case 1: case 51: //case 9: 
                    return 1;
                default:
                    return 0;
            }


        case 51:
            switch(n_id)
            {
                case 1: case 56:
                    return 1;
                default:
                    return 0;
            }

        case 56:
            switch(n_id)
            {
                case 51: case 58:
                    return 1;
                default:
                    return 0;
            }



        case 9:
            switch(n_id)
            {
                case 1: //case 13: // case 12: case 10: case 13:
                    return 1;
                default:
                    return 0;
            }


        case 54:
            switch(n_id)
            {
                case 1: //case 58: //case 61:  //case 62: case 12: 
                    return 1;
                default:
                    return 0;
            }


        case 1:
            switch(n_id)
            {
                case 10: case 12: case 9: case 54:
                    return 1;
                default:
                    return 0;
            }

        default:
            return 0;

    }
}
#endif /*MOD_NEIGHBOURS && MOD_TYPE == 6*/


/*---------------------------------------------------------------------------*/
#if MOD_NEIGHBOURS && MOD_TYPE == 2
uint8_t 
check_mod_neighbours(uint16_t n_id)
{
    switch(my_addr)
    {
        case 42479:
            switch(n_id)
            {
                case 49144:
                    return 1;
                default:
                    return 0;
            }
        case 49144:
            switch(n_id)
            {
                case 49144:
                    return 6;
                case 42479: case 49334:
                    return 1;
                default:
                    return 0;
            }

        case 49334:
            switch(n_id)
            {
                case 49144: case 52492:
                    return 1;
                default:
                    return 0;
            }

        case 52492:
            switch(n_id)
            {
                case 52492:
                    return 6;
                case 49334: case 51598:
                    return 1;
                default:
                    return 0;
            }

        case 51598:
            switch(n_id)
            {
                case 52492:
                    return 1;
                default:
                    return 0;
            }

        default:
            return 0;
    }
}
#endif /*MOD_NEIGHBOURS && MOD_TYPE == 2*/



/*---------------------------------------------------------------------------*/
#if MOD_NEIGHBOURS && MOD_TYPE == 5
uint8_t 
check_mod_neighbours(uint16_t n_id)
{
    switch(my_addr)
    {

        case 49:
            switch(n_id)
            {
                case 78: 
                    return 1;
                default:
                    return 0;
            }

        case 78:
            switch(n_id)
            {
                case 72: //case 49:
                    return 1;
                default:
                    return 0;
            }


        case 72:
            switch(n_id)
            {
                case 75: //case 78:
                    return 1;
                default:
                    return 0;
            }


        case 75:
            switch(n_id)
            {
                case 65: //case 72: 
                    return 1;
                default:
                    return 0;
            }


        case 65:
            switch(n_id)
            {
                case 64: //case 75:
                    return 1;
                default:
                    return 0;
            }


        case 64:
            switch(n_id)
            {
                case 62:  //case 65:  //case 63: 
                    return 1;
                default:
                    return 0;
            }


        // case 76:
        //     switch(n_id)
        //     {
        //         case 64: case 69: //case 77:
        //             return 1;
        //         default:
        //             return 0;
        //     }


        // case 69:
        //     switch(n_id)
        //     {
        //         case 66: case 64:
        //             return 1;
        //         default:
        //             return 0;
        //     }

        // case 66:
        //     switch(n_id)
        //     {
        //         case 62: case 69: 
        //             return 1;
        //         default:
        //             return 0;
        //     }


        case 62:
            switch(n_id)
            {
                case 71:  //case 64: 
                    return 1;
                default:
                    return 0;
            }


        case 71:
            switch(n_id)
            {
                case 70: //case 62://case 74: 
                    return 1;
                default:
                    return 0;
            }


        case 70:
            switch(n_id)
            {
                case 71: //case 64:
                    return 1;
                default:
                    return 0;
            }


        

        // case 74:
        //     switch(n_id)
        //     {
        //         case 62: case 71:
        //             return 1;
        //         default:
        //             return 0;
        //     }


        


        

        

        // case 78:
            // switch(n_id)
            // {
            //     case 77: 
            //         return 1;
            //     default:
            //         return 0;
            // }



        // case 40:
        //     switch(n_id)
        //     {
        //         case 48: 
        //             return 1;
        //         default:
        //             return 0;
        //     }

        // case 48:
        //     switch(n_id)
        //     {
        //         case 40: case 44: 
        //             return 1;
        //         default:
        //             return 0;
        //     }

        // case 44:
        //     switch(n_id)
        //     {
        //         case 48: case 42: 
        //             return 1;
        //         default:
        //             return 0;
        //     }


        // case 42:
        //     switch(n_id)
        //     {
        //         case 44: case 45: 
        //             return 1;
        //         default:
        //             return 0;
        //     }

        // case 45:
        //     switch(n_id)
        //     {
        //         case 42: case 72:
        //             return 1;
        //         default:
        //             return 0;
        //     }



        // case 72:
        //     switch(n_id)
        //     {
        //         case 78: case 45: 
        //             return 1;
        //         default:
        //             return 0;
        //     }


        // case 78:
        //     switch(n_id)
        //     {
        //         case 49: case 72: 
        //             return 1;
        //         default:
        //             return 0;
        //     }


        // case 49:
        //     switch(n_id)
        //     {
        //         case 78: case 55:
        //             return 1;
        //         default:
        //             return 0;
        //     }


        // case 55:
        //     switch(n_id)
        //     {
        //         case 49: case 58:
        //             return 1;
        //         default:
        //             return 0;
        //     }


        // case 58:
        //     switch(n_id)
        //     {
        //         case 55: case 54: 
        //             return 1;
        //         default:
        //             return 0;
        //     }


        // case 54:
        //     switch(n_id)
        //     {
        //         case 58: case 1: 
        //             return 1;
        //         default:
        //             return 0;
        //     }


        // case 1:
        //     switch(n_id)
        //     {
        //         case 54: case 10: 
        //             return 1;
        //         default:
        //             return 0;
        //     }


        // case 10:
        //     switch(n_id)
        //     {
        //         case 1: case 51:
        //             return 1;
        //         default:
        //             return 0;
        //     }



        // case 51:
        //     switch(n_id)
        //     {
        //         case 10:  case 60:
        //             return 1;
        //         default:
        //             return 0;
        //     }


        // case 60:
        //     switch(n_id)
        //     {
        //         case 51: 
        //             return 1;
        //         default:
        //             return 0;
        //     }

         default:
            return 0;
    }
}
#endif /*MOD_NEIGHBOURS && MOD_TYPE == 5*/

/*---------------------------------------------------------------------------*/
#if MOD_NEIGHBOURS && MOD_TYPE == 3
uint8_t 
check_mod_neighbours(uint16_t n_id)
{
    switch(my_addr)
    {
        case 64:
            switch(n_id)
            {
                case 66: case 69: case 70: case 71: case 63: case 76:
                    return 1;
                default:
                    return 0;
            }

        case 74:
            switch(n_id)
            {
                case 60: case 62: case 75: case 65: case 66: case 69: case 76:
                    return 1;
                default:
                    return 0;
            }

        case 66:
            switch(n_id)
            {
                case 69: case 76: case 64: case 74:
                    return 1;
                default:
                    return 0;
            }

        
        case 69:
            switch(n_id)
            {
                case 66: case 76: case 64: case 74:
                    return 1;
                default:
                    return 0;
            }

        case 76:
            switch(n_id)
            {
                case 69: case 66: case 64: case 74:
                    return 1;
                default:
                    return 0;
            }

        case 75:
            switch(n_id)
            {
                case 74: 
                    return 1;
                default:
                    return 0;
            }


        case 70:
            switch(n_id)
            {
                case 64: case 69:
                    return 1;
                default:
                    return 0;
            }


        case 71:
            switch(n_id)
            {
                case 64: case 69:
                    return 1;
                default:
                    return 0;
            }

        

        case 60:
            switch(n_id)
            {
                case 74:
                    return 1;
                default:
                    return 0;
            }

        case 62:
            switch(n_id)
            {
                case 74:
                    return 1;
                default:
                    return 0;
            }


        case 63:
            switch(n_id)
            {
                case 64: case 78:
                    return 1;
                default:
                    return 0;
            }

        case 65:
            switch(n_id)
            {
                case 74:
                    return 1;
                default:
                    return 0;
            }


        case 78:
            switch(n_id)
            {
                case 63: case 77: case 72:
                    return 1;
                default:
                    return 0;
            }

        case 72:
            switch(n_id)
            {
                case 78:
                    return 1;
                default:
                    return 0;
            }
            
        case 77:
            switch(n_id)
            {
                case 78:
                    return 1;
                default:
                    return 0;
            }  


        default:
            return 0;
    }
}
#endif /*MOD_NEIGHBOURS && MOD_TYPE == 2*/

/*---------------------------------------------------------------------------*/

struct neighbour* add_to_neighbour(uint16_t addr, uint8_t state, uint8_t degree, timesync_frame_t *syncframe)
{
    struct neighbour *n;
    /* Check if we already know this neighbor. */
#if MOD_NEIGHBOURS

    
// #if TEST_FTSP
//     for(n = list_head(mod_neighbour_list); n != NULL; n = list_item_next(n))
//     {
//         if(n->addr == addr && check_mod_neighbours(n->addr))
//         {
//             if(my_addr != 70)
//                 ftsp_recv(syncframe, degree, addr);
//             return n;
//         }else {
//             continue;
//         }
//     }



//     for(n = list_head(neighbour_list); n != NULL; n = list_item_next(n))
//     {
//         if(n->addr == addr)
//         {
//             PRINTF("Calling FTSP recv 2\n");
//             if(my_addr != 70)
//                 ftsp_recv(syncframe, degree, addr);
//             return NULL;
//         }
//     }
// #else
    for(n = list_head(mod_neighbour_list); n != NULL; n = list_item_next(n))
    {
        if(n->addr == addr && check_mod_neighbours(n->addr))
        {
            gtsp_recv(n, syncframe, 0);

            
            if(my_state == IDLE)
            {
                return n;
            }

            if(state == DISC_TO_EREV || state == CONVERGENCE_PROACTIVE)
            {
                state--;
            }            
            n->state = state;
            if((my_state < CONSENSUS_SYNCHRONIZATION && state == my_state) || my_state == DISCOVERY)
            {
#if TEST_FTSP
                ftsp_update_rtimer(n);
#else
                gtsp_update_rtimer(mod_neighbour_list); 
#endif
            }
            // For local center synchronization
            else if((my_state == CONSENSUS_SYNCHRONIZATION) && (my_cluster.role == CH) && (my_cons_slot == this_sync_slot))
            {

            }
            
            if(my_state < CONNECTION_DECLARATION)
            {
                n->degree = degree;
            }
            return n;
        }
    }


    for(n = list_head(neighbour_list); n != NULL; n = list_item_next(n))
    {
        if(n->addr == addr)
        {
            gtsp_recv(n, syncframe, 0);
            return NULL;
        }
    }
//#endif
#else /*MOD_NEIGHBOURS*/
    for(n = list_head(neighbour_list); n != NULL; n = list_item_next(n)) 
    {
        if(n->addr == addr)
        {
            gtsp_recv(n, syncframe, 0);

            if(my_state == IDLE)
            {
                return n;
            }
            
            if(state == DISC_TO_EREV || state == CONVERGENCE_PROACTIVE)
            {
                state--;
            }  
            n->state = state;
            if((my_state < CONSENSUS_SYNCHRONIZATION && state == my_state) || my_state == DISCOVERY)
            {
                gtsp_update_rtimer(neighbour_list);
            }

            if(my_state < CONNECTION_DECLARATION)
            {
                n->degree = degree;
            }
            return n;
        }
    }
#endif /*MOD_NEIGHBOURS*/ 


    if(n == NULL)
    {
        n = memb_alloc(&neighbour_memb);
        /* If we could not allocate a new neighbor entry, we give up */
        if(n == NULL) 
        {
          return NULL;
        }
        n->addr = addr;

#if TEST_FTSP
#else
        if(my_state < CONNECTION_DECLARATION)
        {
            n->degree = degree;
        }
        n->role = CM;
        n->state = state;
#endif
        
#if MOD_NEIGHBOURS
// #if TEST_FTSP
//         if(check_mod_neighbours(n->addr)) 
//             list_add(mod_neighbour_list, n);
//         ftsp_recv(syncframe, degree, addr);

// #else
        if(check_mod_neighbours(n->addr))
        {
            gtsp_recv(n, syncframe, 1);
            my_degree++;
            list_add(mod_neighbour_list, n);
            announcement_set_degree(&discovery_announcement, my_degree);
        }
        else
        {
            gtsp_recv(n, syncframe, 1);
            list_add(neighbour_list, n);
            return NULL;
        }
//#endif
#else /*MOD_NEIGHBOURS*/
        gtsp_recv(n, syncframe, 1);
        my_degree++;
        list_add(neighbour_list, n);
        announcement_set_degree(&discovery_announcement, my_degree);
#endif /*MOD_NEIGHBOURS*/
    }
    return n;
    
}


void
update_neighbour_role(uint16_t addr, role_t role_sender)
{
    struct neighbour *k;
    #if MOD_NEIGHBOURS
    for(k = list_head(mod_neighbour_list); k != NULL; k = list_item_next(k))
    {
        if(k->addr == addr && check_mod_neighbours(k->addr))
        {
            if(role_sender == 1)
                k->role = 1;
            else if(role_sender == 2)
                k->role = 2;
        }
    }
    #else
    for(k = list_head(neighbour_list); k != NULL; k = list_item_next(k))
    {
      if(k->addr == addr)
        {
            if(role_sender == 1)
                k->role = 1;
            else if(role_sender == 2)
                k->role = 2;
        }
    }
    #endif
}


/*---------------------------------------------------------------------------*/
void 
csync_update_placing(void)
{
    struct neighbour *n;
    my_placing = 0;

#if MOD_NEIGHBOURS
    for(n = list_head(mod_neighbour_list); n != NULL; n = list_item_next(n))
    {
        if(n->degree > my_degree)
        {
            my_placing += (n->degree - my_degree);
        }
        else if(n->degree == my_degree && n->addr > my_addr)
        {
            my_placing++;
        }
    }

#else /*MOD_NEIGHBOURS*/
    for(n = list_head(neighbour_list); n != NULL; n = list_item_next(n)) 
    {
        if(n->degree > my_degree)
        {
            my_placing += (n->degree - my_degree);
        }
        else if(n->degree == my_degree && n->addr > my_addr)
        {
            my_placing++;
        }
    }
#endif /*MOD_NEIGHBOURS*/ 
    //PRINTF("\nmy_placing %u", my_placing);
}


/*---------------------------------------------------------------------------*/
uint8_t 
csync_CHB_placing(void)
{
    uint8_t placing = 0;

    for(ch = list_head(*my_cluster.CHs_list); ch != NULL; ch = list_item_next(ch))
    {
        if(ch->degree > my_degree && my_cluster.role == CB)
        {
            placing += (ch->degree - my_degree);
        }
        else if(ch->degree == my_degree && ch->addr > my_addr)
        {
            placing++;
        }
    }
    return placing;
}

/*---------------------------------------------------------------------------*/
void
csync_trusted_synchronization(neighbour_t *n, uint16_t c_addr, double cons_rate)
{
    neighbour_t *nn;
    struct CHB *ch;
    int32_t n_fine_diff = n->fine_diff;

    #if AVG_CONSENSUS
    #else
    double curr_rate = rtimer_avg_rate(), new_rate = 0, rate_update = 0;
    #endif
    
    //PRINTF("relative rate = %lu, avg_rate = %lu", n->relative_rate * 1000, curr_rate * 1000);

    if(my_cluster.role == CM)
    {
        ch = list_head(*my_cluster.CHs_list);
        if(n->addr != ch->addr)
        {
            return;
        }
        if(my_cons_slot == this_sync_slot)
        {
            #if AVG_CONSENSUS

            #else
            // new_rate = ((double)n->relative_rate/(double)curr_rate);
            // my_cons_rate = (double) (new_rate * cons_rate);
            // if(!my_cons_rate)
            //     my_cons_rate = 1.0;
            cons_rate *= (double)n->relative_rate;
            rtimer_set_avg_rate(cons_rate);
            #endif
        }
        
    }
    else if(my_cluster.role == CB)
    {
        for(ch = list_head(*my_cluster.CBs_list); ch != NULL; ch = list_item_next(ch))
        {
            if(n->addr == ch->addr)
            {
                return;
            }
        }
        if(my_cons_slot > this_sync_slot)
        {
            #if AVG_CONSENSUS
            #else
            new_rate = ((double)n->relative_rate/(double)curr_rate);
            my_cons_rate = (double) (new_rate * cons_rate);
            if(!my_cons_rate)
                my_cons_rate = 1.0;
            cons_rate *= (double)n->relative_rate;
            rtimer_set_avg_rate(cons_rate);
            #endif
        }
    }
    else if(my_cluster.role == CH)
    {
        for(ch = list_head(*my_cluster.CBs_list); ch != NULL; ch = list_item_next(ch))
        {
            if(n->addr != ch->addr)
            {
                return;
            }
        }

        #if AVG_CONSENSUS
        #else
        new_rate = ((double)n->relative_rate/(double)curr_rate);
        my_cons_rate = (double) (new_rate * cons_rate);
        if(!my_cons_rate)
            my_cons_rate = 1.0;
        cons_rate *= (double)n->relative_rate;
        rtimer_set_avg_rate(cons_rate);
        #endif
        if(my_cons_slot == this_sync_slot)
        {
            rtimer_set_avg_rate(cons_rate);
        }
        // Synchronizing local center
        else if ((sync_LC_addr != my_addr) && (sync_LC_addr == c_addr))
        {
            rate_update = (cons_rate + curr_rate)/2;
            rtimer_set_avg_rate(rate_update);
        }
        else
        {
            return;
        }
    }

    // rtimer_set_avg_rate(n->relative_rate); 
    rtimer_adjust_fine_offset(n_fine_diff);

#if MOD_NEIGHBOURS
    for(nn = list_head(mod_neighbour_list); nn != NULL; nn = list_item_next(nn))
    {
        nn->fine_diff -=  n_fine_diff;
    }
#else /*MOD_NEIGHBOURS*/ 
    for(nn = list_head(neighbour_list); nn != NULL; nn = list_item_next(nn))
    {
        nn->fine_diff -=  n_fine_diff;
    }
#endif /*MOD_NEIGHBOURS*/ 
    
    PRINTF(", sync C %u, N %u @ %ld", c_addr, n->addr, n_fine_diff);
}