#include "twr_shared_sts.h"

// -------------------------------------------------------
// STS stuff goes here
// -------------------------------------------------------

dwt_config_t config_with_sts = {
    5,
    DWT_PLEN_128,
    DWT_PAC8,
    9,
    9,
    DWT_SFD_IEEE_4Z,
    DWT_BR_6M8,
    DWT_PHRMODE_STD,
    DWT_PHRRATE_STD,
    (129 + 1 + 8 - 8),
    DWT_STS_MODE_ND,
    DWT_STS_LEN_64,
    DWT_PDOA_M0
};

const huddle_beacon_cfg_t g_huddle_beacons[BEACON_COUNT] = {
    { HUDDLE_STATIC_VENDOR_ID, { 0x62, 0x63, 0x6e, 0x00, 0x00, 0x01 } },   // "bcn", 0x000001
    { HUDDLE_STATIC_VENDOR_ID, { 0x62, 0x63, 0x6e, 0x00, 0x00, 0x02 } },   // "bcn", 0x000002
    { HUDDLE_STATIC_VENDOR_ID, { 0x62, 0x63, 0x6e, 0x00, 0x00, 0x03 } },   // "bcn", 0x000003
};

void huddle_build_sts_iv_for_beacon(unsigned idx, dwt_sts_cp_iv_t *out_iv) {
    if (!out_iv) {
        return;
    }

    if (idx >= BEACON_COUNT) {
        *out_iv = (dwt_sts_cp_iv_t){ 0, 0, 0, 0 };
        return;
    }

    const uint16_t vendor_id = g_huddle_beacons[idx].short_addr;
    const uint8_t *iv = g_huddle_beacons[idx].iv6;

    *out_iv = (dwt_sts_cp_iv_t){
        U32BE4((vendor_id >> 8) & 0xFF,
                vendor_id & 0xFF,
                iv[0], 
                iv[1]),
        U32BE4(iv[2], iv[3], iv[4], iv[5]),
        0x00000000u,  // fixed upper[63:32]
        0x00000000u   // counter start (chip advances on RX/TX)
    };
}