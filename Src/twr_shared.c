#include "twr_shared.h"

uint8_t poll_msg[] = {FRAME_CTRL, 0, PAN_ID, EMPTY_ADDR, INIT_ADDR, 0xE0, 0, 0};
uint8_t resp_msg[] = {
    FRAME_CTRL,
    0,
    PAN_ID,
    INIT_ADDR,
    EMPTY_ADDR,
    0xE1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0};

const uint16_t beacon_address_array[BEACON_COUNT] = {
    0xaa69, // beacon 1
    0xbb69, // beacon 2
    0xcc69  // beacon 3
};
