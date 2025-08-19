#include "configs.h"

void execute_main_program(void) {

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
    pingwin_ss_twr_initiator();
#endif

#ifdef PINGWIN_CONFIG_RESPONDER
    pingwin_ss_twr_responder();
#endif

#ifdef PINGWIN_CONFIG_RESPONDER_STS
    pingwin_ss_twr_responder_sts();
#endif

}