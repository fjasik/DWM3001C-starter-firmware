#if defined(PINGWIN_SS_TWR_INITIATOR)

int pingwin_ss_twr_initiator(void);

#endif

#if defined(PINGWIN_SS_TWR_RESPONDER)

int pingwin_ss_twr_responder(void);

#endif

#if defined(PINGWIN_SS_TWR_RESPONDER_STS)

int pingwin_ss_twr_responder_sts(void);

#endif

#if defined(PINGWIN_UWB_WITH_USB_INITIATOR)

int init_usb(void);
int init_uwb(void);
void usb_loop_with_uwb_initiator(void);

#endif

void execute_main_program(void);