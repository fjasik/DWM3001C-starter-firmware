#include "twr_shared.h"

dwt_config_t config = {
    5,            /* Channel number. */
    DWT_PLEN_128, /* Preamble length. Used in TX only. */
    DWT_PAC8,     /* Preamble acquisition chunk size. Used in RX only. */
    9,            /* TX preamble code. Used in TX only. */
    9,            /* RX preamble code. Used in RX only. */
    2, /* 0 to use standard 8 symbol SFD, 1 to use non-standard 8 symbol, 2 for
          non-standard 16 symbol SFD and 3 for 4z 8 symbol SDF type */
    DWT_BR_6M8,      /* Data rate. */
    DWT_PHRMODE_STD, /* PHY header mode. */
    DWT_PHRRATE_STD, /* PHY header rate. */
    (129 + 8 - 8), /* SFD timeout (preamble length + 1 + SFD length - PAC size).
                      Used in RX only. */
    DWT_STS_MODE_OFF, /* STS disabled */
    DWT_STS_LEN_64,   /* STS length see allowed values in Enum dwt_sts_lengths_e
                       */
    DWT_PDOA_M0       /* PDOA mode off */
};

dwt_config_t config_with_sts = {
    5,            /* Channel number. */
    DWT_PLEN_128, /* Preamble length. Used in TX only. */
    DWT_PAC8,     /* Preamble acquisition chunk size. Used in RX only. */
    9,            /* TX preamble code. Used in TX only. */
    9,            /* RX preamble code. Used in RX only. */
    2, /* 0 to use standard 8 symbol SFD, 1 to use non-standard 8 symbol, 2 for
          non-standard 16 symbol SFD and 3 for 4z 8 symbol SDF type */
    DWT_BR_6M8,        /* Data rate. */
    DWT_PHRMODE_STD,   /* PHY header mode. */
    DWT_PHRRATE_STD,   /* PHY header rate. */
    (129 + 1 + 8 - 8), /* SFD timeout (preamble length + 1 + SFD length - PAC
                          size). Used in RX only. */
    DWT_STS_MODE_ND,   /* STS no-data (SP3) */
    DWT_STS_LEN_64, /* STS length see allowed values in Enum dwt_sts_lengths_e
                     */
    DWT_PDOA_M0     /* PDOA mode off */
};

dwt_txconfig_t txconfig_options = {
    0x34,       /* PG delay. */
    0xfdfdfdfd, /* TX power. */
    0x0         /*PG count*/
};

uint8_t poll_msg[] = {FRAME_CTRL, 0, PAN_ID, EMPTY_ADDR, INIT_ADDR, 0xE0, 0, 0};
uint8_t resp_msg[] = {
    FRAME_CTRL,
    0,
    PAN_ID,
    INIT_ADDR,
    EMPTY_ADDR,
    0xE1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0
};

const uint16_t beacon_address_array[BEACON_COUNT] = {
    0xaa69, // beacon 1
    0xbb69, // beacon 2
    0xcc69  // beacon 3
};
