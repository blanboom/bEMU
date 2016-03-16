/* NES Memory Map
 * http://wiki.nesdev.com/w/index.php/CPU_memory_map
 *
 ********************************************************
 * 地址范围     | Size | 说明
 * ------------+------+----------------------------------
 * 0000 ~ 07FF | 0800 | 2KB 内部 RAM
 * 0800 ~ 0FFF | 0800 | 0000 ~ 07FF 的镜像
 * 1000 ~ 17FF | 0800 | 0000 ~ 07FF 的镜像
 * 1800 ~ 1FFF | 0800 | 0000 ~ 07FF 的镜像
 * 2000 ~ 2007 | 0008 | PPU 寄存器
 * 2008 ~ 3FFF | 1ff8 | 2000 ~ 2007 的镜像 (每 8 字节重复一次)
 * 4000 ~ 401F | 0020 | APU 与 IO 寄存器
 * 4020 ~ FFFF | BFE0 | 卡带所使用的内存区域
 * ------------+------+----------------------------------
 * 卡带使用的内存区域:
 * 4020 ~ 5FFF | 1FE0 | Expansion ROM
 * 6000 ~ 7FFF | 2000 | SRAM (Save RAM)
 * 8000 ~ FFFF | 8000 | PRG ROM
 */

/* 中断向量:
 * FFFA ~ FFFB: NMI vector
 * FFFC ~ FFFD: Reset vector
 * FFFE ~ FFFF: IRQ/BRK vector
 */

/* PPU 寄存器:
 * 2000: PPU Control Register 1
 * 2001: PPU Control Register 2
 * 2002: PPU Status Register
 * 2003: SPR-RAM Address Register
 * 2004: SPR-RAM I/O Register
 * 2005: VRAM Address Register 1
 * 2006: VRAM Address Register 2
 * 2007: VRAM I/O Register
 *
 * APU 寄存器:
 * 4000: pAPU Pulse 1 Control Register
 * 4001: pAPU Pulse 1 Ramp Control Register
 * 4002: pAPU Pulse 1 Fine Tune (FT) Register
 * 4003: pAPU Pulse 1 Coarse Tune (CT) Register
 * 4004: pAPU Pulse 2 Control Register
 * 4005: pAPU Pulse 2 Ramp Control Register
 * 4006: pAPU Pulse 2 Fine Tune Register
 * 4007: pAPU Pulse 2 Coarse Tune Register
 * 4008: pAPU Triangle Control Register 1
 * 4009: pAPU Triangle Control Register 2
 * 400A: pAPU Triangle Frequency Register 1
 * 400B: pAPU Triangle Frequency Register 2
 * 400C: pAPU Noise Control Register 1
 * 400E: pAPU Noise Frequency Register 1
 * 400F: pAPU Noise Frequency Register 2
 * 4010: pAPU Delta Modulation Control Register
 * 4011: pAPU Delta Modulation D/A Register
 * 4012: pAPU Delta Modulation Address Register
 * 4013: pAPU Delta Modulation Data Length Register
 * 4014: Sprite DMA Register
 * 4015: pAPU Sound/Vertical Clock Signal Register
 * 4016: Controller 1
 * 4017: Controller 2
 */


#include "memory.h"

uint8_t *prg_rom_ptr;
uint8_t *chr_rom_ptr;
int prg_rom_size;

uint8_t interal_ram[0x0800];  // 0000 ~ 07FF
uint8_t save_ram[0x2000];     // 6000 ~ 7FFF

void memory_init(uint8_t *prg_rom, int prg_rom_length) {
    prg_rom_ptr = prg_rom;
    prg_rom_size = prg_rom_length;
}

uint8_t memory_read_byte(uint16_t address) {
    switch(address >> 13) {
        case 0:                        // 0000 ~ 1FFF, 内部 RAM
            return interal_ram[address % 0x0800];
        case 1:                        // 2000 ~ 3FFF, PPU 寄存器
            // TODO: 尚未实现
            return 0;
        case 2:                        // APU 与 IO 寄存器
            // TODO: 尚未实现
            return 0;
        case 3:                        // Save RAM
            return save_ram[address - 0x6000];
        default:                       // PRG ROM
            return prg_rom_ptr[(address - 0x8000) % prg_rom_size];
    }
}

uint16_t memory_read_word(uint16_t address) {
    return memory_read_byte(address) + (memory_read_byte(address + 1) << 8);
}

void memory_write_byte(uint16_t address, uint8_t data) {
    if (address == 0x4014) {
        // TODO: DMA 传输
        return;
    }

    switch(address >> 13) {
        case 0:                        // 0000 ~ 1FFF, 内部 RAM
            interal_ram[address % 0x0800] = data;
        case 1:                        // 2000 ~ 3FFF, PPU 寄存器
            // TODO: 尚未实现
            return;
        case 2:                        // APU 与 IO 寄存器
            // TODO: 尚未实现
            return;
        case 3:                        // Save RAM
            save_ram[address - 0x6000] = data;
        default:                       // PRG ROM
            prg_rom_ptr[(address - 0x8000) % prg_rom_size] = data;
    }
}

void memory_write_word(uint16_t address, uint16_t data) {
    memory_write_byte(address, data & 0xFF);
    memory_write_byte(address + 1, data >> 8);
}