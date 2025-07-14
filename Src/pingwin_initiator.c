#include "debug.h"
#include "deca_probe_interface.h"
#include "twr_shared.h"

#include <config_options.h>
#include <deca_device_api.h>
#include <deca_spi.h>
#include <example_selection.h>
#include <port.h>
#include <shared_defines.h>
#include <shared_functions.h>

#if defined(PINGWIN_SS_TWR_INITIATOR)

/* Inter-ranging delay period, in milliseconds. */
#define RNG_DELAY_MS 1000

/* Buffer to store received response message.
 * Its size is adjusted to longest frame that this example code is supposed to handle. */
#define RX_BUF_LEN 20
static uint8_t rx_buffer[RX_BUF_LEN];

/* Delay between frames, in UWB microseconds. See NOTE 1 below. */
#define POLL_TX_TO_RESP_RX_DLY_UUS 240
/* Receive response timeout. See NOTE 5 below. */
#define RESP_RX_TIMEOUT_UUS 400

/* Values for the PG_DELAY and TX_POWER registers reflect the bandwidth and power of the spectrum at the current
 * temperature. These values can be calibrated prior to taking reference measurements. See NOTE 2 below. */
extern dwt_txconfig_t txconfig_options;

// Function to mutate tx_poll_msg and rx_resp_msg based on the beacon address
static void mutate_addresses(uint16_t beacon_address) {
    // Convert the 16-bit beacon address into network order (big-endian)
    uint8_t high_byte = (beacon_address >> 8) & 0xFF;
    uint8_t low_byte = beacon_address & 0xFF;

    // Update the RESP_ADDR field in both tx_poll_msg and rx_resp_msg
    poll_msg[5] = high_byte;
    poll_msg[6] = low_byte;

    resp_msg[7] = high_byte;
    resp_msg[8] = low_byte;
}

// Function to return the pointer to tx_poll_msg
static uint8_t* get_tx_poll_msg(uint16_t beacon_address) {
    mutate_addresses(beacon_address);
    return poll_msg;
}

// Function to return the pointer to rx_resp_msg
static uint8_t* get_rx_resp_msg(uint16_t beacon_address) {
    mutate_addresses(beacon_address);
    return resp_msg;
}

// Incremented per frame, per beacon connection
static uint8_t beacon_seq_num_array[BEACON_COUNT] = {
    0,
    0,
    0
};

