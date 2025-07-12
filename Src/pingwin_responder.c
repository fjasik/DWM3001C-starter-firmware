#include "deca_probe_interface.h"
#include <deca_device_api.h>
#include <deca_spi.h>
#include <example_selection.h>
#include <port.h>
#include <shared_defines.h>
#include <shared_functions.h>

#define PINGWIN_SS_TWR_RESPONDER
#if defined(PINGWIN_SS_TWR_RESPONDER)

#define BEACON_NUMBER 2

extern void test_run_info(unsigned char *data);

#define APP_NAME "Pingwin Huddle UWB firmware SS TWR RESP v1.0"

/* Default communication configuration. We use default non-STS DW mode. */
static dwt_config_t config = {
    5,                /* Channel number. */
    DWT_PLEN_128,     /* Preamble length. Used in TX only. */
    DWT_PAC8,         /* Preamble acquisition chunk size. Used in RX only. */
    9,                /* TX preamble code. Used in TX only. */
    9,                /* RX preamble code. Used in RX only. */
    1,                /* 0 to use standard 8 symbol SFD, 1 to use non-standard 8 symbol, 2 for non-standard 16 symbol SFD and 3 for 4z 8 symbol SDF type */
    DWT_BR_6M8,       /* Data rate. */
    DWT_PHRMODE_STD,  /* PHY header mode. */
    DWT_PHRRATE_STD,  /* PHY header rate. */
    (129 + 8 - 8),    /* SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only. */
    DWT_STS_MODE_OFF, /* STS disabled */
    DWT_STS_LEN_64,   /* STS length see allowed values in Enum dwt_sts_lengths_e */
    DWT_PDOA_M0       /* PDOA mode off */
};

/* Default antenna delay values for 64 MHz PRF. See NOTE 2 below. */
#define TX_ANT_DLY 16385
#define RX_ANT_DLY 16385

// Pingwin defined hardcoded addresses for now (netowrk order)
// This is taken from the ss_twr_initiator.c
#define FRAME_CTRL 0x41, 0x88 // 0x8841 - 16-bit addressing
#define PAN_ID     0x48, 0x50 // PH - Pingwin Huddle
#define INIT_ADDR  0x42, 0xef // Hardcoded !
#define EMPTY_ADDR 0x00, 0x00 // Initially set to 0x00 0x00, to be replaced

#define BEACON_COUNT 3
const static uint16_t beacon_address_array[BEACON_COUNT] = {
    0xaa69, // beacon 1
    0xbb69, // beacon 2
    0xcc69  // beacon 3
};

