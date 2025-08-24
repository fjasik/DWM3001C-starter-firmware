#include "../debug.h"
#include "../twr_shared_sts.h"
#include "configs.h"
#include "deca_probe_interface.h"

#include <inttypes.h>
#include <deca_device_api.h>
#include <deca_spi.h>
#include <port.h>
#include <shared_defines.h>
#include <shared_functions.h>

#define BEACON_NUMBER 1

/* Frame sequence number, incremented after each transmission. */
static uint8_t frame_seq_nb = 0;

// Delay between frames, in UWB microseconds
#define POLL_RX_TO_RESP_TX_DLY_UUS 650

// Values for the PG_DELAY and TX_POWER registers reflect the bandwidth and
// power of the spectrum at the current temperature. These values can be
// calibrated prior to taking reference measurements. We roll we default ones
extern dwt_txconfig_t txconfig_options;

int pingwin_ss_twr_responder_sts(void) {
    debug_printf("Pingwin Huddle UWB firmware SS TWR RESP STS no data");

    // Configure SPI rate
    port_set_dw_ic_spi_fastrate();

    // Reset and initialize DW chip.
    reset_DWIC();

    Sleep(2); // Time needed for DW3000 to start up

    // Probe for the correct device driver
    dwt_probe((struct dwt_probe_s*)&dw3000_probe_interf);

    while (!dwt_checkidlerc()) // Need to make sure DW IC is in IDLE_RC
    {};

    if (dwt_initialise(DWT_DW_INIT) == DWT_ERROR) {
        debug_printf("INIT FAILED");
        while (1) {};
    }

    // Enabling LEDs here for debug
    dwt_setleds(DWT_LEDS_ENABLE | DWT_LEDS_INIT_BLINK);

    // Configure DW IC
    if (dwt_configure(&config_with_sts)) {
        debug_printf("CONFIG FAILED");
        while (1) {};
    }

    dwt_sts_cp_iv_t sts_iv = { 0 };
    huddle_build_sts_iv_for_beacon(BEACON_NUMBER, &sts_iv);

    dwt_sts_cp_key_t sts_key = DEFAULT_STS_KEY;

    debug_printf("Initialised beacon number %u", BEACON_NUMBER);
    debug_printf("Using IV: 0x%08" PRIX32 " 0x%08" PRIX32 " 0x%08" PRIX32 " 0x%08" PRIX32 "\n",
             sts_iv.iv0, sts_iv.iv1,
             sts_iv.iv2, sts_iv.iv3);

    // Program Static STS key & IV once; 
    // IV will be (re)loaded each loop so the counter starts predictably.
    dwt_configurestskey(&sts_key);
    dwt_configurestsiv(&sts_iv);

    // Configure the TX parameters
    dwt_configuretxrf(&txconfig_options);
    dwt_setrxantennadelay(RX_ANT_DLY);
    dwt_settxantennadelay(TX_ANT_DLY);

    dwt_setlnapamode(DWT_LNA_ENABLE | DWT_PA_ENABLE);

    const uint32_t rx_delay_time_data_rate 
        = get_rx_delay_time_data_rate(&config_with_sts);
    const uint32_t rx_delay_time_txpreamble 
        = get_rx_delay_time_txpreamble(&config_with_sts);

    while (1) {
        // Use STS no-data (SP3) for Android FiRa interop
        // We do it every loop
        dwt_configurestsmode(DWT_STS_MODE_ND);

        // Reload IV (this loads the 128b V with counter=0; chip advances low 32b counter on RX/TX)
        dwt_configurestsiv(&sts_iv);
        dwt_configurestsloadiv();

        // Activate reception immediatel
        dwt_rxenable(DWT_START_RX_IMMEDIATE);

        uint32_t status_reg = 0;
        uint16_t stsStatus = 0;
        int16_t stsQual = 0;
        int goodSts = 0;

        // Poll for reception of a frame or error/timeout
        waitforsysstatus(
            &status_reg,
            NULL,
            (DWT_INT_RXFR_BIT_MASK | SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_ND_RX_ERR),
            0);
            
        // Check STS correlation quality
        goodSts = dwt_readstsquality(&stsQual);

        if (!(status_reg & DWT_INT_RXFR_BIT_MASK)) {
            dwt_writesysstatuslo(SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR);
            debug_printf("Unspecified error");
            continue;
        }

        if (goodSts < 0 || dwt_readstsstatus(&stsStatus, 0) != DWT_SUCCESS) {
            debug_printf("Bad STS");
            dwt_writesysstatuslo(SYS_STATUS_ALL_RX_ERR);
            continue;
        }

        // Treat any good STS-only "poll" as valid (we're relaxing address checks).
        // Clear good RX frame event in the DW IC status register
        debug_printf("Good frame received");
        dwt_writesysstatuslo(DWT_INT_RXFR_BIT_MASK);

        // Retrieve poll reception timestamp
        const uint64_t poll_rx_ts = get_rx_timestamp_u64();

        // See the example code to know what the fuck is happening here
        const uint32_t resp_tx_time = (poll_rx_ts + (
            (POLL_RX_TO_RESP_TX_DLY_UUS
             + rx_delay_time_data_rate
             + rx_delay_time_txpreamble
             + ((1U << (config_with_sts.stsLength + 2)) * 8U)) * UUS_TO_DWT_TIME)) >> 8;
        dwt_setdelayedtrxtime(resp_tx_time);

        // Send SP3 response: zero-length ranging frame (no payload)
        dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK);
        dwt_writetxfctrl(0, 0, 1); // "1" = ranging

        const int ret = dwt_starttx(DWT_START_TX_DELAYED);
        if (ret != DWT_SUCCESS) {
            debug_printf("Response transmission failure");
            continue;
        }

        // Poll DW IC until TX frame sent event set
        waitforsysstatus(NULL, NULL, DWT_INT_TXFRS_BIT_MASK, 0);

        // Clear TXFRS event
        dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK);

        // Increment frame sequence number after transmission of the poll message (modulo 256).
        frame_seq_nb++;
    }
}

