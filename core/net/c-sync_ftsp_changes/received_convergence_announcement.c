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
received_convergence_announcement(struct announcement *a, const linkaddr_t *from,
            uint16_t id, struct announcement_value *a_value, timesync_frame_t *syncframe, annstate_t last_event)
{
    

    struct neighbour *n;
    struct CHB *ch;

    if(packetbuf_attr(PACKETBUF_ATTR_RSSI) >= RSSI_THRESHOLD)
    {
        
        n = add_to_neighbour(from->u16, a_value->instr, a_value->degree, syncframe);
        if(n == NULL || !n->synced)
        {
            return;
        }

        
        /* Clustering starts here */
        switch(my_state)
        {

            case DISCOVERY: 
            break;

            case ELECTION_REVELATION:
            break;

            case ELECTION_DECLARATION:
            break;

            case CONNECTION_REVELATION:
            break;
            
            case CONNECTION_DECLARATION:
              if(a_value->instr == CONSENSUS_CONVERGENCE)
              {
                polite_announcement_cancel();

                if(rtimer_schedule(RTIMER_0, RTIMER_DATE, a_value->date_coarse, a_value->date_fine, enter_consensus_revelation))
                {
                  rt[RTIMER_0].state = RTIMER_SINGLEPASS;
                  if(my_cluster.role == CH)
                  {
                    my_cluster.role = CM;
                    list_init(*my_cluster.CBs_list);
                    handle_lists(a, a_value, n);
                  }
                  enter_convergence(rt);
                }
              }
            break;

            case CONSENSUS_CONVERGENCE:
              if(a_value->instr == my_state || a_value->instr == CONVERGENCE_PROACTIVE)
              {
                role_t role_sender = CM;
                for(ch = list_head(*my_cluster.CHs_list); ch != NULL; ch = list_item_next(ch))
                {
                  if(ch->addr == n->addr)
                  {
                    role_sender = CH;
                    n->role = 1;
                    update_neighbour_role(n->addr, 1);
                  }
                }
                for(ch = list_head(*my_cluster.CBs_list); ch != NULL; ch = list_item_next(ch))
                {
                  if(ch->addr == n->addr)
                  {
                    role_sender = CB;
                    n->role = 2;
                    update_neighbour_role(n->addr, 2);
                  }
                }


                if(!role_sender)
                {
                  if(my_cluster.role == CH && list_length(*my_cluster.CBs_list) == 0)
                  { 
                    handle_lists(a, a_value, n);
                  }
                  return;
                }

                if(my_cluster.role == CB)
                {
                  for(ch = list_head(*my_cluster.CHs_list); ch != NULL; ch = list_item_next(ch))
                  {
                    if(ch->addr == n->addr)
                    {
                      if(ch->cons_slot == 0 && my_cons_slot == a_value->degree)
                      {
                        if(!my_slot_ack)
                        {
                          my_slot_ack = 1;
                          ch->cons_slot = a_value->degree;
                          announcement_set_instr(a, a_value->instr);
                          announcement_set_degree(a, a_value->degree);
                          announcement_set_ref_addr(a, n->addr);
                          announcement_bump(a);
                          //PRINTF("\nBUMP %u", n->addr);
                        }
                      }
                    }
                  }
                }
                else if(my_cluster.role == CH)
                {
                  if(a_value->ref_addr == my_addr)
                  {
                    my_cons_slot = a_value->degree;
                    my_slot_ack = 1;
                    return;
                  }

                  polite_announcement_cancel();
                  for(ch = list_head(*my_cluster.CHs_list); ch != NULL; ch = list_item_next(ch))
                  {
                    if(ch->addr == a_value->ref_addr)
                    {
                      ch->cons_slot = a_value->degree;
                    }
                  }
                }
              }

              // if(a_value->instr == CONVERGENCE_PROACTIVE)
              // {
              //   if(my_cluster.role == CB)
              //   {

              //   }
              //   else if(my_cluster.role == CH)
              //   {

              //   }
              // }

            break;

            case CONSENSUS_REVELATION:
            break;

            case CONSENSUS_SYNCHRONIZATION:
              if(a_value->instr == CONSENSUS_CONVERGENCE)
              {
                polite_announcement_cancel();

                PRINTF("\n%u: received_convergence_announcement from %u with: instr %u, degree %u, date_coarse %lu, date_fine %lu, ref_addr %u",
                    linkaddr_node_addr.u16, from->u16, a_value->instr, a_value->degree, a_value->date_coarse, a_value->date_fine, a_value->ref_addr);
                my_sync_border = 0; // To prevent byzantine consensus being initiated
                if(rtimer_schedule(RTIMER_0, RTIMER_DATE, a_value->date_coarse, a_value->date_fine, enter_consensus_revelation))
                {
                  rt[RTIMER_0].state = RTIMER_SINGLEPASS;
                  ref_n_CHB_degree = NUM_CONS_SLOTS + 3;
                  if(my_cluster.role == CH)
                  {
                    my_cluster.role = CM;
                    list_init(*my_cluster.CBs_list);
                    handle_lists(a, a_value, n);
                  }
                  enter_discovery(rt);
                }
              }
            break;

            default:
            break;
        }
    }
}