#ifndef __PROJECT_CONF_H__
#define __PROJECT_CONF_H__

#undef NETSTACK_NETWORK
#define NETSTACK_NETWORK rime_driver
#undef NETSTACK_CONF_LLSEC
#define NETSTACK_CONF_LLSEC nullsec_driver
#undef NETSTACK_CONF_MAC
#define NETSTACK_CONF_MAC  csma_driver
#undef NETSTACK_CONF_RDC
#define NETSTACK_CONF_RDC csyncrdc_framer_driver 
#undef NETSTACK_CONF_FRAMER
#define NETSTACK_CONF_FRAMER framer_802154
#undef NETSTACK_CONF_RADIO
#define NETSTACK_CONF_RADIO cc2420_driver

#undef NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE
#define NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE 32

#undef PACKETBUF_CONF_WITH_PACKET_TYPE
#define PACKETBUF_CONF_WITH_PACKET_TYPE 1
//#define CHAMELEON_CONF_MODULE chameleon_raw

/* Enable SFD timestamps */
#undef CC2420_CONF_SFD_TIMESTAMPS
#define CC2420_CONF_SFD_TIMESTAMPS       1

#undef F_CPU
#define F_CPU 4194304uL						// Defines CPU and DCO speed
#define RTIMER_CONF_SECOND 524288UL    		// RTIMER_ARCH_SECOND
#define RTIMER_HF_SECOND RTIMER_CONF_SECOND
#define RTIMER_LF_SECOND 512UL

#define RTIMER_AB_RESOLUTION RTIMER_HF_SECOND / RTIMER_LF_SECOND
#define RTIMER_AB_RESOLUTION_SHIFT 10 // log2(RTIMER_AB_RESOLUTION) FIXED!!!!!!!!!!

/* Timer A update interval of tb_compare, maybe make it smaller, minimum RTIMER_AB_UPDATE = 2, maximum RTIMER_AB_UPDATE = (65536 * RTIMER_LF_SECOND / RTIMER_HF_SECOND) */
//#define RTIMER_AB_UPDATE 2 // ~update tb_compare every 4 ms 						(range 2, 4, 8,16,32)
//#define RTIMER_AB_UPDATE_SHIFT 11 // log2(RTIMER_AB_RESOLUTION * RTIMER_AB_UPDATE) 
#define RTIMER_AB_UPDATE 4 // ~update tb_compare every 4 ms 						(range 2, 4, 8,16,32)
#define RTIMER_AB_UPDATE_SHIFT 12 // log2(RTIMER_AB_RESOLUTION * RTIMER_AB_UPDATE) 
#define RTIMER_AB_UPDATE_RESOLUTION RTIMER_AB_RESOLUTION * RTIMER_AB_UPDATE
#define TRANSMISSION_DELAY 3 // Datasheet: 3us -> 6; Bit less due to delay on SFD flag

#define TRUE 1



#define MOD_NEIGHBOURS 1 // default 0, 1 for hardcoded neighbours to create topologies
#define MOD_TYPE 5 // 1 for full network, 5 for chain, 6 for sparse network, 7 for dense network, 3 for byzantine testing
#define IDLE_BROADCAST 1
#define TEST_FTSP 1 // default 0, 1 for FTSP testing
#define TEST_GTSP 0 // default 0, 1 for GTSP testing
#define TEST_BYZ 0 // default 0, 1 for Byzantine fault testing

#define AVG_CONSENSUS 0

#define MAX_RX_SYNC_DISCOVERY 12
#define RSSI_THRESHOLD -80

// EVENT TIMER INTERVALS
//#define DISC_MIN_INTERVAL 25  // 25 CLOCK_SECOND tick -> 128Hz
#define DISC_MIN_INTERVAL CLOCK_SECOND * 0.1
#define DISC_MAX_INTERVAL CLOCK_SECOND * 0.5
#define POLITE_INTERVAL 4 // 1 CLOCK_SECOND tick -> 128Hz
#define IDLE_MIN_INTERVAL CLOCK_SECOND * 0.1
#define IDLE_MAX_INTERVAL CLOCK_SECOND * 0.5

// RTIMER INTERVALS
#define DISC_TO_EREV_INTERVAL RTIMER_HF_SECOND * 5
// #define DISC_TO_EREV_INTERVAL RTIMER_HF_SECOND * 5 for normal operation

#define REGULAR_SLOT_INTERVAL RTIMER_HF_SECOND * 0.3
#define CONS_CTRL_SLOT_INTERVAL RTIMER_HF_SECOND * 0.3
#define IDLE_SLOT_INTERVAL RTIMER_HF_SECOND * 3

//CSMA PARAMETERS, to keep delay from rtimer_sync_send to MAC-Layer timestamping within 125ms -> no Timer B overflow -> valid timedata. Otherwise free packet
#define CSMA_BACKOFF_PERIOD 1 // ticks in CLOCK_SECOND (128 Hz)
#define CSMA_CONF_MAX_FRAME_RETRIES 2
#define RADIO_CCA_THRESHOLD -80

//POLITE_ANNOUNCEMENT
#define PA_REGULAR_MAX_SEND_DUPS 1
#define PA_REGULAR_MAX_RECV_DUPS 20
#define PA_RESILIENCE_MAX_SEND_DUPS 10

#define NUM_CH_MAX 4
#define NUM_CB_MAX NUM_CH_MAX * 2
#define NUM_BL_MAX 2


#define UTIL_PROACTIVE_SLOT 3
#define NUM_CONS_SLOTS 3
#define POLITE_PROACTIVE_OFFSET ((CONS_CTRL_SLOT_INTERVAL / RTIMER_HF_SECOND) * CLOCK_SECOND) / 3
#define NUM_CONS_CTRL_ITERATIONS 3

#define BYZANTINE_FINE_DIFF 500
#define BYZANTINE_COARSE_DIFF 3
#define BYZANTINE_INTERVAL RTIMER_HF_SECOND * 0.05

#endif /* __PROJECT_CONF_H__ */
