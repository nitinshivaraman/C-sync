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

uint8_t handle_lists(struct announcement *a, struct announcement_value *a_value, struct neighbour *n)
{
    list_t *handle_list;
    struct memb *handle_memb;

    if(my_state == ELECTION_DECLARATION || my_state == CONSENSUS_CONVERGENCE)
    {
        handle_list = my_cluster.CHs_list;
        handle_memb = my_cluster.m_CH;
    }
    else if(my_state == CONNECTION_DECLARATION)
    {
        handle_list = my_cluster.CBs_list;
        handle_memb = my_cluster.m_CB;
    }
    else
    {
        return 0;
    }
    struct CHB *ch;

    if(list_length(*handle_list) == 0)
    {
        ch = memb_alloc(handle_memb);
        if(ch == NULL)
        {
            return 0;
        }

        // // For False CH nodes
        // if((!check_mod_neighbours(n->addr)) || (n->degree > 20))
        //     return 0;

        if(my_state == ELECTION_DECLARATION || my_state == CONSENSUS_CONVERGENCE)
        {
            my_cluster.role = CM;
        }

        ch->addr = n->addr;
        ch->cons_slot = 0;
        if(my_cluster.role == CH)
        {
            ch->n_CHB_addr_A = a_value->ref_addr - my_addr;
            ch->degree = a_value->degree - my_degree;
        }
        else
        {
            ch->n_CHB_addr_A = a_value->ref_addr;
            ch->degree = n->degree;
        }
        ch->n_CHB_addr_B = 0;

        list_push(*handle_list, ch);

        return 1;
    }

    uint8_t position = 1;
    for(ch = list_head(*handle_list); ch != NULL; ch = list_item_next(ch))
    {
        if(ch->addr == n->addr)
        {
            return 1;
        }
        else if(n->degree > ch->degree || (n->degree == ch->degree && n->addr > ch->addr))
        {
            break;
        }
        position++;
    }


    if(list_length(*handle_list) < NUM_CH_MAX)
    {
        ch = memb_alloc(handle_memb);
    }
    else if(ch != NULL && list_length(*handle_list) == NUM_CH_MAX)
    {
        ch = list_chop(*handle_list);
        memb_free(handle_memb, ch);
        ch = memb_alloc(handle_memb);
    }
    else
    {
        return 1;
    }

    if(ch == NULL)
    {
        return 1;
    }



    ch->addr = n->addr;
    ch->cons_slot = 0;
    if(my_cluster.role == CH)
    {
        ch->n_CHB_addr_A = a_value->ref_addr - my_addr;
        ch->degree = a_value->degree - my_degree;
    }
    else
    {
        ch->n_CHB_addr_A = a_value->ref_addr;
        ch->degree = n->degree;
    }
    ch->n_CHB_addr_B = 0;

    if(position == 1)
    {
        list_push(*handle_list, ch);
    }
    else
    {
        struct CH *chch;
        for(chch = list_head(*handle_list); chch != NULL; chch = list_item_next(chch))
        {
            if(position <= 2)
            {
                break;
            }
            position--;
        }
        if(chch != NULL)
        {
            list_insert(*handle_list, chch, ch);
        }
    }
    return 1;
}