// Notice tx and rx position swapped wrt ss_twr_initiator.c
static uint8_t rx_poll_msg[] = { FRAME_CTRL, 0, PAN_ID, EMPTY_ADDR, INIT_ADDR, 0xE0, 0, 0 };
static uint8_t tx_resp_msg[] = { FRAME_CTRL, 0, PAN_ID, INIT_ADDR, EMPTY_ADDR, 0xE1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/* Length of the common part of the message (up to and including the function code, see NOTE 3 below). */
#define ALL_MSG_COMMON_LEN 10
/* Index to access some of the fields in the frames involved in the process. */
#define ALL_MSG_SN_IDX          2
#define RESP_MSG_POLL_RX_TS_IDX 10
#define RESP_MSG_RESP_TX_TS_IDX 14
#define RESP_MSG_TS_LEN         4
/* Frame sequence number, incremented after each transmission. */
static uint8_t frame_seq_nb = 0;

/* Buffer to store received messages.
 * Its size is adjusted to longest frame that this example code is supposed to handle. */
#define RX_BUF_LEN 12 // Must be less than FRAME_LEN_MAX_EX
static uint8_t rx_buffer[RX_BUF_LEN];

/* Delay between frames, in UWB microseconds. See NOTE 1 below. */
#define POLL_RX_TO_RESP_TX_DLY_UUS 650

/* Values for the PG_DELAY and TX_POWER registers reflect the bandwidth and power of the spectrum at the current
 * temperature. These values can be calibrated prior to taking reference measurements. See NOTE 5 below. */
extern dwt_txconfig_t txconfig_options;

static void set_responder_address() {
    const uint16_t beacon_address = beacon_address_array[BEACON_NUMBER];

    // Convert the 16-bit beacon address into network order (big-endian)
    uint8_t high_byte = (beacon_address >> 8) & 0xFF;
    uint8_t low_byte = beacon_address & 0xFF;

    // Update the RESP_ADDR field in both tx_poll_msg and rx_resp_msg
    rx_poll_msg[5] = high_byte;
    rx_poll_msg[6] = low_byte;

    tx_resp_msg[7] = high_byte;
    tx_resp_msg[8] = low_byte;
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn main()
 *
 * @brief Application entry point.
 *
 * @param  none
 *
 * @return none
 */
int pingwin_ss_twr_responder(void)
{
    /* Display application name on LCD. */
    test_run_info((unsigned char *)(APP_NAME "\r\n"));

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
        test_run_info((unsigned char *)"INIT FAILED     ");
        while (1) { };
    }

    /* Enabling LEDs here for debug so that for each TX the D1 LED will flash on DW3000 red eval-shield boards. */
    dwt_setleds(DWT_LEDS_ENABLE | DWT_LEDS_INIT_BLINK);

    /* Configure DW IC. See NOTE 13 below. */
    /* if the dwt_configure returns DWT_ERROR either the PLL or RX calibration has failed the host should reset the device */
    if (dwt_configure(&config))
    {
        test_run_info((unsigned char *)"CONFIG FAILED     ");
        while (1) { };
    }

    /* Configure the TX spectrum parameters (power, PG delay and PG count) */
    dwt_configuretxrf(&txconfig_options);

    /* Apply default antenna delay value. See NOTE 2 below. */
    dwt_setrxantennadelay(RX_ANT_DLY);
    dwt_settxantennadelay(TX_ANT_DLY);

    /* Next can enable TX/RX states output on GPIOs 5 and 6 to help debug, and also TX/RX LEDs
     * Note, in real low power applications the LEDs should not be used. */
    dwt_setlnapamode(DWT_LNA_ENABLE | DWT_PA_ENABLE);

    set_responder_address();

    /* Loop forever responding to ranging requests. */
    while (1)
    {
        /* Activate reception immediately. */
        dwt_rxenable(DWT_START_RX_IMMEDIATE);

        uint32_t status_reg = 0;

        /* Poll for reception of a frame or error/timeout. See NOTE 6 below. */
        waitforsysstatus(&status_reg, NULL, (DWT_INT_RXFCG_BIT_MASK | SYS_STATUS_ALL_RX_ERR), 0);

        if (!(status_reg & DWT_INT_RXFCG_BIT_MASK))
        {
            /* Clear RX error events in the DW IC status register. */
            dwt_writesysstatuslo(SYS_STATUS_ALL_RX_ERR);
            continue;
        }

        /* Clear good RX frame event in the DW IC status register. */
        dwt_writesysstatuslo(DWT_INT_RXFCG_BIT_MASK);

        /* A frame has been received, read it into the local buffer. */
        uint16_t frame_len = dwt_getframelength();
        if (frame_len > sizeof(rx_buffer)) 
        {
            test_run_info((unsigned char *)"Frame too big\r\n");
            continue;
        }

        dwt_readrxdata(rx_buffer, frame_len, 0);

        /* Check that the frame is a poll sent by "SS TWR initiator" example.
            * As the sequence number field of the frame is not relevant, it is cleared to simplify the validation of the frame. */
        rx_buffer[ALL_MSG_SN_IDX] = 0;
        if (memcmp(rx_buffer, rx_poll_msg, ALL_MSG_COMMON_LEN) != 0)
        {
            test_run_info((unsigned char *)"Frame header mismatch\r\n");
            continue;
        }

        /* Retrieve poll reception timestamp. */
        const uint64_t poll_rx_ts = get_rx_timestamp_u64();

        test_run_info((unsigned char *)"Good frame Received\r\n");

        /* Compute response message transmission time. See NOTE 7 below. */
        const uint32_t resp_tx_time = (poll_rx_ts + (POLL_RX_TO_RESP_TX_DLY_UUS * UUS_TO_DWT_TIME)) >> 8;
        dwt_setdelayedtrxtime(resp_tx_time);

        /* Response TX timestamp is the transmission time we programmed plus the antenna delay. */
        const uint64_t resp_tx_ts = (((uint64_t)(resp_tx_time & 0xFFFFFFFEUL)) << 8) + TX_ANT_DLY;

        /* Write all timestamps in the final message. See NOTE 8 below. */
        resp_msg_set_ts(&tx_resp_msg[RESP_MSG_POLL_RX_TS_IDX], poll_rx_ts);
        resp_msg_set_ts(&tx_resp_msg[RESP_MSG_RESP_TX_TS_IDX], resp_tx_ts);

        /* Write and send the response message. See NOTE 9 below. */
        tx_resp_msg[ALL_MSG_SN_IDX] = frame_seq_nb;
        dwt_writetxdata(sizeof(tx_resp_msg), tx_resp_msg, 0); /* Zero offset in TX buffer. */
        dwt_writetxfctrl(sizeof(tx_resp_msg), 0, 1);          /* Zero offset in TX buffer, ranging. */
        const int ret = dwt_starttx(DWT_START_TX_DELAYED);

        /* If dwt_starttx() returns an error, abandon this ranging exchange and proceed to the next one. See NOTE 10 below. */
        if (ret != DWT_SUCCESS)
        {
            test_run_info((unsigned char *)"Response transmission failure\r\n");
            continue;
        }

        /* Poll DW IC until TX frame sent event set. See NOTE 6 below. */
        waitforsysstatus(NULL, NULL, DWT_INT_TXFRS_BIT_MASK, 0);

        /* Clear TXFRS event. */
        dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK);

        /* Increment frame sequence number after transmission of the poll message (modulo 256). */
        frame_seq_nb++;
    }
}
#endif