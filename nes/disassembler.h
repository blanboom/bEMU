#ifndef BEMU_DISASSEMBLER_H
#define BEMU_DISASSEMBLER_H

#include <stdint.h>

#define DISASSEBLER_ERROR (-1)

int disasm(uint8_t *prg_rom, int length);
int disasm_once(uint8_t *prg_rom, int pc);

#endif //BEMU_DISASSEMBLER_H
