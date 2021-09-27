#include "net/c-sync/gtsp.h"


#define DEBUG 1
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif




static uint16_t get_lg_delta_mac_netw(uint16_t hw_mac_timestamp, uint16_t hw_ntw_timestamp);


/*---------------------------------------------------------------------------*/
void
gtsp_recv(neighbour_t *n, timesync_frame_t *syncframe, uint8_t new_neighbour)
{
  rtimer_clock_t ta, tb, ta_compare, tb_compare;

  uint32_t now_my_coarse;
  uint32_t now_my_fine;
  double my_clock_rate;

  // #if AVG_CONSENSUS
  // double avg_rate = RTIMER_AVG_RATE();
  // #endif
  
  do
  {
    while(clock_state || clock_priority);

    clock_state = CLOCK_OCCUPIED;
    my_clock_rate = clock_get_rate();

    ta_compare = TACCR0 - RTIMER_AB_UPDATE;
    tb_compare = TBCCR0;
    TBCCTL3 ^= CCIS0;
    TACCTL1 ^= CCIS0;

    ta = TACCR1;
    tb = TBCCR3;

    clock_state = CLOCK_FREE;
    
    now_my_coarse = RTIMER_COARSE_NOW();
    now_my_fine = rtimer_stamps_to_now(ta, tb, ta_compare, tb_compare, my_clock_rate);

    
  } while(now_my_fine == 0);

  //PRINTF("\ngtsp_recv"); 

  uint32_t now_n_coarse = syncframe->coarse_now;
  uint32_t now_n_fine = rtimer_stamps_to_now(syncframe->ta, syncframe->tb, syncframe->ta_compare, syncframe->tb_compare, syncframe->clock_rate);


  if(new_neighbour)
  {
    n->relative_rate = 0;
    n->jumped = 0;
    n->synced = 0;
  }

  uint16_t send_delta_mac_netw = (get_lg_delta_mac_netw(syncframe->tb, syncframe->hw_mac_timestamp) * syncframe->clock_rate) * (syncframe->avg_rate + 1);
  uint16_t recv_delta_mac_netw = (get_lg_delta_mac_netw(packetbuf_attr(PACKETBUF_ATTR_TIMESTAMP), tb) * my_clock_rate) * (n->relative_rate + 1);
  uint16_t delta_transmission = (uint16_t)(TRANSMISSION_DELAY * (1 + n->relative_rate) / (1 + RTIMER_AVG_RATE()));

  uint32_t total_offset = syncframe->fine_offset + send_delta_mac_netw + recv_delta_mac_netw + delta_transmission;

  rtimer_hwdate_to_lgdate(&now_n_coarse, &now_n_fine, total_offset);

  if(!new_neighbour)
  {
    // if(((my_addr == 66) && (n->addr == 69))
    //   || ((my_addr == 69) && ((n->addr == 66) || (n->addr == 62)))
    //   || ((my_addr == 62) && ((n->addr == 74) || (n->addr == 69)))
    //   || ((my_addr == 74) && ((n->addr == 71) || (n->addr == 62)))
    //   || ((my_addr == 71) && ((n->addr == 70) || (n->addr == 74)))
    //   || ((my_addr == 70) && ((n->addr == 71) || (n->addr == 63)))
    //   || ((my_addr == 63) && ((n->addr == 70) || (n->addr == 64)))
    //   || ((my_addr == 64) && ((n->addr == 63) || (n->addr == 65)))
    //   || ((my_addr == 65) && ((n->addr == 76) || (n->addr == 64)))
    //   || ((my_addr == 76) && ((n->addr == 75) || (n->addr == 65)))
    //   || ((my_addr == 75) && ((n->addr == 72) || (n->addr == 76)))
    //   || ((my_addr == 72) && ((n->addr == 77) || (n->addr == 75)))
    //   || ((my_addr == 77) && (n->addr == 72)))
    // if(((my_addr == 40) && (n->addr == 48))
    //   || ((my_addr == 48) && ((n->addr == 40) || (n->addr == 44)))
    //   || ((my_addr == 44) && ((n->addr == 48) || (n->addr == 42)))
    //   || ((my_addr == 42) && ((n->addr == 44) || (n->addr == 45)))
    //   || ((my_addr == 45) && ((n->addr == 42) || (n->addr == 72)))
    //   || ((my_addr == 72) && ((n->addr == 45) || (n->addr == 78)))
    //   || ((my_addr == 78) && ((n->addr == 72) || (n->addr == 49)))
    //   || ((my_addr == 49) && ((n->addr == 78) || (n->addr == 55)))
    //   || ((my_addr == 55) && ((n->addr == 49) || (n->addr == 58)))
    //   || ((my_addr == 58) && ((n->addr == 55) || (n->addr == 54)))
    //   || ((my_addr == 54) && ((n->addr == 58) || (n->addr == 1)))
    //   || ((my_addr == 1) && ((n->addr == 54) || (n->addr == 10)))
    //   || ((my_addr == 10) && ((n->addr == 1) || (n->addr == 51)))
    //   || ((my_addr == 51) && ((n->addr == 10) || (n->addr == 60)))
    //   || ((my_addr == 60) && (n->addr == 51)))
      uint32_t delta_n_coarse = now_n_coarse;
      uint32_t delta_lg_n_fine = now_n_fine;
      rtimer_lgdates_to_lg_interval(&delta_n_coarse, &delta_lg_n_fine, n->last_n_coarse, n->last_lg_n_fine);

      uint32_t delta_my_coarse = now_my_coarse;
      uint32_t delta_hw_my_fine = now_my_fine;
      rtimer_lgdates_to_lg_interval(&delta_my_coarse, &delta_hw_my_fine, n->last_my_coarse, n->last_hw_my_fine);


      double current_rate = ((double)delta_lg_n_fine / (double)delta_hw_my_fine) - 1;

      if(-GTSP_DRIFT_THRESHOLD < current_rate && current_rate < GTSP_DRIFT_THRESHOLD)
      {
        n->relative_rate = ((double)GTSP_MOVING_ALPHA) * n->relative_rate + ((double)(1 - GTSP_MOVING_ALPHA)) * current_rate;
        n->jumped = 0;
      }
      else
      {
        n->jumped = 1;
      }
  }

  n->last_my_coarse = now_my_coarse;
  n->last_hw_my_fine = now_my_fine;
  n->last_n_coarse = now_n_coarse;
  n->last_lg_n_fine = now_n_fine;

  rtimer_update_offset(now_my_coarse, rtimer_stamps_to_now(ta, tb, ta_compare, tb_compare, my_clock_rate));
  rtimer_hwdate_to_lgdate(&now_my_coarse, &now_my_fine, RTIMER_FINE_OFFSET());

  n->coarse_diff = rtimer_diff(now_my_coarse, now_n_coarse);

  //PRINTF("\nI %u, N %u, my_coarse %lu, n_coarse %lu, my_fine %lu, n_fine %lu" , linkaddr_node_addr.u16, n->addr, now_my_coarse, now_n_coarse, now_my_fine, now_n_fine);
  

  if((n->coarse_diff == -1) && (now_my_fine > RTIMER_FINE_MAX - GTSP_COARSE_OVERFLOW_THRESHOLD) && (now_n_fine < GTSP_COARSE_OVERFLOW_THRESHOLD))
  {
    n->fine_diff = rtimer_diff(RTIMER_FINE_MAX, now_my_fine) + now_n_fine;
    n->coarse_diff = 0;
  }
  else if((n->coarse_diff == 1) && (now_my_fine < GTSP_COARSE_OVERFLOW_THRESHOLD) && (now_n_fine > RTIMER_FINE_MAX - GTSP_COARSE_OVERFLOW_THRESHOLD))
  {
    n->fine_diff = rtimer_diff(RTIMER_FINE_MAX, now_n_fine) + now_my_fine;
    n->coarse_diff = 0;
  }
  else
  {
    n->fine_diff = rtimer_diff(now_my_fine, now_n_fine);
  }

  // #if AVG_CONSENSUS
  // if(my_state == CONSENSUS_CONVERGENCE)
  // {
  //   avg_rate += syncframe->avg_rate;
  //   avg_rate /= 2;
  // }
  // else if(my_state == CONSENSUS_SYNCHRONIZATION)
  // {
  //   rtimer_set_avg_rate(syncframe->avg_rate);
  // }
  // #endif

    if(my_state == IDLE || my_state == DISCOVERY)
    //if(my_state == IDLE)
    {
      // if(((my_addr == 66) && (n->addr == 69))
      // || ((my_addr == 69) && ((n->addr == 66) || (n->addr == 62)))
      // || ((my_addr == 62) && ((n->addr == 74) || (n->addr == 69)))
      // || ((my_addr == 74) && ((n->addr == 71) || (n->addr == 62)))
      // || ((my_addr == 71) && ((n->addr == 70) || (n->addr == 74)))
      // || ((my_addr == 70) && ((n->addr == 71) || (n->addr == 63)))
      // || ((my_addr == 63) && ((n->addr == 70) || (n->addr == 64)))
      // || ((my_addr == 64) && ((n->addr == 63) || (n->addr == 65)))
      // || ((my_addr == 65) && ((n->addr == 76) || (n->addr == 64)))
      // || ((my_addr == 76) && ((n->addr == 75) || (n->addr == 65)))
      // || ((my_addr == 75) && ((n->addr == 72) || (n->addr == 76)))
      // || ((my_addr == 72) && ((n->addr == 77) || (n->addr == 75)))
      // || ((my_addr == 77) && (n->addr == 72)))
      // if(((my_addr == 40) && (n->addr == 48))
      // || ((my_addr == 48) && ((n->addr == 40) || (n->addr == 44)))
      // || ((my_addr == 44) && ((n->addr == 48) || (n->addr == 42)))
      // || ((my_addr == 42) && ((n->addr == 44) || (n->addr == 45)))
      // || ((my_addr == 45) && ((n->addr == 42) || (n->addr == 72)))
      // || ((my_addr == 72) && ((n->addr == 45) || (n->addr == 78)))
      // || ((my_addr == 78) && ((n->addr == 72) || (n->addr == 49)))
      // || ((my_addr == 49) && ((n->addr == 78) || (n->addr == 55)))
      // || ((my_addr == 55) && ((n->addr == 49) || (n->addr == 58)))
      // || ((my_addr == 58) && ((n->addr == 55) || (n->addr == 54)))
      // || ((my_addr == 54) && ((n->addr == 58) || (n->addr == 1)))
      // || ((my_addr == 1) && ((n->addr == 54) || (n->addr == 10)))
      // || ((my_addr == 10) && ((n->addr == 1) || (n->addr == 51)))
      // || ((my_addr == 51) && ((n->addr == 10) || (n->addr == 60)))
      // || ((my_addr == 60) && (n->addr == 51)))
        PRINTF("\n%u %lu N %u fd %ld" , linkaddr_node_addr.u16, now_my_fine, n->addr, n->fine_diff);
    }
}

