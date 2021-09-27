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
received_declaration_announcement(struct announcement *a, const linkaddr_t *from,
            uint16_t id, struct announcement_value *a_value, timesync_frame_t *syncframe, annstate_t last_event)
{


  struct neighbour *n;
  if(packetbuf_attr(PACKETBUF_ATTR_RSSI) >= RSSI_THRESHOLD)
  {
    
    n = add_to_neighbour(from->u16, a_value->instr, a_value->degree, syncframe);
    if(n == NULL || !n->synced)
    {
      return;
    }

    #if MOD_NEIGHBOURS && MOD_TYPE == 5
    if(((my_addr == 65) || (my_addr == 71) || (my_addr == 76)) && ((my_state == ELECTION_DECLARATION) || (my_state == ELECTION_REVELATION)))
    {
        PRINTF("\n%u: received_declaration_announcement from %u with: instr %u, degree %u, date_coarse %lu, date_fine %lu, ref_addr %u",
          linkaddr_node_addr.u16, from->u16, a_value->instr, a_value->degree, a_value->date_coarse, a_value->date_fine, a_value->ref_addr);
        switch(my_addr)
        {
          case 65:
            switch(n->addr)
            {
              case 76: case 64:
                handle_lists(a, a_value, n);
            }
          case 71:
            switch(n->addr)
            {
              case 70: case 74:
                handle_lists(a, a_value, n);
            }
          case 76:
            switch(n->addr)
            {
              case 75: case 77:
                handle_lists(a, a_value, n);
            }
        }
    }
    #endif


    
    /* Clustering starts here */
    switch(my_state)
    {
        case DISCOVERY: 
          my_cluster.role = CM;
          broadcast_announcement_stop();
          announcement_remove_value(&discovery_announcement);
          polite_announcement_init(LOGICAL_CHANNEL, 0, PA_RESILIENCE_MAX_SEND_DUPS, PA_REGULAR_MAX_RECV_DUPS);
          if(a_value->instr == CONNECTION_DECLARATION)
          {
            my_state = CONNECTION_DECLARATION;
            process_poll(&c_gtsp_process);
          }
          else if(a_value->instr == ELECTION_DECLARATION)
          {

              polite_announcement_cancel();
              if(rtimer_schedule(RTIMER_0, RTIMER_DATE, a_value->date_coarse, a_value->date_fine, enter_connection_revelation))
              {
                handle_lists(a, a_value, n);
                rt[RTIMER_0].state = RTIMER_SINGLEPASS;
                enter_election_declaration(rt);
              }
          }
        break;

        case ELECTION_REVELATION:

          if(a_value->instr == ELECTION_DECLARATION)
          {
            // PRINTF("\n%u: received_declaration_announcement from %u with: instr %u, degree %u, date_coarse %lu, date_fine %lu, ref_addr %u",
            //   linkaddr_node_addr.u16, from->u16, a_value->instr, a_value->degree, a_value->date_coarse, a_value->date_fine, a_value->ref_addr);
            polite_announcement_cancel();
            if(rtimer_schedule(RTIMER_0, RTIMER_DATE, a_value->date_coarse, a_value->date_fine, enter_connection_revelation))
            {
              rt[RTIMER_0].state = RTIMER_SINGLEPASS;
              enter_election_declaration(rt);
            }
          }
        break;

        case ELECTION_DECLARATION:
          if(a_value->instr == my_state)
          {
            if(n->degree > my_degree || (n->degree == my_degree && n->addr > my_addr))
            {
              polite_announcement_cancel();
              if(!handle_lists(a, a_value, n))
              {
                announcement_bump(a);
              }
            }
          }

          else if(a_value->instr == CONNECTION_DECLARATION)
          {
            polite_announcement_cancel();

            if(rtimer_schedule(RTIMER_0, RTIMER_DATE, a_value->date_coarse, a_value->date_fine, enter_convergence))
            {
              rt[RTIMER_0].state = RTIMER_SINGLEPASS;
              list_init(*my_cluster.CHs_list);
              enter_connection_revelation(rt);
            }
          }
        break;

        case CONNECTION_REVELATION:
        break;

        case CONNECTION_DECLARATION:
          if(a_value->instr == my_state)
          {

            if(my_cluster.role == CB)
            {

            }
            else if(my_cluster.role == CH)
            {
              handle_lists(a, a_value, n);
            }
          }
        break;

        case CONSENSUS_CONVERGENCE:
        break;

        case CONSENSUS_REVELATION:
        break;

        case CONSENSUS_SYNCHRONIZATION:
        break;

        default:
        break;
    }
  }
}

