#ifndef USB_CDC_H
#define USB_CDC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "app_usbd_cdc_acm.h"

/**
 * @brief Initialize USB CDC ACM
 *
 * Initializes clocks, USB stack, and CDC ACM class.
 * Must be called once at startup.
 */
void usb_cdc_init(void);

/**
 * @brief Write data over USB CDC ACM
 *
 * @param data Pointer to data buffer
 * @param len  Number of bytes to write
 * @return NRF_SUCCESS if write queued, error code otherwise
 */
ret_code_t usb_cdc_write(const char *data, size_t len);

/**
 * @brief Process USB events
 *
 * Must be called regularly to handle USB stack events.
 */
void usb_cdc_process(void);

/**
 * @brief Set flag to send periodic data
 *
 * @param enable True to enable sending, false to disable
 */
void usb_cdc_set_send_flag(bool enable);

void start_pingwin_usb(void);

#endif // USB_CDC_H
