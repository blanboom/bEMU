//
// Created by Blanboom on 16/1/12.
//

#ifndef BEMU_DISASSEMBLER_H
#define BEMU_DISASSEMBLER_H

#define DISASSEBLER_ERROR (-1)

int disasm(char *prg_rom, int length);
int disasm_once(uint8_t *prg_rom, int pc);

#endif //BEMU_DISASSEMBLER_H
