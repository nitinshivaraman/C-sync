#include "net/c-sync/c-sync.h"

#define DEBUG 1
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif




/*---------------------------------------------------------------------------*/
void
received_synchronization_announcement(struct announcement *a, const linkaddr_t *from,
            uint16_t id, struct announcement_value *a_value, timesync_frame_t *syncframe, annstate_t last_event)
{
    

    struct neighbour *n;
    struct CHB *ch;

    list_t *temp_val;
    struct memb *temp_memb;

    list_t *bl_list;
    struct BL *bl_new;
    uint16_t byzantine_threshold = 0;

    /* Byzantine agreement is done if >50% correct msgs received */
    byzantine_threshold = my_degree/2 + 1;

    if(packetbuf_attr(PACKETBUF_ATTR_RSSI) >= RSSI_THRESHOLD)
    {
        /* Bridge message bounced back */
        if(a_value->ref_addr == my_addr)
          return;
      
        n = add_to_neighbour(from->u16, a_value->instr, a_value->degree, syncframe);
        if(n == NULL || !n->synced)
        {
            return;
        }
         PRINTF("\n%u: received_synchronization_announcement from %u with: instr %u, degree %u, date_coarse %lu, date_fine %lu, ref_addr %u",
             linkaddr_node_addr.u16, from->u16, a_value->instr, a_value->degree, a_value->date_coarse, a_value->date_fine, a_value->ref_addr);

        /* Clustering starts here */
        switch(my_state)
        {

            case DISCOVERY: 
              my_cluster.role = CM;
              broadcast_announcement_stop();
              announcement_remove_value(&discovery_announcement);
              polite_announcement_init(LOGICAL_CHANNEL, 0, PA_RESILIENCE_MAX_SEND_DUPS, PA_REGULAR_MAX_RECV_DUPS);
              if(rtimer_schedule(RTIMER_0, RTIMER_DATE, a_value->date_coarse, a_value->date_fine, enter_idle))
              {
                rt[RTIMER_0].state = RTIMER_SINGLEPASS;
                if(list_length(*my_cluster.CHs_list) > 0)
                {
                  temp_val = my_cluster.CHs_list;
                  temp_memb = my_cluster.m_CH;
                  ch = memb_alloc(temp_memb);
                  ch->n_CHB_addr_A = a_value->ref_addr;
                  ch->cons_slot = a_value->degree;
                  list_push(*temp_val, temp_memb);
                    
                  my_cons_slot = a_value->degree;
                  csync_trusted_synchronization(n, a_value->ref_addr, a_value->cons_rate);
                  my_sync_border = 0;
                  announcement_set_degree(&synchronization_announcement, a_value->degree);
                  announcement_set_date_coarse(&synchronization_announcement, a_value->date_coarse);
                  announcement_set_date_fine(&synchronization_announcement, a_value->date_fine);
                  announcement_set_ref_addr(&synchronization_announcement, a_value->ref_addr);
                }
              }
            break;

            case ELECTION_REVELATION:
            break;

            case ELECTION_DECLARATION:
            break;

            case CONNECTION_REVELATION:
            break;

            case CONSENSUS_CONVERGENCE:
            break;

            case CONSENSUS_REVELATION:
              if(a_value->instr == CONSENSUS_SYNCHRONIZATION)
              {
                polite_announcement_cancel();
                if(rtimer_schedule(RTIMER_0, RTIMER_DATE, a_value->date_coarse, a_value->date_fine, enter_idle))
                {
                  rt[RTIMER_0].state = RTIMER_SINGLEPASS;
                  if(my_cluster.role == CH && list_length(*my_cluster.CBs_list) == 0)
                  { 
                    handle_lists(a, a_value, n);
                    for(ch = list_head(*my_cluster.CHs_list); ch != NULL; ch = list_item_next(ch))
                    {
                      if(ch->addr == n->addr)
                      {
                        ch->cons_slot = a_value->degree;
                      }
                    }
                  }
                  enter_consensus_synchronization(rt);
                }
              }
            break;

            case CONSENSUS_SYNCHRONIZATION:
            //PRINTF("This slot is %d and my_cons_slot is %d\n", this_sync_slot, my_cons_slot);
              if(a_value->instr == my_state)
              {
                for(ch = list_head(*my_cluster.blacklist); ch != NULL; ch = list_item_next(ch))
                {
                  if(ch->addr == n->addr)
                  {
                    return;
                  }
                }

                if(my_cluster.role == CB)
                {
                  //PRINTF("\nThis_sync_slot as %d and my_cons_slot as %d\n", this_sync_slot, my_cons_slot);
                  if(my_cons_slot > this_sync_slot)
                  {
                    if((n->fine_diff < BYZANTINE_FINE_DIFF) && (n->coarse_diff < BYZANTINE_COARSE_DIFF))
                    {
                      PRINTF("Synchonizing to CH %d and fine diff is %ld\n", a_value->ref_addr, n->fine_diff);
                      csync_trusted_synchronization(n, a_value->ref_addr, a_value->cons_rate);
                      my_sync_border = 1;
                    }
                    else
                    {
                      bl_list = my_cluster.blacklist;
                      temp_memb = my_cluster.m_bl;

                      PRINTF("Setting my_sync_border to malicious node\n");

                      bl_new = memb_alloc(temp_memb);
                      bl_new->addr = n->addr;
                      list_push(*bl_list, bl_new);
                      my_sync_border = 1;
                      enter_byzantine_consensus(rt);
                      return;
                    }
                  }
                  else if(my_cons_slot == this_sync_slot)
                  {
                    PRINTF("Fine diff is %ld and max diff %d", n->fine_diff, BYZANTINE_FINE_DIFF);
                    if((n->fine_diff > BYZANTINE_FINE_DIFF) || (n->coarse_diff > BYZANTINE_COARSE_DIFF))
                    {
                      bl_list = my_cluster.blacklist;
                      temp_memb = my_cluster.m_bl;

                      PRINTF("Setting my_sync_border to faulty node\n");

                      bl_new = memb_alloc(temp_memb);
                      bl_new->addr = n->addr;
                      list_push(*bl_list, bl_new);
                      my_sync_border = 1;
                      enter_byzantine_consensus(rt);
                      return;
                    }
                    // }
                    // /* Didn't receive a message from CH -> Start byzantine consensus */
                    // else if(n-)
                    // {
                      // for(ch = list_head(*my_cluster.CHs_list); ch != NULL; ch = list_item_next(ch))
                      // {
                      //   if(ch->addr != n->addr)
                      //   {
                      //     bl_list = my_cluster.blacklist;
                      //     temp_memb = my_cluster.m_bl;

                      //     PRINTF("Setting my_sync_border to 2 here 2222222\n");

                      //     bl_new = memb_alloc(temp_memb);
                      //     bl_new->addr = n->addr;
                      //     list_push(*bl_list, bl_new);
                      //     my_sync_border = 1;
                      //     enter_byzantine_consensus(rt);                        
                      //   }
                        
                      // }
                    // }
                    else
                    {
                        /* normal message received */
                      polite_announcement_cancel();
                      my_sync_border = 1;
                    }
                  }
                  announcement_set_degree(&synchronization_announcement, a_value->degree);
                  announcement_set_date_coarse(&synchronization_announcement, a_value->date_coarse);
                  announcement_set_date_fine(&synchronization_announcement, a_value->date_fine);
                  announcement_set_ref_addr(&synchronization_announcement, a_value->ref_addr);    
                }
                else if((my_cluster.role == CH))
                {
                  announcement_set_date_coarse(&synchronization_announcement, a_value->date_coarse);
                  announcement_set_date_fine(&synchronization_announcement, a_value->date_fine);
                  announcement_set_ref_addr(&synchronization_announcement, a_value->ref_addr);
                  if(this_sync_slot == my_cons_slot && !my_sync_border)  //&& (a_value->ref_addr != my_addr)
                  {
                    PRINTF("Synch for CH to be done\n");
                    csync_trusted_synchronization(n, a_value->ref_addr, a_value->cons_rate);
                    announcement_bump(&synchronization_announcement);
                    my_sync_border = 1;
                  }
                  // Synchronization between local centers
                  else
                  {
                    csync_trusted_synchronization(n, a_value->ref_addr, a_value->cons_rate);
                  }
                }
                else if((my_cluster.role == CM) && (this_sync_slot == my_cons_slot)) //&& (a_value->ref_addr != my_addr)
                {
                  announcement_set_date_coarse(&synchronization_announcement, a_value->date_coarse);
                  announcement_set_date_fine(&synchronization_announcement, a_value->date_fine);
                  announcement_set_ref_addr(&synchronization_announcement, a_value->ref_addr);
                  csync_trusted_synchronization(n, a_value->ref_addr, a_value->cons_rate);
                }
              }
              /**************************NEW**************************/
              else if(a_value->instr == BYZANTINE_CONSENSUS)
              {
                for(ch = list_head(*my_cluster.blacklist); ch != NULL; ch = list_item_next(ch))
                {
                  if(ch->addr == n->addr)
                  {
                    return;
                  }
                }

                PRINTF("Fine diff is %ld and max diff %d\n", n->fine_diff, BYZANTINE_FINE_DIFF);

                if((n->fine_diff > BYZANTINE_FINE_DIFF) || (n->coarse_diff > BYZANTINE_COARSE_DIFF))
                {
                  if(my_cons_slot == this_sync_slot)
                  {
                    msg_count++;
                    PRINTF("Synchronizing with %d\n", a_value->ref_addr);
                    csync_trusted_synchronization(n, a_value->ref_addr, a_value->cons_rate);
                    announcement_set_degree(&synchronization_announcement, a_value->degree);
                    announcement_set_date_coarse(&synchronization_announcement, a_value->date_coarse);
                    announcement_set_date_fine(&synchronization_announcement, a_value->date_fine);
                    announcement_set_ref_addr(&synchronization_announcement, a_value->ref_addr);
                    my_sync_border = 1;
                    if(msg_count > byzantine_threshold)
                    {
                      polite_announcement_cancel();
                      my_sync_border = 0;
                    }
                    else
                    {
                      my_sync_border = 1;
                      enter_byzantine_consensus(rt);
                      // announcement_set_instr(&synchronization_announcement, BYZANTINE_CONSENSUS);
                      // announcement_bump(&synchronization_announcement);
                    }
                  }
                  /* CB of the next scheduled cluster */
                  else
                  {
                    csync_trusted_synchronization(n, a_value->ref_addr, a_value->cons_rate);
                    announcement_set_degree(&synchronization_announcement, a_value->degree);
                    announcement_set_date_coarse(&synchronization_announcement, a_value->date_coarse);
                    announcement_set_date_fine(&synchronization_announcement, a_value->date_fine);
                    announcement_set_ref_addr(&synchronization_announcement, a_value->ref_addr);
                    my_sync_border = 1;
                  }
                }
                
              }

            break;

            default:
            break;
        }
    }
}



