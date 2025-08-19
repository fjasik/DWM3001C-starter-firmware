// This file contains the first version of a working
// USB CDC ACM module alongside the UWB ranging that
// can run as a standalone beacon

#include "configs.h"

// From USB
#include "../debug.h"
#include "app_error.h"
#include "app_timer.h"
#include "app_usbd.h"
#include "app_usbd_cdc_acm.h"
#include "app_usbd_serial_num.h"
#include "boards.h"
#include "nrf_drv_clock.h"
#include "nrf_drv_usbd.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

// From UWB
#include "../twr_shared.h"
#include "deca_probe_interface.h"

#include <deca_device_api.h>
#include <deca_spi.h>
#include <port.h>
#include <shared_defines.h>
#include <shared_functions.h>

#define PINGWIN_UWB_WITH_USB_INITIATOR
#if defined(PINGWIN_UWB_WITH_USB_INITIATOR)

// ----------------- UWB declaration section -----------------

#define RNG_DELAY_MS 1000

#define RX_BUF_LEN 20
static uint8_t uwb_rx_buffer[RX_BUF_LEN];

#define POLL_TX_TO_RESP_RX_DLY_UUS 240
#define RESP_RX_TIMEOUT_UUS 400

extern dwt_txconfig_t txconfig_options;

// Incremented per frame, per beacon connection
static uint8_t beacon_seq_num_array[BEACON_COUNT] = {
    0,
    0,
    0
};

// ----------------- USB declaration section -----------------

#define APP_USBD_CONFIG_EVENT_QUEUE_ENABLE 1

#define LED_USB_RESUME (BSP_BOARD_LED_0)
#define LED_CDC_ACM_OPEN (BSP_BOARD_LED_1)
#define LED_CDC_ACM_RX (BSP_BOARD_LED_2)
#define LED_CDC_ACM_TX (BSP_BOARD_LED_3)

#define BTN_CDC_DATA_SEND 0
#define BTN_CDC_NOTIFY_SEND 1

#define BTN_CDC_DATA_KEY_RELEASE (bsp_event_t)(BSP_EVENT_KEY_LAST + 1)

#ifndef USBD_POWER_DETECTION
    #define USBD_POWER_DETECTION true
#endif

static void cdc_acm_user_ev_handler(
    const app_usbd_class_inst_t* p_inst, app_usbd_cdc_acm_user_event_t event);

#define CDC_ACM_COMM_INTERFACE 0
#define CDC_ACM_COMM_EPIN NRF_DRV_USBD_EPIN2

#define CDC_ACM_DATA_INTERFACE 1
#define CDC_ACM_DATA_EPIN NRF_DRV_USBD_EPIN1
#define CDC_ACM_DATA_EPOUT NRF_DRV_USBD_EPOUT1

APP_USBD_CDC_ACM_GLOBAL_DEF(
    m_app_cdc_acm,
    cdc_acm_user_ev_handler,
    CDC_ACM_COMM_INTERFACE,
    CDC_ACM_DATA_INTERFACE,
    CDC_ACM_COMM_EPIN,
    CDC_ACM_DATA_EPIN,
    CDC_ACM_DATA_EPOUT,
    APP_USBD_CDC_COMM_PROTOCOL_AT_V250);

#define READ_SIZE 1
static char usb_rx_buffer[READ_SIZE];

void printf_to_usb(const char *format, ...);

// ----------------- UWB functions section -----------------

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

int init_uwb(void) {
    port_set_dw_ic_spi_fastrate();
    reset_DWIC();

    Sleep(2); // Time needed for DW3000 to start up

    dwt_probe((struct dwt_probe_s *)&dw3000_probe_interf);

    while (!dwt_checkidlerc()) { };
    if (dwt_initialise(DWT_DW_INIT) == DWT_ERROR)
    {
        debug_printf("UWB INIT FAILED");
        return 1;
    }

    dwt_setleds(DWT_LEDS_ENABLE | DWT_LEDS_INIT_BLINK);

    if (dwt_configure(&config))
    {
        debug_printf("UWB CONFIG FAILED");
        return 2;
    }

    dwt_configuretxrf(&txconfig_options);

    dwt_setrxantennadelay(RX_ANT_DLY);
    dwt_settxantennadelay(TX_ANT_DLY);

    dwt_setrxaftertxdelay(POLL_TX_TO_RESP_RX_DLY_UUS);
    dwt_setrxtimeout(RESP_RX_TIMEOUT_UUS);

    dwt_setlnapamode(DWT_LNA_ENABLE | DWT_PA_ENABLE);

    debug_printf("UWB initialised");

    return 0;
}