int pingwin_ss_twr_initiator(void)
{
    /* Display application name on LCD. */
    debug_printf("Pingwin Huddle UWB firmware SS TWR INIT v1.0");

    /* Configure SPI rate, DW3000 supports up to 36 MHz */
    port_set_dw_ic_spi_fastrate();

    /* Reset and initialize DW chip. */
    reset_DWIC(); /* Target specific drive of RSTn line into DW3000 low for a period. */

    Sleep(2); // Time needed for DW3000 to start up (transition from INIT_RC to IDLE_RC, or could wait for SPIRDY event)

    /* Probe for the correct device driver. */
    dwt_probe((struct dwt_probe_s *)&dw3000_probe_interf);

    while (!dwt_checkidlerc()) /* Need to make sure DW IC is in IDLE_RC before proceeding */ { };
    if (dwt_initialise(DWT_DW_INIT) == DWT_ERROR)
    {
        debug_printf("INIT FAILED     ");
        while (1) { };
    }

    /* Enabling LEDs here for debug so that for each TX the D1 LED will flash on DW3000 red eval-shield boards. */
    dwt_setleds(DWT_LEDS_ENABLE | DWT_LEDS_INIT_BLINK);

    /* Configure DW IC. See NOTE 13 below. */
    /* if the dwt_configure returns DWT_ERROR either the PLL or RX calibration has failed the host should reset the device */
    if (dwt_configure(&config))
    {
        debug_printf("CONFIG FAILED     ");
        while (1) { };
    }

    /* Configure the TX spectrum parameters (power, PG delay and PG count) */
    dwt_configuretxrf(&txconfig_options);

    /* Apply default antenna delay value. See NOTE 2 below. */
    dwt_setrxantennadelay(RX_ANT_DLY);
    dwt_settxantennadelay(TX_ANT_DLY);

    /* Set expected response's delay and timeout. See NOTE 1 and 5 below.
     * As this example only handles one incoming frame with always the same delay and timeout, those values can be set here once for all. */
    dwt_setrxaftertxdelay(POLL_TX_TO_RESP_RX_DLY_UUS);
    dwt_setrxtimeout(RESP_RX_TIMEOUT_UUS);

    /* Next can enable TX/RX states output on GPIOs 5 and 6 to help debug, and also TX/RX LEDs
     * Note, in real low power applications the LEDs should not be used. */
    dwt_setlnapamode(DWT_LNA_ENABLE | DWT_PA_ENABLE);

    // We're doing ghetto programming here
    char output_buffer[32] = { 0 };

    while (1)
    {
        for (int beacon_index = 0; beacon_index < BEACON_COUNT; beacon_index++) 
        {
            mutate_addresses(beacon_address_array[beacon_index]);

            snprintf(output_buffer, sizeof(output_buffer), "Beacon: %u, ", beacon_index);
            printf(output_buffer);

            /* Write frame data to DW IC and prepare transmission. See NOTE 7 below. */
            poll_msg[ALL_MSG_SN_IDX] = beacon_seq_num_array[beacon_index];
            dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK);
            dwt_writetxdata(sizeof(poll_msg), poll_msg, 0); /* Zero offset in TX buffer. */
            dwt_writetxfctrl(sizeof(poll_msg), 0, 1);          /* Zero offset in TX buffer, ranging. */

            /* Start transmission, indicating that a response is expected so that reception is enabled automatically after the frame is sent and the delay
            * set by dwt_setrxaftertxdelay() has elapsed. */
            dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED);

            uint32_t status_reg = 0;

            /* We assume that the transmission is achieved correctly, poll for reception of a frame or error/timeout. See NOTE 8 below. */
            waitforsysstatus(&status_reg, NULL, (DWT_INT_RXFCG_BIT_MASK | SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR), 0);

            /* Increment frame sequence number after transmission of the poll message (modulo 256). */
            beacon_seq_num_array[beacon_index]++;

            if (!(status_reg & DWT_INT_RXFCG_BIT_MASK))
            {
                /* Clear RX error/timeout events in the DW IC status register. */
                dwt_writesysstatuslo(SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR);
                debug_printf("Timeout or error");
                continue;
            }

            uint16_t frame_len;

            /* Clear good RX frame event in the DW IC status register. */
            dwt_writesysstatuslo(DWT_INT_RXFCG_BIT_MASK);

            frame_len = dwt_getframelength();
            if (frame_len > sizeof(rx_buffer))
            {
                debug_printf("Frame too big");
                continue;
            }

            /* A frame has been received, read it into the local buffer. */
            dwt_readrxdata(rx_buffer, frame_len, 0);

            /* Check that the frame is the expected response from the companion "SS TWR responder" example.
                * As the sequence number field of the frame is not relevant, it is cleared to simplify the validation of the frame. */
            rx_buffer[ALL_MSG_SN_IDX] = 0;
            if (memcmp(rx_buffer, resp_msg, ALL_MSG_COMMON_LEN) != 0)
            {
                debug_printf("Frame header mismatch");
                continue;
            }

            uint32_t poll_tx_ts, resp_rx_ts, poll_rx_ts, resp_tx_ts;
            int32_t rtd_init, rtd_resp;
            float clockOffsetRatio;

            /* Retrieve poll transmission and response reception timestamps. See NOTE 9 below. */
            poll_tx_ts = dwt_readtxtimestamplo32();
            resp_rx_ts = dwt_readrxtimestamplo32();

            /* Read carrier integrator value and calculate clock offset ratio. See NOTE 11 below. */
            clockOffsetRatio = ((float)dwt_readclockoffset()) / (uint32_t)(1 << 26);

            /* Get timestamps embedded in response message. */
            resp_msg_get_ts(&rx_buffer[RESP_MSG_POLL_RX_TS_IDX], &poll_rx_ts);
            resp_msg_get_ts(&rx_buffer[RESP_MSG_RESP_TX_TS_IDX], &resp_tx_ts);

            /* Compute time of flight and distance, using clock offset ratio to correct for differing local and remote clock rates */
            rtd_init = resp_rx_ts - poll_tx_ts;
            rtd_resp = resp_tx_ts - poll_rx_ts;

            double tof = ((rtd_init - rtd_resp * (1 - clockOffsetRatio)) / 2.0) * DWT_TIME_UNITS;
            double distance = tof * SPEED_OF_LIGHT;

            /* Display computed distance on LCD. */
            snprintf(output_buffer, sizeof(output_buffer), "Distance: %3.2f m", distance);
            printf(output_buffer);
        }

        Sleep(RNG_DELAY_MS);        
    }
}
#endif
