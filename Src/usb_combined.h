#ifndef USB_COMBINED_H
#define USB_COMBINED_H

int init_usb(void);
int init_uwb(void);
void usb_loop_with_uwb_initiator(void);

#endif // USB_COMBINED_H
