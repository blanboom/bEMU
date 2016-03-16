//
// Created by Blanboom on 16/3/16.
//

#ifndef BEMU_MEMORY_H
#define BEMU_MEMORY_H

#include <stdint.h>

void memory_init(char *prg_rom);
uint8_t memory_read_byte(uint16_t address);
uint16_t memory_read_word(uint16_t address);
void memory_write_byte(uint16_t address, uint8_t data);
void memory_write_word(uint16_t address, uint16_t data);

#endif //BEMU_MEMORY_H