void check_uwb_sys_status(uint32_t *lo_result, uint32_t *hi_result, uint32_t lo_mask, uint32_t hi_mask)
{
    uint32_t lo_result_tmp = dwt_readsysstatuslo();  // Read the lower 32-bits

    // Check if the lower 32-bits match the mask
    if (lo_result_tmp & lo_mask)
    {
        if (lo_result != NULL)
        {
            *lo_result = lo_result_tmp;  // Assign to lo_result if not NULL
        }
    }

    uint32_t hi_result_tmp = dwt_readsysstatushi();  // Read the higher 32-bits

    // Check if the higher 32-bits match the mask
    if (hi_result_tmp & hi_mask)
    {
        if (hi_result != NULL)
        {
            *hi_result = hi_result_tmp;  // Assign to hi_result if not NULL
        }
    }
}

uint32_t check_uwb_response(void) {
    uint32_t status_reg = 0;

    check_uwb_sys_status(
        &status_reg, 
        NULL, 
        (DWT_INT_RXFCG_BIT_MASK | SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR), 
        0);

    return status_reg;
}

int send_uwb_poll_msg(int beacon_index) {
    mutate_addresses(beacon_address_array[beacon_index]);
    const int current_address = beacon_address_array[beacon_index];

    printf("Polling beacon %u (0x%04x)\r\n", beacon_index, current_address);

    poll_msg[ALL_MSG_SN_IDX] = beacon_seq_num_array[beacon_index];
    dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK);
    dwt_writetxdata(sizeof(poll_msg), poll_msg, 0);    // Zero offset in TX buffer
    dwt_writetxfctrl(sizeof(poll_msg), 0, 1);          // Zero offset in TX buffer, ranging

    // Increment frame sequence number after transmission of the poll message (modulo 256)
    beacon_seq_num_array[beacon_index]++;

    // Start transmission, indicating that a response is expected so that 
    // reception is enabled automatically after the frame is sent and the delay
    // set by dwt_setrxaftertxdelay() has elapsed
    return dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED);
}

