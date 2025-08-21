#include <deca_device_api.h>
#include <deca_spi.h>
#include <port.h>
#include <shared_defines.h>
#include <shared_functions.h>

/* Default antenna delay values for 64 MHz PRF. See NOTE 2 below. */
#define TX_ANT_DLY 16385
#define RX_ANT_DLY 16385

// Pingwin defined hardcoded addresses for now (netowrk order)
// This is taken from the ss_twr_initiator.c
#define FRAME_CTRL 0x41, 0x88 // 0x8841 - 16-bit addressing
#define PAN_ID 0x48, 0x50     // PH - Pingwin Huddle
#define INIT_ADDR 0x42, 0xef  // Hardcoded !
#define EMPTY_ADDR 0x00, 0x00 // Initially set to 0x00 0x00, to be replaced

/* Length of the common part of the message (up to and including the function
 * code, see NOTE 3 below). */
#define ALL_MSG_COMMON_LEN 10

/* Index to access some of the fields in the frames involved in the process. */
#define ALL_MSG_SN_IDX 2
#define RESP_MSG_POLL_RX_TS_IDX 10
#define RESP_MSG_RESP_TX_TS_IDX 14
#define RESP_MSG_TS_LEN 4

// This is the default config we used for initial trials,
// between the pingwin initiator and pingwin responder. NO STS
extern dwt_config_t config;

extern dwt_config_t config_with_sts;
extern dwt_txconfig_t txconfig_options;

extern uint8_t poll_msg[12];
extern uint8_t resp_msg[20];

#define BEACON_COUNT 3
extern const uint16_t beacon_address_array[BEACON_COUNT];
