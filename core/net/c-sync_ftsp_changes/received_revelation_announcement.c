#include "net/c-sync/c-sync.h"

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

/*---------------------------------------------------------------------------*/
void
received_revelation_announcement(struct announcement *a, const linkaddr_t *from,
            uint16_t id, struct announcement_value *a_value, timesync_frame_t *syncframe, annstate_t last_event)
{

    struct neighbour *n;
    struct CHB *ch;
    
    list_t *temp_val;
    struct memb *temp_memb;

    //PRINTF("\nPACKETBUF_ATTR_RSSI %d", packetbuf_attr(PACKETBUF_ATTR_RSSI));
    if(packetbuf_attr(PACKETBUF_ATTR_RSSI) >= RSSI_THRESHOLD)
    {

        n = add_to_neighbour(from->u16, a_value->instr, a_value->degree, syncframe);
        if(n == NULL|| !n->synced)
        {
            return;
        }
        
        if((my_addr == 65) || (my_addr ==71) || (my_addr == 75))
        PRINTF("\n%u: received_revelation_announcement from %u with: instr %u, degree %u, date_coarse %lu, date_fine %lu, ref_addr %u",
          linkaddr_node_addr.u16, from->u16, a_value->instr, a_value->degree, a_value->date_coarse, a_value->date_fine, a_value->ref_addr);
        
        /* Clustering starts here */
        switch(my_state)
        {

            case DISCOVERY:
            //   if(a_value->instr == ELECTION_REVELATION)
            //   {
            //     polite_announcement_cancel();
            //     PRINTF("\n%u: received_revelation_announcement from %u with: instr %u, degree %u, date_coarse %lu, date_fine %lu, ref_addr %u",
            //         linkaddr_node_addr.u16, from->u16, a_value->instr, a_value->degree, a_value->date_coarse, a_value->date_fine, a_value->ref_addr);
            //     if(rtimer_schedule(RTIMER_0, RTIMER_DATE, a_value->date_coarse, a_value->date_fine, enter_election_declaration))
            //     {
            //       rt[RTIMER_0].state = RTIMER_SINGLEPASS;
            //       if(n->degree > my_degree || (n->degree == my_degree && n->addr > my_addr))
            //       {
            //           if(rtimer_schedule(RTIMER_0, RTIMER_DATE, a_value->date_coarse, a_value->date_fine, enter_election_declaration))
            //           {
            //               announcement_set_date_coarse(a, a_value->date_coarse);
            //               announcement_set_date_fine(a, a_value->date_fine);
            //           }
            //       }
            //       enter_election_revelation(rt);
            //     }
            //   }
              if(a_value->instr == ELECTION_REVELATION)
              {
                //PRINTF("\n%u: received_revelation_announcement from %u with: instr %u, degree %u, date_coarse %lu, date_fine %lu, ref_addr %u",
                //linkaddr_node_addr.u16, from->u16, a_value->instr, a_value->degree, a_value->date_coarse, a_value->date_fine, a_value->ref_addr);
                polite_announcement_cancel();
                if(n->degree > my_degree || (n->degree == my_degree && n->addr > my_addr))
                {
                  if(rtimer_schedule(RTIMER_0, RTIMER_DATE, a_value->date_coarse, a_value->date_fine, enter_connection_revelation))
                  {
                    rt[RTIMER_0].state = RTIMER_SINGLEPASS;
                    enter_election_revelation(rt);
                  }
                }
              }
              else if(a_value->instr == CONNECTION_REVELATION)
              {
                my_cluster.role = CM;
                if(rtimer_schedule(RTIMER_0, RTIMER_DATE, a_value->date_coarse, a_value->date_fine, enter_consensus_synchronization))
                {
                  rt[RTIMER_0].state = RTIMER_SINGLEPASS;
                  broadcast_announcement_stop();
                  announcement_remove_value(&discovery_announcement);
                  polite_announcement_init(LOGICAL_CHANNEL, 0, PA_RESILIENCE_MAX_SEND_DUPS, PA_REGULAR_MAX_RECV_DUPS);
                  enter_connection_revelation(rt);
                }
              }
              else if(a_value->instr == CONSENSUS_REVELATION)
              {
                my_cluster.role = CM;
                if(rtimer_schedule(RTIMER_0, RTIMER_DATE, a_value->date_coarse, a_value->date_fine, enter_consensus_synchronization))
                {
                  rt[RTIMER_0].state = RTIMER_SINGLEPASS;
                  broadcast_announcement_stop();
                  announcement_remove_value(&discovery_announcement);
                  polite_announcement_init(LOGICAL_CHANNEL, 0, PA_RESILIENCE_MAX_SEND_DUPS, PA_REGULAR_MAX_RECV_DUPS);
                  if(list_length(*my_cluster.CHs_list) > 0)
                  {
                    for(ch = list_head(*my_cluster.CHs_list); ch != NULL; ch = list_item_next(ch))
                    {
                      if(ch->addr == n->addr)
                      {
                        ch->cons_slot = a_value->degree;
                        my_cons_slot = a_value->degree;
                      }
                    }
                  }
                  else
                  {
                    temp_val = my_cluster.CHs_list;
                    temp_memb = my_cluster.m_CH;
                    ch = memb_alloc(temp_memb);
                    ch->n_CHB_addr_A = a_value->ref_addr;
                    ch->cons_slot = a_value->degree;
                    list_push(*temp_val, temp_memb);
                    
                    my_cons_slot = a_value->degree;
                  }
                  enter_consensus_revelation(rt);
                }
              }
                  

            break;

            case ELECTION_REVELATION:
                if(a_value->instr == my_state)
                {
                  if(n->degree > my_degree || (n->degree == my_degree && n->addr > my_addr))
                  {
                    polite_announcement_cancel();
                  }
                }
            break;

            case ELECTION_DECLARATION:
            break;

            case CONNECTION_REVELATION:
                // PRINTF("\n%u: received_revelation_announcement from %u with: instr %u, degree %u, date_coarse %lu, date_fine %lu, ref_addr %u",
                //   linkaddr_node_addr.u16, from->u16, a_value->instr, a_value->degree, a_value->date_coarse, a_value->date_fine, a_value->ref_addr);
                if(a_value->instr == my_state && my_cluster.role == CB)
                {
                  if(n->degree > my_degree || (n->degree == my_degree && n->addr > my_addr))
                  {
                    handle_lists(a, a_value, n);
                  }
                }
            break;

            case CONSENSUS_CONVERGENCE:
              if(a_value->instr == CONSENSUS_REVELATION)
              {
                polite_announcement_cancel();
                PRINTF("\n%u: received_revelation_announcement from %u with: instr %u, degree %u, date_coarse %lu, date_fine %lu, ref_addr %u",
                    linkaddr_node_addr.u16, from->u16, a_value->instr, a_value->degree, a_value->date_coarse, a_value->date_fine, a_value->ref_addr);
                if(rtimer_schedule(RTIMER_0, RTIMER_DATE, a_value->date_coarse, a_value->date_fine, enter_consensus_synchronization))
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
                        my_cons_slot = a_value->degree;
                      }
                    }
                  }

                  my_cons_slot = NUM_CONS_SLOTS + 1;
                  my_slot_ack = 0;
                  enter_consensus_revelation(rt);
                }

              }
            break;

            case CONSENSUS_REVELATION:
                if(a_value->instr == my_state)
                {
                  if(my_cluster.role == CM)
                  {
                    for(ch = list_head(*my_cluster.CHs_list); ch != NULL; ch = list_item_next(ch))
                    {
                      if(ch->addr == n->addr)
                      {
                        ch->cons_slot = a_value->degree;
                        my_cons_slot = a_value->degree;
                      }
                    }
                  }
                  if(my_cluster.role == CB)
                  {
                    for(ch = list_head(*my_cluster.CHs_list); ch != NULL; ch = list_item_next(ch))
                    {
                      if(ch->addr == n->addr)
                      {
                        if(ch->cons_slot != a_value->degree)
                        {

                        } 
                        else
                        {

                        }
                      }
                    }
                  }
                }
            break;

            case CONSENSUS_SYNCHRONIZATION:
            break;

            default:
            break;
        }
    }
}


