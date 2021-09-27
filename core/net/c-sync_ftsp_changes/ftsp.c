#include "contiki-conf.h"
#include "net/rime/rime.h"
#include "net/c-sync/c-sync.h"

#define FTSP_ENTRY_EMPTY           0      // Regression table entry is empty
#define FTSP_ENTRY_FULL            1      // Regression table entry is full
#define FTSP_ERR                   0      // Synchronization Error state
#define FTSP_OK                    1      // Synchronization OK state
#define FTSP_ENTRY_VALID_LIMIT     4      // number of entries to become synchronized
#define FTSP_ENTRY_THROWOUT_LIMIT  1000    // if time sync error is bigger than this clear the table
#define FTSP_MAX_ENTRIES           8      // number of entries in the regression table


#define DEBUG 1
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif


/**
 *  Regression table entry structure
 */
typedef struct ftsp_table_t {
    uint8_t state;              /// << Indicates if the entries is used or not
    uint32_t local;             /// << Recorded local time
    int32_t offset;            /// << Difference of received global time and local time
} ftsp_table_t;

static ftsp_table_t table[FTSP_MAX_ENTRIES];
static uint8_t  table_entries = 0;
/* Since contiki does not have a hierarchy, temporarily fix node 1 as root */
static uint8_t  free_item = 0;
//static uint8_t  heart_beats = 0;
static uint8_t num_errors = 0;
static int32_t  offset;
static float    skew;
//static uint32_t local_time;
static uint32_t now_my_coarse = 0;
static uint32_t now_my_fine = 0;
static uint32_t now_n_coarse = 0;
static uint32_t now_n_fine = 0;
static uint32_t now_local_time = 0;

//static void send_beacon(ftsp_beacon_t *);
// static void linear_regression(void);
static void clear_table();
// static void add_to_regression_table();


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

/*---------------------------------------------------------------------------*/
void
ftsp_recv(timesync_frame_t *syncframe, uint16_t n_seq_num, uint16_t sender)
{
    rtimer_clock_t ta, tb, ta_compare, tb_compare;
    int32_t coarse_diff = 0, fine_diff = 0;

    double my_clock_rate;

  
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

    
    

    now_n_coarse = syncframe->coarse_now;
    now_n_fine = rtimer_stamps_to_now(syncframe->ta, syncframe->tb, syncframe->ta_compare, syncframe->tb_compare, syncframe->clock_rate);

    uint16_t send_delta_mac_netw = (get_lg_delta_mac_netw(syncframe->tb, syncframe->hw_mac_timestamp) * syncframe->clock_rate) * (syncframe->avg_rate + 1);
    uint16_t recv_delta_mac_netw = (get_lg_delta_mac_netw(packetbuf_attr(PACKETBUF_ATTR_TIMESTAMP), tb) * my_clock_rate) * (skew + 1);
    uint16_t delta_transmission = (uint16_t)TRANSMISSION_DELAY;
    uint32_t total_offset = syncframe->fine_offset + send_delta_mac_netw + recv_delta_mac_netw + delta_transmission;  

    rtimer_hwdate_to_lgdate(&now_n_coarse, &now_n_fine, total_offset);


    rtimer_update_offset(now_my_coarse, rtimer_stamps_to_now(ta, tb, ta_compare, tb_compare, my_clock_rate));
    rtimer_hwdate_to_lgdate(&now_my_coarse, &now_my_fine, RTIMER_FINE_OFFSET());

    now_local_time = rtimer_lgdate_to_hwdate(&now_my_coarse, &now_my_fine);

    coarse_diff = rtimer_diff(now_my_coarse, now_n_coarse);

  //PRINTF("\nI %u, N %u, my_coarse %lu, n_coarse %lu, my_fine %lu, n_fine %lu" , linkaddr_node_addr.u16, n->addr, now_my_coarse, now_n_coarse, now_my_fine, now_n_fine);
  

    if((coarse_diff == -1) && (now_my_fine > RTIMER_FINE_MAX - FTSP_ENTRY_THROWOUT_LIMIT) && (now_n_fine < FTSP_ENTRY_THROWOUT_LIMIT))
    {
        fine_diff = rtimer_diff(RTIMER_FINE_MAX, now_my_fine) + now_n_fine;
        coarse_diff = 0;
    }
    else if((coarse_diff == 1) && (now_my_fine < FTSP_ENTRY_THROWOUT_LIMIT) && (now_n_fine > RTIMER_FINE_MAX - FTSP_ENTRY_THROWOUT_LIMIT))
    {
        fine_diff = rtimer_diff(RTIMER_FINE_MAX, now_n_fine) + now_my_fine;
        coarse_diff = 0;
    }
    else
    {
        fine_diff = rtimer_diff(now_my_fine, now_n_fine);
    }

    //PRINTF("The seq num for n and my are %d and %d\n", n_seq_num, my_degree);

    // if (n_seq_num <= my_degree)
    // {
    //     return;
    // } 

    // my_degree = n_seq_num;
        

        
    // Ignoring this condition with a fixed root
    // if (root_id == my_addr) //((root_id == old_root)  {
    // {
    //   heart_beats = 0;
    // }

    // add new entry to the regression table
    // add_to_regression_table();
    // linear_regression();
    // my_degree++;



    PRINTF("\n%u in FTSP with offset %ld, %lu N %u fd %ld" , linkaddr_node_addr.u16, rtimer_fine_offset(), now_my_fine, sender, fine_diff); //rtimer_diff(now_my_fine, now_n_fine)

    // rt[RTIMER_0].state = RTIMER_SINGLEPASS;
    // if ((my_addr == root_id) || (is_synced() == FTSP_ERR))
    //     enter_discovery(rt);
    // else
    //     enter_idle(rt);

}


