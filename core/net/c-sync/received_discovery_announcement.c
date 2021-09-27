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
received_discovery_announcement(struct announcement *a, const linkaddr_t *from,
            uint16_t id, struct announcement_value *a_value, timesync_frame_t *syncframe, annstate_t last_event)
{
    struct neighbour *n;
    //PRINTF("\nPACKETBUF_ATTR_RSSI %d", packetbuf_attr(PACKETBUF_ATTR_RSSI));
    if(packetbuf_attr(PACKETBUF_ATTR_RSSI) >= RSSI_THRESHOLD)
    {
        n = add_to_neighbour(from->u16, a_value->instr, a_value->degree, syncframe);

        if(n == NULL)
        {
            return;
        }
        else if(n->synced)
        {
            synced_counter++;
        }

        /* Clustering starts here */
        switch(my_state)
        {
            case DISCOVERY:

                if(!csync_all_synced())
                {
                    //rt[RTIMER_0].state = RTIMER_INACTIVE;
                    announcement_set_instr(a, DISCOVERY); 
                    synced_counter = 0;
                }

                int8_t comp_res;

#if TEST_GTSP

                if(a_value->instr == my_state && DISCOVERY == announcement_get_instr(a) && synced_counter >= MAX_RX_SYNC_DISCOVERY)
                {
                    if(rtimer_schedule(RTIMER_0, RTIMER_INTERVAL_NOW, 0, DISC_TO_EREV_INTERVAL, enter_idle))
                    {                     
                        PRINTF(", IN");
                        announcement_set_instr(a, DISC_TO_EREV);
                        announcement_set_date_coarse(a, rt[RTIMER_0].time_coarse_lg);
                        announcement_set_date_fine(a, rt[RTIMER_0].time_fine_lg);
                    }
                }
                else if(a_value->instr == DISC_TO_EREV)
                {
                    //PRINTF("\n%u: received_discovery_announcement from %u with: instr %u, degree %u, date_coarse %lu, date_fine %lu",
                        //linkaddr_node_addr.u16, from->u16, a_value->instr, a_value->degree, (uint32_t)a_value->date_coarse, a_value->date_fine);
                    comp_res = rtimer_compare(RTIMER_0, a_value->date_coarse, a_value->date_fine);
                    //PRINTF("comp_res %u", comp_res);
                    if(comp_res == 1)
                    {
                        if(rtimer_schedule(RTIMER_0, RTIMER_DATE, a_value->date_coarse, a_value->date_fine, enter_idle))
                        {
                            PRINTF(", RE");
                            announcement_set_date_coarse(a, a_value->date_coarse);
                            announcement_set_date_fine(a, a_value->date_fine);
                            announcement_set_instr(a, DISC_TO_EREV);
                        }
                        else
                        {
                            rt[RTIMER_0].state = RTIMER_INACTIVE;
                            announcement_set_instr(a, DISCOVERY);
                            synced_counter = 0;
                        }
                    }
                }

#else

                if(a_value->instr == my_state && DISCOVERY == announcement_get_instr(a) && synced_counter >= MAX_RX_SYNC_DISCOVERY)
                {
                    if(rtimer_schedule(RTIMER_0, RTIMER_INTERVAL_NOW, 0, DISC_TO_EREV_INTERVAL, enter_election_revelation))
                    {                     
                        PRINTF(", IN");
                        announcement_set_instr(a, DISC_TO_EREV);
                        announcement_set_date_coarse(a, rt[RTIMER_0].time_coarse_lg);
                        announcement_set_date_fine(a, rt[RTIMER_0].time_fine_lg);
                    }
                }
                else if(a_value->instr == DISC_TO_EREV)
                {
                    //PRINTF("\n%u: received_discovery_announcement from %u with: instr %u, degree %u, date_coarse %lu, date_fine %lu",
                        //linkaddr_node_addr.u16, from->u16, a_value->instr, a_value->degree, (uint32_t)a_value->date_coarse, a_value->date_fine);
                    comp_res = rtimer_compare(RTIMER_0, a_value->date_coarse, a_value->date_fine);
                    //PRINTF("comp_res %u", comp_res);
                    if(comp_res == 1)
                    {
                        if(rtimer_schedule(RTIMER_0, RTIMER_DATE, a_value->date_coarse, a_value->date_fine, enter_election_revelation))
                        {
                            PRINTF(", RE");
                            announcement_set_date_coarse(a, a_value->date_coarse);
                            announcement_set_date_fine(a, a_value->date_fine);
                            announcement_set_instr(a, DISC_TO_EREV);
                        }
                        else
                        {
                            rt[RTIMER_0].state = RTIMER_INACTIVE;
                            announcement_set_instr(a, DISCOVERY);
                            synced_counter = 0;
                        }
                    }
                }

#endif  // TEST_GTSP
            break;

            case ELECTION_REVELATION:
            break;

            case ELECTION_DECLARATION:
            break;

            case CONNECTION_REVELATION:
            break;

            case CONNECTION_DECLARATION:
            break;

            case CONSENSUS_CONVERGENCE:
            break;

            case CONSENSUS_REVELATION:
            break;

            case CONSENSUS_SYNCHRONIZATION:
            break;

            case IDLE:
            break;

            default:
            break;
        }
    }
}

