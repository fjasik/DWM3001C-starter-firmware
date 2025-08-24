#include "configs.h"

//#define PINGWIN_UWB_WITH_USB_INITIATOR
//#define PINGWIN_CONFIG_INITIATOR
//#define PINGWIN_CONFIG_RESPONDER
#define PINGWIN_CONFIG_RESPONDER_STS

void execute_main_program(void) {

#ifdef PINGWIN_UWB_WITH_USB_INITIATOR
    extern int init_usb();
    int result = init_usb();
    if (result != 0) {
        while (1) {
        }
    }

    extern int init_uwb();
    result = init_uwb();
    if (result != 0) {
        while (1) {
        }
    }

    extern int usb_loop_with_uwb_initiator();
    usb_loop_with_uwb_initiator();

#endif

#ifdef PINGWIN_CONFIG_INITIATOR
    extern int pingwin_ss_twr_initiator();
    pingwin_ss_twr_initiator();
#endif

#ifdef PINGWIN_CONFIG_RESPONDER
    extern int pingwin_ss_twr_responder();
    pingwin_ss_twr_responder();
#endif

#ifdef PINGWIN_CONFIG_RESPONDER_STS
    extern int pingwin_ss_twr_responder_sts();
    pingwin_ss_twr_responder_sts();
#endif
}
