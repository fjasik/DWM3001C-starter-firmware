#ifndef USB_CDC_H
#define USB_CDC_H

#include "sdk_errors.h"

ret_code_t write_to_usb(char* buffer, size_t size);
void start_pingwin_usb(void);
void usb_spin(void);

#endif // USB_CDC_H
