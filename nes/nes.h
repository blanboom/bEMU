#ifndef NES_H
#define NES_H

#include <stdint.h>

/* 错误代码 */
#define ERR_FILE_NOT_EXIST              (-1)
#define ERR_NES_FILE_HEADER_READ_FAILED (-2)
#define ERR_MEMORY_ALLOCATE_FAILED      (-3)
#define ERR_PRG_ROM_LOAD_FAILED         (-4)
#define ERR_CHR_ROM_LOAD_FAILED         (-5)

/* 存储游戏 ROM 中的信息 */
struct _cartridge {
    /**
     * NES ROM 的 header
     * 一共有 16 字节, 每个字节定义如下:
     * 0-3 字节: 固定值, $4E $45 $53 $1A (前三字节即为 "NES")
     * 4: PRG ROM 大小, 单位为 16KB 的倍数
     * 5: CHR ROM 大小, 单位为 8KB 的倍数, 0 代表 8KB
     * 6: Flag s
     * 7: Flag 7
     * 8: PRG RAM 大小, 单位为 8KB 的倍数, 0 代表 8KB
     * 9: Flag 9
     * 10: Flag 10
     * 11-15: 统一用 0 填充
     * 对于任天堂第一方游戏, 无需考虑 Flag 6, Flag 7, Flag 9, Flag 10
     *
     * 参考资料: http://ewind.us/2015/nes-emu-3-rom-assembly/
     *          http://wiki.nesdev.com/w/index.php/INES
     */
    uint8_t header[16];
    int prg_rom_size; // PRG ROM 大小 (Byte)
    int chr_rom_size; // CHR ROM 大小 (Byte)
    int prg_ram_size; // PRG RAM 大小 (Byte)
    uint8_t *prg_rom;
    uint8_t *chr_rom;
};

extern struct _cartridge cartridge;

int nes_load_rom(char *rom);
void nes_print_rom_metadata();
void nes_exit();

#endif