// This function assumes a good RX was observed
uint32_t receive_uwb_resp_msg(int beacon_index) {
    const int current_address = beacon_address_array[beacon_index];
    uint16_t frame_len;

    frame_len = dwt_getframelength();
    if (frame_len > sizeof(uwb_rx_buffer)) {
        const char* message = "Frame too big";
        
        printf("Beacon 0x%04x: %s\r\n", current_address, message);
        printf_to_usb("Beacon 0x%04x: %s\r\n", current_address, message);

        return 1;
    }

    dwt_readrxdata(uwb_rx_buffer, frame_len, 0);

    // Check that the frame is the expected response from the companion responder.
    // todo: validate sequence numbers as well
    uwb_rx_buffer[ALL_MSG_SN_IDX] = 0;
    if (memcmp(uwb_rx_buffer, resp_msg, ALL_MSG_COMMON_LEN) != 0) {
        const char* message = "Frame header mismatch";

        printf("Beacon 0x%04x: %s\r\n", current_address, message);
        printf_to_usb("Beacon 0x%04x: %s\r\n", current_address, message);

        return 2;
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
    resp_msg_get_ts(&uwb_rx_buffer[RESP_MSG_POLL_RX_TS_IDX], &poll_rx_ts);
    resp_msg_get_ts(&uwb_rx_buffer[RESP_MSG_RESP_TX_TS_IDX], &resp_tx_ts);

    /* Compute time of flight and distance, using clock offset ratio to correct for differing local and remote clock rates */
    rtd_init = resp_rx_ts - poll_tx_ts;
    rtd_resp = resp_tx_ts - poll_rx_ts;

    double tof = ((rtd_init - rtd_resp * (1 - clockOffsetRatio)) / 2.0) * DWT_TIME_UNITS;
    double distance = tof * SPEED_OF_LIGHT;
    
    // Print out the distance to USB and to local debugger
    printf_to_usb("Beacon 0x%04x: %3.2f m\r\n", current_address, distance);
    printf("Beacon 0x%04x: %3.2f m\r\n", current_address, distance);

    return 0;
}

// ----------------- USB functions section -----------------

static void cdc_acm_user_ev_handler(
    const app_usbd_class_inst_t* p_inst, app_usbd_cdc_acm_user_event_t event
) {
    const app_usbd_cdc_acm_t* p_cdc_acm = app_usbd_cdc_acm_class_get(p_inst);

    switch (event) {
    case APP_USBD_CDC_ACM_USER_EVT_PORT_OPEN: {
        bsp_board_led_on(LED_CDC_ACM_OPEN);

        /*Setup first transfer*/
        ret_code_t ret =
            app_usbd_cdc_acm_read(&m_app_cdc_acm, usb_rx_buffer, READ_SIZE);
        UNUSED_VARIABLE(ret);
        break;
    }
    case APP_USBD_CDC_ACM_USER_EVT_PORT_CLOSE:
        bsp_board_led_off(LED_CDC_ACM_OPEN);
        break;
    case APP_USBD_CDC_ACM_USER_EVT_TX_DONE:
        bsp_board_led_invert(LED_CDC_ACM_TX);
        break;
    case APP_USBD_CDC_ACM_USER_EVT_RX_DONE: {
        ret_code_t ret;
        debug_printf("Bytes waiting: %d", app_usbd_cdc_acm_bytes_stored(p_cdc_acm));
        do {
            /*Get amount of data transfered*/
            size_t size = app_usbd_cdc_acm_rx_size(p_cdc_acm);
            debug_printf("RX: size: %lu char: %c", size, usb_rx_buffer[0]);

            /* Fetch data until internal buffer is empty */
            ret = app_usbd_cdc_acm_read(&m_app_cdc_acm, usb_rx_buffer, READ_SIZE);
        } while (ret == NRF_SUCCESS);

        bsp_board_led_invert(LED_CDC_ACM_RX);
        break;
    }
    default:
        break;
    }
}

static void usbd_user_ev_handler(app_usbd_event_type_t event) {
    switch (event) {
    case APP_USBD_EVT_DRV_SUSPEND:
        bsp_board_led_off(LED_USB_RESUME);
        break;
    case APP_USBD_EVT_DRV_RESUME:
        bsp_board_led_on(LED_USB_RESUME);
        break;
    case APP_USBD_EVT_STARTED:
        break;
    case APP_USBD_EVT_STOPPED:
        app_usbd_disable();
        bsp_board_leds_off();
        break;
    case APP_USBD_EVT_POWER_DETECTED:
        debug_printf("USB power detected");
        if (!nrf_drv_usbd_is_enabled()) {
            app_usbd_enable();
        }
        break;
    case APP_USBD_EVT_POWER_REMOVED:
        debug_printf("USB power removed");
        app_usbd_stop();
        break;
    case APP_USBD_EVT_POWER_READY:
        debug_printf("USB ready");
        app_usbd_start();

        break;
    default:
        break;
    }
}

void printf_to_usb(const char *format, ...) {
    static char usb_tx_buffer[NRF_DRV_USBD_EPSIZE];

    va_list args;
    va_start(args, format);
    int size = vsnprintf(usb_tx_buffer, sizeof(usb_tx_buffer), format, args);
    va_end(args);

    if (size > 0)
    {
        // Ensure we don’t send more than the buffer
        size_t safe_size = (size < NRF_DRV_USBD_EPSIZE) ? size : NRF_DRV_USBD_EPSIZE - 1;
        app_usbd_cdc_acm_write(&m_app_cdc_acm, usb_tx_buffer, safe_size);
    }
}

int init_usb(void) {
    static const app_usbd_config_t usbd_config = {
        .ev_state_proc = usbd_user_ev_handler
    };

    ret_code_t ret = nrf_drv_clock_init();
    APP_ERROR_CHECK(ret);

    nrf_drv_clock_lfclk_request(NULL);

    while (!nrf_drv_clock_lfclk_is_running()) {
        /* Just waiting */
    }

    ret = app_timer_init();
    APP_ERROR_CHECK(ret);

    //init_bsp();

    app_usbd_serial_num_generate();

    ret = app_usbd_init(&usbd_config);
    APP_ERROR_CHECK(ret);

    const app_usbd_class_inst_t* class_cdc_acm =
        app_usbd_cdc_acm_class_inst_get(&m_app_cdc_acm);
    ret = app_usbd_class_append(class_cdc_acm);
    APP_ERROR_CHECK(ret);

    if (USBD_POWER_DETECTION) {
        ret = app_usbd_power_events_enable();
        APP_ERROR_CHECK(ret);
    }
    else {
        debug_printf("No USB power detection enabled");
        debug_printf("Starting USB now");

        app_usbd_enable();
        app_usbd_start();
    }

    debug_printf("USB initialised");

    return 0;
}

// ----------------- Time -----------------

volatile uint32_t g_ms_ticks = 0;

void SysTick_Handler(void) {
    g_ms_ticks++; // increment every 1 ms
}

void init_systick_ms_timer(void) {
    // SystemCoreClock should be defined (e.g., 64000000 for 64 MHz)
    SysTick_Config(SystemCoreClock / 1000); // 1 ms interval
}

// ----------------- Main -----------------

static int current_index = BEACON_COUNT - 1;

int get_next_beacon_index(void) {
    return current_index = (current_index + 1) % BEACON_COUNT;
}

int get_current_beacon_index(void) {
    return current_index;
}

void usb_loop_with_uwb_initiator(void) {
    init_systick_ms_timer();

    uint32_t last_poll_timestamp = 0;
    bool should_send_poll = true;

    while (1) {
        while (app_usbd_event_queue_process()) {
            // Do nothing
        }

        const uint32_t now = g_ms_ticks;
        if (should_send_poll && now - last_poll_timestamp > 1000) {
            //debug_printf("Sending @ tick: %u", now);
            //printf_to_usb("Sending @ tick: %u\r\n", now);

            const int beacon_index = get_next_beacon_index();
            send_uwb_poll_msg(beacon_index);

            should_send_poll = false;
            last_poll_timestamp = now;

            // Immidiately try getting the response
            //continue;
        }

        if (should_send_poll) {
            // Wait
            continue;
        }

        const uint32_t uwb_result = check_uwb_response();

        //debug_printf("Result %u @ tick: %u", uwb_result, now);
        //printf_to_usb("Result %u @ tick: %u\r\n", uwb_result, now);

        const int current_index = get_current_beacon_index();
        const int current_address = beacon_address_array[current_index];

        // Case 0: Nothing happened (no status set or no events)
        // TBD, check what the function returns when nothing happens !
        if (uwb_result == 0) {
            continue;
        }
        else if (uwb_result & DWT_INT_RXFCG_BIT_MASK) {
            //debug_printf("Good frame received!");

            // Clear good RX frame event in the DW IC status register
            dwt_writesysstatuslo(DWT_INT_RXFCG_BIT_MASK);

            receive_uwb_resp_msg(current_index);
        }
        else if (uwb_result & SYS_STATUS_ALL_RX_ERR) {
            // Clear RX error event in the DW IC status register
            dwt_writesysstatuslo(SYS_STATUS_ALL_RX_ERR);
            
            printf("Beacon 0x%04x: error\r\n", current_address);
            printf_to_usb("Beacon 0x%04x: error\r\n", current_address);
        }
        else if (uwb_result & SYS_STATUS_ALL_RX_TO) {
            // Clear RX timeout event in the DW IC status register
            dwt_writesysstatuslo(SYS_STATUS_ALL_RX_TO);
            
            printf("Beacon 0x%04x: timeout\r\n", current_address);
            printf_to_usb("Beacon 0x%04x: timeout\r\n", current_address);
        }
        else {
            printf("Beacon 0x%04x: unknown result: 0x%08X\r\n", current_address, uwb_result);
            printf_to_usb("Beacon 0x%04x: unknown result: 0x%08X\r\n", current_address, uwb_result);
        }

        should_send_poll = true;
        //__WFE();
    }
}

#endif