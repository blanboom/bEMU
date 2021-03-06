/* 反汇编器
 */

#include "disassembler.h"
#include <stdio.h>

/* 根据不同的寻址方式输出不同的内容, 同时设置 pc 的变化量 */
#define ACCUMULATOR(name)  printf("%s\t A\n", name);                                  pc_delta = 1;
#define ABSOLUTE(name)     printf("%s\t $%02x%02x\n", name, opcode[2], opcode[1]);    pc_delta = 3;
#define ABSOLUTE_X(name)   printf("%s\t $%02x%02x, X\n", name, opcode[2], opcode[1]); pc_delta = 3;
#define ABSOLUTE_Y(name)   printf("%s\t $%02x%02x, Y\n", name, opcode[2], opcode[1]); pc_delta = 3;
#define IMPLIED(name)      printf("%s\n", name);                                      pc_delta = 1;
#define IMMEDIATE(name)    printf("%s\t #$%02x\n", name, opcode[1]);                  pc_delta = 2;
#define INDIRECT(name)     printf("%s\t ($%02x%02x)\n", name, opcode[2], opcode[1]);  pc_delta = 3;
#define INDIRECT_X(name)   printf("%s\t ($%02x, X)\n", name, opcode[1]);              pc_delta = 2;
#define INDIRECT_Y(name)   printf("%s\t ($%02x), Y\n", name, opcode[1]);              pc_delta = 2;
#define RELATIVE(name)     printf("%s\t $%02x\n", name, opcode[1]);                   pc_delta = 2;
#define ZERO_PAGE(name)    printf("%s\t $%02x\n", name, opcode[1]);                   pc_delta = 2;
#define ZERO_PAGE_X(name)  printf("%s\t $%02x, X\n", name, opcode[1]);                pc_delta = 2;
#define ZERO_PAGE_Y(name)  printf("%s\t $%02x, Y\n", name, opcode[1]);                pc_delta = 2;


/* 反汇编, 并通过 printf() 打印
 * 输入:
 *     prg_rom: 指向 PRG ROM 首字节的指针
 *     length:      PRG ROM 的长度
 * 输出:
 *     0: 正常返回
 */
int disasm(uint8_t *prg_rom, int length) {
    int counter = 0;
    if(length <= 0) return DISASSEBLER_ERROR;

    while(counter < length) {
        counter += disasm_once(prg_rom, counter);
    }

    return 0;
}


/* 对单条指令进行反汇编, 并通过 printf() 打印
 * 输入:
 *     prg_rom: 指向 PRG ROM 首字节的指针
 *     pc:      指令的位置
 * 输出:
 *     指令的长度, -1 代表出错
 */
