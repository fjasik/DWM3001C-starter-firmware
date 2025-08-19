#include "nrf_delay.h"
#include "usb_combined.h"

#include <boards.h>
#include <deca_spi.h>
#include <port.h>
#include <sdk_config.h>
#include <stdio.h>

//#define PINGWIN_UWB_WITH_USB_INITIATOR
//#define PINGWIN_CONFIG_INITIATOR
#define PINGWIN_CONFIG_RESPONDER

#define UART_HWFC APP_UART_FLOW_CONTROL_DISABLED
#define MAX_TEST_DATA_BYTES (15U) // max number of test bytes to be used for tx and rx
#define UART_TX_BUF_SIZE 256      // UART TX buffer size
#define UART_RX_BUF_SIZE 256      // UART RX buffer size

#if defined(PINGWIN_CONFIG_INITIATOR) && defined(PINGWIN_CONFIG_RESPONDER)
    #error "Choose your correct Pingwin configuration"
#endif

// Keep in case we want to run other examples
void test_run_info(unsigned char* data) {
    printf("%s\n", data);
}

int main(void) {
    /* Initialize all configured peripherals */
    bsp_board_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS);

    /* Initialise DWM3001C GPIO and SPI */
    gpio_init();
    dwm3001c_spi_init();
    dw_irq_init();

    /* Small pause before startup */
    nrf_delay_ms(2);

#ifdef PINGWIN_UWB_WITH_USB_INITIATOR
    int result = init_usb();
    if (result != 0) {
        while (1) { }
    }

    result = init_uwb();
    if (result != 0) {
        while (1) { }
    }

    usb_loop_with_uwb_initiator();

#endif

#ifdef PINGWIN_CONFIG_INITIATOR
    extern int pingwin_ss_twr_initiator(void);
    pingwin_ss_twr_initiator();
#endif

#ifdef PINGWIN_CONFIG_RESPONDER
    extern int pingwin_ss_twr_responder(void);
    pingwin_ss_twr_responder();
#endif

    while (1) { }
}
