#include "nrf_delay.h"

#include <boards.h>
#include <deca_spi.h>
#include <port.h>
#include <sdk_config.h>
#include <stdio.h>

// This is what sets the correct configuration to be compiled
#include "configurations/configs.h"

#define UART_HWFC APP_UART_FLOW_CONTROL_DISABLED
#define MAX_TEST_DATA_BYTES                                                    \
    (15U) // max number of test bytes to be used for tx and rx
#define UART_TX_BUF_SIZE 256 // UART TX buffer size
#define UART_RX_BUF_SIZE 256 // UART RX buffer size

int main(void) {
    /* Initialize all configured peripherals */
    bsp_board_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS);

    /* Initialise DWM3001C GPIO and SPI */
    gpio_init();
    dwm3001c_spi_init();
    dw_irq_init();

    /* Small pause before startup */
    nrf_delay_ms(2);

    execute_main_program();

    while (1) {
    }
}
