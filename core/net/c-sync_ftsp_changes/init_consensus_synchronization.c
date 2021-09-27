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

void init_consensus_synchronization(void)
{
	struct CHB *ch;

    my_cons_slot = (NUM_CONS_SLOTS + 1) - my_cons_slot;
    for(ch = list_head(*my_cluster.CHs_list); ch != NULL; ch = list_item_next(ch))
    {
        ch->cons_slot = (NUM_CONS_SLOTS + 1) - ch->cons_slot;
    }

    if(my_cluster.role == CH)
    {
        ref_n_CHB_degree = my_cons_slot;
    }
    else
    {
        ref_n_CHB_degree = NUM_CONS_SLOTS;
    }
    ref_n_CHB_addr = my_addr;
    

    if(my_cluster.role == CH)
    {
        my_sync_border = 1;
    }

    for(ch = list_head(*my_cluster.CHs_list); ch != NULL; ch = list_item_next(ch))
    {
        if(ch->cons_slot < ref_n_CHB_degree)
        {
            ref_n_CHB_addr = ch->addr;
            ref_n_CHB_degree = ch->cons_slot;
            my_sync_border = 0;
        }
        else if(ch->cons_slot > NUM_CONS_SLOTS)
        {
            my_sync_border = 0;
        }
    }

    if(my_cluster.role == CB)
    {
        for(ch = list_head(*my_cluster.CHs_list); ch != NULL; ch = list_item_next(ch))
        {
            if(ch->addr != ref_n_CHB_addr)
            {
                my_cons_slot = ch->cons_slot;
            }
        }
    }
}

void update_consensus_synchronization(void)
{
    struct CHB *ch;

    for(ch = list_head(*my_cluster.CHs_list); ch != NULL; ch = list_item_next(ch))
    {
        if(ch->addr == ref_n_CHB_addr)
        {
            break;
        }
        my_cons_slot = ch->cons_slot;
    }
}