static uint16_t
get_lg_delta_mac_netw(uint16_t hw_a_timestamp, uint16_t hw_b_timestamp)
{
  uint16_t hw_delta_mac_ntw; 

  if(hw_a_timestamp < hw_b_timestamp)
  {
    hw_delta_mac_ntw = hw_b_timestamp - hw_a_timestamp;
  }
  else
  {
    hw_delta_mac_ntw = hw_b_timestamp + (65535 - hw_a_timestamp);
  }

  return hw_delta_mac_ntw;
}

void
gtsp_update_rtimer(list_t neighbour_list)
{
  struct neighbour *n; 
  struct neighbour *nn;

  double avg_rate = RTIMER_AVG_RATE();

  uint8_t coarse_diff_count = 0;
  int32_t coarse_synced_offset = 0;
  uint8_t coarse_synced_count = 0;

  int32_t fine_diff_offset = 0;
  uint8_t fine_diff_count = 0;
  int32_t fine_synced_offset = 0;
  uint8_t fine_synced_count = 0;

  for(n = list_head(neighbour_list); n != NULL; n = list_item_next(n)) 
  {
    //PRINTF(", sync %u", n->addr);
    //PRINTF("\n I %u, N %u, %u,fd %ld", my_addr, n->addr, n->synced ,n->fine_diff);

    if(n->state != my_state)
    {
      continue;
    }

    if(n->coarse_diff == 0)
    {
      coarse_synced_count++;
    }
    else
    {
      coarse_diff_count++;
      coarse_synced_offset += n->coarse_diff;
      // rtimer_adjust_coarse_count(coarse_synced_offset);
    }

    // if(coarse_synced_count < coarse_diff_count)
    // {
    //   for(n = list_head(neighbour_list); n != NULL; n = list_item_next(n)) 
    //   {
    //     coarse_diff_count = 0;

    //     for(nn = list_head(neighbour_list); nn != NULL; nn = list_item_next(nn)) 
    //     {
    //       if(n->coarse_diff == nn->coarse_diff)
    //       {
    //         coarse_diff_count++;
    //       }
    //     }


    //     // Need to change something here. Majority in sync, no jump is made
    //     if(coarse_diff_count > coarse_synced_count)
    //     {
    //       coarse_synced_count = coarse_diff_count;
    //       coarse_synced_offset = n->coarse_diff;
    //     }
    //     if(coarse_synced_offset)
    //       rtimer_adjust_coarse_count(coarse_synced_offset);
    //   }
    // }


    if(-GTSP_JUMP_THRESHOLD < n->fine_diff && n->fine_diff < GTSP_JUMP_THRESHOLD)
    {

      fine_synced_offset += n->fine_diff;
      avg_rate += n->relative_rate;
      fine_synced_count++;
      n->synced = 1;
    }
    else
    {
      n->synced = 0;
      fine_diff_count++;
    }
  }

  coarse_synced_offset /= (coarse_diff_count + 1);

  //PRINTF(", coarse_synced_offset %ld", coarse_synced_offset);
  rtimer_adjust_coarse_count(coarse_synced_offset);

  fine_synced_offset /= (fine_synced_count + 1);

  
  //if(fine_synced_count > fine_diff_count)
  {
    avg_rate /= (fine_synced_count + fine_diff_count + 1);
    rtimer_set_avg_rate(avg_rate);

    //PRINTF(", synced_offset %ld", fine_synced_offset);
  }
  if(my_state == DISCOVERY) //else
  {
    //fine_synced_offset = 0;
    int32_t next_offset = 0;

    for(n = list_head(neighbour_list); n != NULL; n = list_item_next(n)) 
    {
      if(n->synced)
      {
        continue;
      }

      fine_diff_count = 1;
      fine_diff_offset = n->fine_diff;

      for(nn = n->next; nn != NULL; nn = list_item_next(nn)) 
      {
        next_offset = rtimer_diff(n->fine_diff, nn->fine_diff);
        if(-GTSP_JUMP_THRESHOLD < next_offset && next_offset < GTSP_JUMP_THRESHOLD)
        {
          fine_diff_count++;
          fine_diff_offset += nn->fine_diff;
        }
      }

      // PRINTF("\nFSC %u, FDC %u", fine_synced_count, fine_diff_count);
      //if(fine_diff_count >= fine_synced_count)
      {
        fine_synced_count = fine_diff_count;
        fine_synced_offset = fine_diff_offset / fine_synced_count;
      }
      //PRINTF("\nFSC %u, FDC %u", fine_synced_count, fine_diff_count);
    }

    //PRINTF(", diff_offset %ld", fine_synced_offset);
  }

  rtimer_adjust_fine_offset(fine_synced_offset);

  for(n = list_head(neighbour_list); n != NULL; n = list_item_next(n))
  {
    n->coarse_diff -= coarse_synced_offset;
    n->fine_diff -=  fine_synced_offset;
    if(-GTSP_JUMP_THRESHOLD < n->fine_diff && n->fine_diff < GTSP_JUMP_THRESHOLD)
    {
      n->synced = 1;
    }
    else
    {
      n->synced = 0;
    }
  }
}