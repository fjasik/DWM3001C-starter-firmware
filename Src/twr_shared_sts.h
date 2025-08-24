#include <deca_device_api.h>
#include <deca_spi.h>
#include <port.h>
#include <shared_defines.h>
#include <shared_functions.h>

// -------------------------------------------------------
// STS stuff goes here
// (change and optimize later)
// -------------------------------------------------------

// Default antenna delay values for 64 MHz PRF
#define TX_ANT_DLY 16385
#define RX_ANT_DLY 16385

#define BEACON_COUNT 3
#define HUDDLE_STATIC_VENDOR_ID 0x4850u // PH - Pingwin Huddle (big endian)

// 128-bit STS key (dev default; replace for production)
#define DEFAULT_STS_KEY {                          \
    0x14EB220F, 0xF86050A8, 0xD1D336AA, 0x14148674 \
}

// Pack 4 bytes (big-endian) into a u32
#ifndef U32BE4
#define U32BE4(a,b,c,d) ( ((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | ((uint32_t)(d)) )
#endif

// Per-beacon configuration
typedef struct {
    uint16_t short_addr;  // 16-bit short address
    uint8_t  iv6[6];      // Static STS IV bytes [6] (after 2B vendor ID)
} huddle_beacon_cfg_t;

extern dwt_config_t config_with_sts;

// Global table of beacons (defined in twr_shared.c)
extern const huddle_beacon_cfg_t g_hudddle_beacons[BEACON_COUNT];

// Build a 128-bit IV = [ vendorId(2) | iv6(6) | 0x00000000 | counter32 ]
void huddle_build_sts_iv_for_beacon(unsigned idx, dwt_sts_cp_iv_t *out_iv);
