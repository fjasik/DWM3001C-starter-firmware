#include "app_error.h"
#include "app_timer.h"
#include "app_usbd.h"
#include "app_usbd_cdc_acm.h"
#include "app_usbd_serial_num.h"
#include "boards.h"
#include "bsp.h"
#include "nrf_drv_clock.h"
#include "nrf_drv_usbd.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

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

static char m_rx_buffer[READ_SIZE];
static char m_tx_buffer[NRF_DRV_USBD_EPSIZE];
static bool m_send_flag = 0;

/**
 * @brief User event handler @ref app_usbd_cdc_acm_user_ev_handler_t
 * (headphones)
 * */
static void cdc_acm_user_ev_handler(
    const app_usbd_class_inst_t* p_inst, app_usbd_cdc_acm_user_event_t event) {
    const app_usbd_cdc_acm_t* p_cdc_acm = app_usbd_cdc_acm_class_get(p_inst);

    switch (event) {
    case APP_USBD_CDC_ACM_USER_EVT_PORT_OPEN: {
        bsp_board_led_on(LED_CDC_ACM_OPEN);

        /*Setup first transfer*/
        ret_code_t ret =
            app_usbd_cdc_acm_read(&m_app_cdc_acm, m_rx_buffer, READ_SIZE);
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
        printf("Bytes waiting: %d", app_usbd_cdc_acm_bytes_stored(p_cdc_acm));
        do {
            /*Get amount of data transfered*/
            size_t size = app_usbd_cdc_acm_rx_size(p_cdc_acm);
            printf("RX: size: %lu char: %c", size, m_rx_buffer[0]);

            /* Fetch data until internal buffer is empty */
            ret = app_usbd_cdc_acm_read(&m_app_cdc_acm, m_rx_buffer, READ_SIZE);
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
        printf("USB power detected");

        if (!nrf_drv_usbd_is_enabled()) {
            app_usbd_enable();
        }
        break;
    case APP_USBD_EVT_POWER_REMOVED:
        printf("USB power removed");
        app_usbd_stop();
        break;
    case APP_USBD_EVT_POWER_READY:
        printf("USB ready");
        app_usbd_start();
        break;
    default:
        break;
    }
}

static void bsp_event_callback(bsp_event_t ev) {
    ret_code_t ret;
    switch ((unsigned int)ev) {
    case CONCAT_2(BSP_EVENT_KEY_, BTN_CDC_DATA_SEND): {
        m_send_flag = 1;
        break;
    }

    case BTN_CDC_DATA_KEY_RELEASE: {
        m_send_flag = 0;
        break;
    }

    case CONCAT_2(BSP_EVENT_KEY_, BTN_CDC_NOTIFY_SEND): {
        ret = app_usbd_cdc_acm_serial_state_notify(
            &m_app_cdc_acm, APP_USBD_CDC_ACM_SERIAL_STATE_BREAK, false);
        UNUSED_VARIABLE(ret);
        break;
    }

    default:
        return; // no implementation needed
    }
}

static void init_bsp(void) {
    ret_code_t ret;
    ret = bsp_init(BSP_INIT_BUTTONS, bsp_event_callback);
    APP_ERROR_CHECK(ret);

    UNUSED_RETURN_VALUE(bsp_event_to_button_action_assign(
        BTN_CDC_DATA_SEND,
        BSP_BUTTON_ACTION_RELEASE,
        BTN_CDC_DATA_KEY_RELEASE));

    /* Configure LEDs */
    bsp_board_init(BSP_INIT_LEDS);
}

void start_pingwin_usb(void) {
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

    init_bsp();

    app_usbd_serial_num_generate();

    ret = app_usbd_init(&usbd_config);
    APP_ERROR_CHECK(ret);

    printf("Pingwin USBD CDC ACM started.\r\n");

    const app_usbd_class_inst_t* class_cdc_acm =
        app_usbd_cdc_acm_class_inst_get(&m_app_cdc_acm);
    ret = app_usbd_class_append(class_cdc_acm);
    APP_ERROR_CHECK(ret);

    if (USBD_POWER_DETECTION) {
        ret = app_usbd_power_events_enable();
        APP_ERROR_CHECK(ret);
    }
    else {
        printf("No USB power detection enabled\r\nStarting USB now");

        app_usbd_enable();
        app_usbd_start();
    }

    while (true) {
        while (app_usbd_event_queue_process()) {
            /* Nothing to do */
        }

        if (m_send_flag) {
            static int frame_counter;

            size_t size = sprintf(
                m_tx_buffer, "Pingwin USB serial device hello: %u\r\n", frame_counter);

            ret = app_usbd_cdc_acm_write(&m_app_cdc_acm, m_tx_buffer, size);
            if (ret == NRF_SUCCESS) {
                ++frame_counter;
            }
        }

        /* Sleep CPU only if there was no interrupt since last loop processing
         */
        __WFE();
    }
}
