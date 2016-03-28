//
// Created by Blanboom on 16/3/28.
//

#include "io.h"

static byte prev_write;
static int p = 10;

byte io_read(uint16_t address) {
    // Joystick 1
    if (address == 0x4016) {
        if (p++ < 9) {
            return get_key_state(p);
        }
    }
    return 0;
}

void io_write(uint16_t address, uint8_t data) {
    if (address == 0x4016) {
        if ((data & 1) == 0 && prev_write == 1) {
            // strobe
            p = 0;
        }
    }
    prev_write = data & 1;
}