int disasm_once(uint8_t *prg_rom, int pc) {
    uint8_t *opcode = &prg_rom[pc];
    int pc_delta = 1;

    switch(*opcode) {
        case 0x00: IMPLIED("BRK"); break;
        case 0x01: INDIRECT_X("ORA"); break;
        case 0x02: IMPLIED("KIL"); break;
        case 0x03: INDIRECT_X("SLO"); break;
        case 0x04: ZERO_PAGE("NOP"); break;
        case 0x05: ZERO_PAGE("ORA"); break;
        case 0x06: ZERO_PAGE("ASL"); break;
        case 0x07: ZERO_PAGE("SLO"); break;
        case 0x08: IMPLIED("PHP"); break;
        case 0x09: IMMEDIATE("ORA"); break;
        case 0x0A: ACCUMULATOR("ASL"); break;
        case 0x0B: IMMEDIATE("ANC"); break;
        case 0x0C: ABSOLUTE("NOP"); break;
        case 0x0D: ABSOLUTE("ORA"); break;
        case 0x0E: ABSOLUTE("ASL"); break;
        case 0x0F: ABSOLUTE("SLO"); break;
        case 0x10: RELATIVE("BPL"); break;
        case 0x11: INDIRECT_Y("ORA"); break;
        case 0x12: IMPLIED("KIL"); break;
        case 0x13: INDIRECT_Y("SLO"); break;
        case 0x14: ZERO_PAGE_X("NOP"); break;
        case 0x15: ZERO_PAGE_X("ORA"); break;
        case 0x16: ZERO_PAGE_X("ASL"); break;
        case 0x17: ZERO_PAGE_X("SLO"); break;
        case 0x18: IMPLIED("CLC"); break;
        case 0x19: ABSOLUTE_Y("ORA"); break;
        case 0x1A: ACCUMULATOR("NOP"); break;
        case 0x1B: ABSOLUTE_Y("SLO"); break;
        case 0x1C: ABSOLUTE_X("NOP"); break;
        case 0x1D: ABSOLUTE_X("ORA"); break;
        case 0x1E: ABSOLUTE_X("ASL"); break;
        case 0x1F: ABSOLUTE_X("SLO"); break;
        case 0x20: ABSOLUTE("JSR"); break;
        case 0x21: INDIRECT_X("AND"); break;
        case 0x22: IMPLIED("KIL"); break;
        case 0x23: INDIRECT_X("RLA"); break;
        case 0x24: ZERO_PAGE("BIT"); break;
        case 0x25: ZERO_PAGE("AND"); break;
        case 0x26: ZERO_PAGE("ROL"); break;
        case 0x27: ZERO_PAGE("RLA"); break;
        case 0x28: IMPLIED("PLP"); break;
        case 0x29: IMMEDIATE("AND"); break;
        case 0x2A: ACCUMULATOR("ROL"); break;
        case 0x2B: IMMEDIATE("ANC"); break;
        case 0x2C: ABSOLUTE("BIT"); break;
        case 0x2D: ABSOLUTE("AND");break;
        case 0x2E: ABSOLUTE("ROL"); break;
        case 0x2F: ABSOLUTE("RLA"); break;
        case 0x30: RELATIVE("BMI"); break;
        case 0x31: INDIRECT_Y("AND"); break;
        case 0x32: IMPLIED("KIL"); break;
        case 0x33: INDIRECT_Y("RLA"); break;
        case 0x34: ZERO_PAGE_X("NOP"); break;
        case 0x35: ZERO_PAGE_X("AND"); break;
        case 0x36: ZERO_PAGE_X("ROL"); break;
        case 0x37: ZERO_PAGE_X("RLA"); break;
        case 0x38: IMPLIED("SEC"); break;
        case 0x39: ABSOLUTE_Y("AND"); break;
        case 0x3A: ACCUMULATOR("NOP"); break;
        case 0x3B: ABSOLUTE_Y("RLA"); break;
        case 0x3C: ABSOLUTE_X("NOP"); break;
        case 0x3D: ABSOLUTE_X("AND"); break;
        case 0x3E: ABSOLUTE_X("ROL"); break;
        case 0x3F: ABSOLUTE_X("RLA"); break;
        case 0x40: IMPLIED("RTI"); break;
        case 0x41: INDIRECT_X("EOR"); break;
        case 0x42: IMPLIED("KIL"); break;
        case 0x43: INDIRECT_X("SRE"); break;
        case 0x44: ZERO_PAGE("NOP"); break;
        case 0x45: ZERO_PAGE("EOR"); break;
        case 0x46: ZERO_PAGE("LSR"); break;
        case 0x47: ZERO_PAGE("SRE"); break;
        case 0x48: IMPLIED("PHA"); break;
        case 0x49: IMMEDIATE("EOR"); break;
        case 0x4A: ACCUMULATOR("LSR"); break;
        case 0x4B: IMMEDIATE("ALR"); break;
        case 0x4C: ABSOLUTE("JMP"); break;
        case 0x4D: ABSOLUTE("EOR"); break;
        case 0x4E: ABSOLUTE("LSR"); break;
        case 0x4F: ABSOLUTE("SRE"); break;
        case 0x50: RELATIVE("BVC"); break;
        case 0x51: INDIRECT_Y("EOR"); break;
        case 0x52: IMPLIED("KIL"); break;
        case 0x53: INDIRECT_Y("SRE"); break;
        case 0x54: ZERO_PAGE_X("NOP"); break;
        case 0x55: ZERO_PAGE_X("EOR"); break;
        case 0x56: ZERO_PAGE_X("LSR"); break;
        case 0x57: ZERO_PAGE_X("SRE"); break;
        case 0x58: IMPLIED("CLI"); break;
        case 0x59: ABSOLUTE_Y("EOR"); break;
        case 0x5A: ACCUMULATOR("NOP"); break;
        case 0x5B: ABSOLUTE_Y("SRE"); break;
        case 0x5C: ABSOLUTE_X("NOP"); break;
        case 0x5D: ABSOLUTE_X("EOR"); break;
        case 0x5E: ABSOLUTE_X("LSR"); break;
        case 0x5F: ABSOLUTE_X("SRE"); break;
        case 0x60: IMPLIED("RTS"); break;
        case 0x61: INDIRECT_X("ADC"); break;
        case 0x62: IMPLIED("KIL"); break;
        case 0x63: INDIRECT_X("RRA"); break;
        case 0x64: ZERO_PAGE("NOP"); break;
        case 0x65: ZERO_PAGE("ADC"); break;
        case 0x66: ZERO_PAGE("ROR"); break;
        case 0x67: ZERO_PAGE("RRA"); break;
        case 0x68: IMPLIED("PLA"); break;
        case 0x69: IMMEDIATE("ADC"); break;
        case 0x6A: ACCUMULATOR("ROR"); break;
        case 0x6B: IMMEDIATE("ARR"); break;
        case 0x6C: INDIRECT("JMP"); break;
        case 0x6D: ABSOLUTE("ADC"); break;
        case 0x6E: ABSOLUTE("ROR"); break;
        case 0x6F: ABSOLUTE("RRA"); break;
        case 0x70: RELATIVE("BVS"); break;
        case 0x71: INDIRECT_Y("ADC"); break;
        case 0x72: IMPLIED("KIL"); break;
        case 0x73: INDIRECT_Y("RRA"); break;
        case 0x74: ZERO_PAGE_X("NOP"); break;
        case 0x75: ZERO_PAGE_X("ADC"); break;
        case 0x76: ZERO_PAGE_X("ROR"); break;
        case 0x77: ZERO_PAGE_X("RRA"); break;
        case 0x78: IMPLIED("SEI"); break;
        case 0x79: ABSOLUTE_Y("ADC"); break;
        case 0x7A: ACCUMULATOR("NOP"); break;
        case 0x7B: ABSOLUTE_Y("RRA"); break;
        case 0x7C: ABSOLUTE_X("NOP"); break;
        case 0x7D: ABSOLUTE_X("ADC"); break;
        case 0x7E: ABSOLUTE_X("ROR"); break;
        case 0x7F: ABSOLUTE_X("RRA"); break;
        case 0x80: IMMEDIATE("NOP"); break;
        case 0x81: INDIRECT_X("STA"); break;
        case 0x82: IMMEDIATE("NOP"); break;
        case 0x83: INDIRECT_X("SAX"); break;
        case 0x84: ZERO_PAGE("STY"); break;
        case 0x85: ZERO_PAGE("STA"); break;
        case 0x86: ZERO_PAGE("STX"); break;
        case 0x87: ZERO_PAGE("SAX"); break;
        case 0x88: IMPLIED("DEY"); break;
        case 0x89: IMMEDIATE("NOP"); break;
        case 0x8A: IMPLIED("TXA"); break;
        case 0x8B: IMMEDIATE("XAA"); break;
        case 0x8C: ABSOLUTE("STY"); break;
        case 0x8D: ABSOLUTE("STA"); break;
        case 0x8E: ABSOLUTE("STX"); break;
        case 0x8F: ABSOLUTE("SAX"); break;
        case 0x90: RELATIVE("BCC"); break;
        case 0x91: INDIRECT_Y("STA"); break;
        case 0x92: IMPLIED("KIL"); break;
        case 0x93: INDIRECT_Y("AHX"); break;
        case 0x94: ZERO_PAGE_X("STY"); break;
        case 0x95: ZERO_PAGE_X("STA"); break;
        case 0x96: ZERO_PAGE_Y("STX"); break;
        case 0x97: ZERO_PAGE_Y("SAX"); break;
        case 0x98: IMPLIED("TYA"); break;
        case 0x99: ABSOLUTE_Y("STA"); break;
        case 0x9A: IMPLIED("TXS"); break;
        case 0x9B: ABSOLUTE_Y("TAS"); break;
        case 0x9C: ABSOLUTE_X("SHY"); break;
        case 0x9D: ABSOLUTE_X("STA"); break;
        case 0x9E: ABSOLUTE_Y("SHX"); break;
        case 0x9F: ABSOLUTE_Y("AHX"); break;
        case 0xA0: IMMEDIATE("LDY");break;
        case 0xA1: INDIRECT_X("LDA"); break;
        case 0xA2: IMMEDIATE("LDX"); break;
        case 0xA3: INDIRECT_X("LAX"); break;
        case 0xA4: ZERO_PAGE("LDY"); break;
        case 0xA5: ZERO_PAGE("LDA"); break;
        case 0xA6: ZERO_PAGE("LDX"); break;
        case 0xA7: ZERO_PAGE("LAX"); break;
        case 0xA8: IMPLIED("TAY"); break;
        case 0xA9: IMMEDIATE("LDA"); break;
        case 0xAA: IMPLIED("TAX"); break;
        case 0xAB: IMMEDIATE("LAX"); break;
        case 0xAC: ABSOLUTE("LDY"); break;
        case 0xAD: ABSOLUTE("LDA"); break;
        case 0xAE: ABSOLUTE("LDX"); break;
        case 0xAF: ABSOLUTE("LAX"); break;
        case 0xB0: RELATIVE("BCS"); break;
        case 0xB1: INDIRECT_Y("LDA"); break;
        case 0xB2: IMPLIED("KIL"); break;
        case 0xB3: INDIRECT_Y("LAX"); break;
        case 0xB4: ZERO_PAGE_X("LDY"); break;
        case 0xB5: ZERO_PAGE_X("LDA"); break;
        case 0xB6: ZERO_PAGE_Y("LDX"); break;
        case 0xB7: ZERO_PAGE_Y("LAX"); break;
        case 0xB8: IMPLIED("CLV"); break;
        case 0xB9: ABSOLUTE_Y("LDA"); break;
        case 0xBA: IMPLIED("TSX"); break;
        case 0xBB: ABSOLUTE_Y("LAS"); break;
        case 0xBC: ABSOLUTE_X("LDY"); break;
        case 0xBD: ABSOLUTE_X("LDA"); break;
        case 0xBE: ABSOLUTE_Y("LDX"); break;
        case 0xBF: ABSOLUTE_Y("LAX"); break;
        case 0xC0: IMMEDIATE("CPY"); break;
        case 0xC1: INDIRECT_X("CMP"); break;
        case 0xC2: IMMEDIATE("NOP"); break;
        case 0xC3: INDIRECT_X("DCP"); break;
        case 0xC4: ZERO_PAGE("CPY"); break;
        case 0xC5: ZERO_PAGE("CMP"); break;
        case 0xC6: ZERO_PAGE("DEC"); break;
        case 0xC7: ZERO_PAGE("DCP"); break;
        case 0xC8: IMPLIED("INY"); break;
        case 0xC9: IMMEDIATE("CMP"); break;
        case 0xCA: IMPLIED("DEX"); break;
        case 0xCB: IMMEDIATE("AXS"); break;
        case 0xCC: ABSOLUTE("CPY"); break;
        case 0xCD: ABSOLUTE("CMP"); break;
        case 0xCE: ABSOLUTE("DEC"); break;
        case 0xCF: ABSOLUTE("DCP"); break;
        case 0xD0: RELATIVE("BNE"); break;
        case 0xD1: INDIRECT_Y("CMP"); break;
        case 0xD2: IMPLIED("KIL"); break;
        case 0xD3: INDIRECT_Y("DCP"); break;
        case 0xD4: ZERO_PAGE_X("NOP"); break;
        case 0xD5: ZERO_PAGE_X("CMP"); break;
        case 0xD6: ZERO_PAGE_X("DEC"); break;
        case 0xD7: ZERO_PAGE_X("DCP"); break;
        case 0xD8: IMPLIED("CLD"); break;
        case 0xD9: ABSOLUTE_Y("CMP"); break;
        case 0xDA: ACCUMULATOR("NOP"); break;
        case 0xDB: ABSOLUTE_Y("DCP"); break;
        case 0xDC: ABSOLUTE_X("NOP"); break;
        case 0xDD: ABSOLUTE_X("CMP"); break;
        case 0xDE: ABSOLUTE_X("DEC"); break;
        case 0xDF: ABSOLUTE_X("DCP"); break;
        case 0xE0: IMMEDIATE("CPX"); break;
        case 0xE1: INDIRECT_X("SBC"); break;
        case 0xE2: IMMEDIATE("NOP"); break;
        case 0xE3: INDIRECT_X("ISC"); break;
        case 0xE4: ZERO_PAGE("CPX"); break;
        case 0xE5: ZERO_PAGE("SBC"); break;
        case 0xE6: ZERO_PAGE("INC"); break;
        case 0xE7: ZERO_PAGE("ISC"); break;
        case 0xE8: IMPLIED("INX"); break;
        case 0xE9: IMMEDIATE("SBC"); break;
        case 0xEA: ACCUMULATOR("NOP"); break;
        case 0xEB: IMMEDIATE("SBC"); break;
        case 0xEC: ABSOLUTE("CPX"); break;
        case 0xED: ABSOLUTE("SBC"); break;
        case 0xEE: ABSOLUTE("INC"); break;
        case 0xEF: ABSOLUTE("ISC"); break;
        case 0xF0: RELATIVE("BEQ"); break;
        case 0xF1: INDIRECT_Y("SBC"); break;
        case 0xF2: IMPLIED("KIL"); break;
        case 0xF3: INDIRECT_Y("ISC"); break;
        case 0xF4: ZERO_PAGE_X("NOP"); break;
        case 0xF5: ZERO_PAGE_X("SBC"); break;
        case 0xF6: ZERO_PAGE_X("INC"); break;
        case 0xF7: ZERO_PAGE_X("ISC"); break;
        case 0xF8: IMPLIED("SED"); break;
        case 0xF9: ABSOLUTE_Y("SBC"); break;
        case 0xFA: ACCUMULATOR("NOP"); break;
        case 0xFB: ABSOLUTE_Y("ISC"); break;
        case 0xFC: ABSOLUTE_X("NOP"); break;
        case 0xFD: ABSOLUTE_X("SBC"); break;
        case 0xFE: ABSOLUTE_X("INC"); break;
        case 0xFF: ABSOLUTE_X("ISC"); break;
        default: printf("op		$%02x", *opcode);
    }

    return pc_delta;
}