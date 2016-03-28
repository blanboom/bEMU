//
// Created by Blanboom on 16/3/28.
//

#ifndef BEMU_IO_H
#define BEMU_IO_H

#include <stdint.h>

uint8_t io_read(uint16_t address);
void io_write(uint16_t address, uint8_t data);

#endif //BEMU_IO_H
