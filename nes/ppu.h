//
// Created by Blanboom on 16/3/22.
//

#ifndef BEMU_PPU_H
#define BEMU_PPU_H

#include <stdbool.h>
#include <stdint.h>


uint8_t ppu_io_read(uint16_t address);
void ppu_io_write(uint16_t address, uint8_t data);
void ppu_sprram_write(uint8_t data);

#endif //BEMU_PPU_H