void linear_regression()
{
    // NS:printf("linear_regression\n");
    int32_t local_sum = 0, offset_sum = 0,
            local_average = 0, offset_average = 0;
    uint8_t i = 0;
    float new_skew = skew;
    uint32_t local_time;

    int32_t new_local_average = 0, new_offset_average = 0;


    if (table_entries == 0) 
    {
        PRINTF("Returning because table_entries is zero\n");
        return;
    }

    /*
        We use a rough approximation first to avoid time overflow errors. The idea
        is that all times in the table should be relatively close to each other.
    */

    new_local_average = table[i].local;
    new_offset_average = table[i].offset;

    // NS:printf("The last non-null offset is %d\n", new_offset_average);

    for (i = 0; i < FTSP_MAX_ENTRIES; i++)
    {
        if (table[i].state == FTSP_ENTRY_FULL)
        {    
            local_sum += (table[i].local - new_local_average) / table_entries;
            local_average += (table[i].local - new_local_average) % table_entries;
            offset_sum += (table[i].offset - new_offset_average) /  table_entries;
            offset_average += (table[i].offset - new_offset_average) % table_entries;
        }
    }

    new_local_average += local_sum + local_average / table_entries;
    new_offset_average += offset_sum + offset_average / table_entries;

    // NS:printf("Updated offset after calculation = %d\n", new_offset_average);


    local_sum = offset_sum = 0;
    for(i = 0; i < FTSP_MAX_ENTRIES; ++i)
    {
       if( table[i].state == FTSP_ENTRY_FULL ) 
       {
                int32_t a = table[i].local - new_local_average;
                int32_t b = table[i].offset - new_offset_average;

                local_sum += (int32_t)a * a;
                offset_sum += (int32_t)a * b;
       }
    }

    if( local_sum != 0 )
        new_skew = (float)offset_sum / (float)local_sum;

    //PRINTF("New skew and offset are %d and %d\n", new_skew, new_offset_average);

    skew = new_skew;
    rtimer_set_avg_rate(skew);
    offset = new_offset_average;
    rtimer_adjust_fine_offset(offset);
    local_time = new_local_average;

    PRINTF("The local time is %lu and offset is %ld\n", local_time, offset);

    //PRINTF("Finished Regression\n");

    // NS:printf("Rate = %4.4f and offset = %u\n", rate, offset);

    // NS:printf("FTSP linear_regression calculated: num_entries=%u, is_synced=%u\n",
    // NS:      table_entries, is_synced());
}

/* This function not only adds an entry but also removes old entries. */
void add_to_regression_table(neighbour_t *n, uint32_t localtime)
{
    int8_t i = 0;
    //int32_t time_error = 0;

    // clear table if the received entry's been inconsistent for some time
    // nitin: revisit
    //time_error = rtimer_diff(now_n_fine, now_my_fine);
    //PRINTF("The time error of received message %ld\n", time_error);

    if( (is_synced() == FTSP_OK) &&
        (n->fine_diff > FTSP_ENTRY_THROWOUT_LIMIT || n->fine_diff < -FTSP_ENTRY_THROWOUT_LIMIT))
    {
        if (++num_errors>5)
        {
            clear_table();
            PRINTF("Clearing the table here\n");
        }
        PRINTF("Error is too high\n");
        return; // don't incorporate a bad reading
    }


    num_errors = 0;

    for(i = 0; i < FTSP_MAX_ENTRIES; ++i)
    {
        // age = msg->local - table[i].local;

        // //logical time error compensation
        // if( age >= 0x7FFFFFFF )
        //     table[i].state = FTSP_ENTRY_EMPTY;

        if( table[i].state == FTSP_ENTRY_EMPTY )
            free_item = i;


    }

    // NS:printf("Adding new item to table for node %u\n", msg->id);

    table[free_item].state = FTSP_ENTRY_FULL;
    table[free_item].local = localtime; //now_local_time; //msg->local;
    table[free_item].offset = n->fine_diff; //now_n_fine - now_local_time; //msg->global - msg->local;
    free_item = (free_item + 1) % FTSP_MAX_ENTRIES;
    table_entries++;
    // NS:printf("Table entry offset = %d\n", table[free_item].offset);

}

static void clear_table()
{
    uint8_t i;
    for (i = 0; i < FTSP_MAX_ENTRIES; i++) {
        table[i].state = FTSP_ENTRY_EMPTY;
    }

    table_entries = 0;
}


int is_synced(void)
{
    if ((table_entries >= FTSP_ENTRY_VALID_LIMIT)) { //|| (root_id == my_addr)
        return FTSP_OK;
    }
    else {
        return FTSP_ERR;
    }
}