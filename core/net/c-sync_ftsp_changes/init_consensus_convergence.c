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

void init_consensus_convergence(void)
{
	struct CHB *ch;
	struct CHB *chch;

    memb_init(my_cluster.m_CH);
	list_init(*my_cluster.CHs_list);

    for(ch = list_head(*my_cluster.CBs_list); ch != NULL; ch = list_item_next(ch))
    {
        for(chch = list_head(*my_cluster.CHs_list); chch != NULL; chch = list_item_next(chch))
        {
            if(ch->n_CHB_addr_A == chch->addr)
            {
            	chch->n_CHB_addr_B = ch->addr;
            	break;
            }
        }

        if(chch == NULL)
        {
		    chch = memb_alloc(my_cluster.m_CH);
		    if(chch == NULL)
		    {
		        return;
		    }
		    chch->addr = ch->n_CHB_addr_A;
		    chch->degree = ch->degree;
		    chch->cons_slot = 0;
		    chch->n_CHB_addr_A = ch->addr;
		    chch->n_CHB_addr_B = 0;

		    list_push(*my_cluster.CHs_list, chch);
        }
    }

    switch(list_length(*my_cluster.CHs_list))
    {
        case 0: case 1:
            my_proactive_slot = 0;
        break;
        case 2:
            my_proactive_slot = 1;
        break;
        case 3:
            my_proactive_slot = 3;
        break;
        default:
            my_proactive_slot = 3;
        break;
    }
    announcement_set_instr(&convergence_announcement, my_state);
    

}