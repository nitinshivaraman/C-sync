#include "net/c-sync/c-sync.h"

#if 0
#define DEBUG 1
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif



/*---------------------------------------------------------------------------*/
void
handle_byzantine_message(struct announcement *a, struct announcement_value *a_value, struct neighbour *n, uint16_t threshold)
{

    uint16_t msg_count = 0;


    /* Clustering starts here */
    switch(my_state)
    {

        case DISCOVERY: 
        //break;

        case ELECTION_REVELATION:
        //break;

        case ELECTION_DECLARATION:
        //break;

        case CONNECTION_REVELATION:
        //break;
        
        case CONNECTION_DECLARATION:
        //break;

        case CONSENSUS_CONVERGENCE:
        //break;

        case CONSENSUS_REVELATION:
        //break;

        case CONSENSUS_SYNCHRONIZATION:
        
        polite_announcement_cancel();
        if(a_value->instr == BYZANTINE_CONSENSUS)
        {
            
            if(my_cluster.role == CM && (n->role == CB || n->role == CM))
            {
                //if(rtimer_schedule(RTIMER_0, RTIMER_DATE, a_value->date_coarse, a_value->date_fine, enter_consensus_synchronization))
                {
                    //rt[RTIMER_0].state = RTIMER_SINGLEPASS;
                    enter_byzantine_consensus(rt);
                }
            }
            else if(my_cluster.role == CB && n->role == CM)
            {
            	csync_trusted_synchronization(n, a_value->ref_addr);
            	if(my_cons_slot > this_sync_slot)
                {
                  break;
                }
                else
                {
                	//if(rtimer_schedule(RTIMER_0, RTIMER_DATE, a_value->date_coarse, a_value->date_fine, enter_consensus_synchronization))
                    {
                        //rt[RTIMER_0].state = RTIMER_SINGLEPASS;
                        enter_byzantine_consensus(rt);
                    }
                }
            }
        }

        case BYZANTINE_CONSENSUS:
        if(a_value->instr == BYZANTINE_CONSENSUS)
        {
            if(my_cluster.role == CM || my_cluster.role == CB)
            {
            	msg_count++;
                if(msg_count > threshold)
                {
                    polite_announcement_cancel();
                }
                else
                {
                    announcement_set_date_coarse(&byzantine_announcement, a_value->date_coarse);
                    announcement_set_date_fine(&byzantine_announcement, a_value->date_fine);
                    announcement_set_ref_addr(&byzantine_announcement, a_value->ref_addr);
                    announcement_add_value(&byzantine_announcement);
                    announcement_bump(&byzantine_announcement);
                }
            }
        }
        else
        	break;
        
        break;

        default:
        break;
    }
    
}


void
csync_trusted_synchronization(neighbour_t *n, uint16_t c_addr)
{
    neighbour_t *nn;
    struct CHB *ch;
    int32_t n_fine_diff = n->fine_diff;

    if(my_cluster.role == CM)
    {
        if((my_cons_slot == this_sync_slot) && (n->role == CH))
            rtimer_set_avg_rate(n->relative_rate);
        ch = list_head(*my_cluster.CHs_list);
        if(n->addr != ch->addr)
        {
            return;
        }
    }
    else if(my_cluster.role == CB)
    {
        if((my_cons_slot > this_sync_slot) && (n->role == CH))
            rtimer_set_avg_rate(n->relative_rate);
        for(ch = list_head(*my_cluster.CBs_list); ch != NULL; ch = list_item_next(ch))
        {
            if(n->addr == ch->addr)
            {
                return;
            }
        }
    }
    else if(my_cluster.role == CH)
    {
        if((my_cons_slot == this_sync_slot) && (n->role == CB))
            rtimer_set_avg_rate(n->relative_rate);
    }

    /************* NEW ************/ // Check if updating rate will have any impact
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







#endif