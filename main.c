#include <stdio.h>
#include "nes/disassembler.h"

/**
 * 存储游戏 ROM 中的信息
 */
struct rom_info {
    /**
     * NES ROM 的 header
     * 一共有 16 字节, 每个字节定义如下:
     * 0-3 字节: 固定值, $4E $45 $53 $1A (前三字节即为 "NES")
     * 4: PRG ROM 大小, 单位为 16KB 的倍数
     * 5: CHR ROM 大小, 单位为 8KB 的倍数, 0 代表 8KB
     * 6: Flag 6
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
    char header[16];
    int prg_rom_size; // PRG ROM 大小 (KB)
    int chr_rom_size; // CHR ROM 大小 (KB)
    int prg_ram_size; // PRG RAM 大小 (KB)
    char *chr_rom;
    char *prg_rom;
} rom;

int main(int argc, char *argv[]) {
    FILE *fp;
    char ch;

    /* 打开 NES ROM 文件 */
    fp = fopen(argv[1],"r");
    if(fp == NULL) {
        printf("ERROR: File does not exit.\n");
        return 0;
    }

    /* 读取 NES ROM 的 Header */
    if(fread(rom.header, sizeof(char), 16, fp) != 16) {
        printf("ERROR: Header read failed.");
        return 0;
    } else {
        rom.prg_rom_size = 16 * rom.header[4];
        rom.chr_rom_size = 8 * rom.header[5];
        rom.prg_ram_size = 8 * rom.header[8];
        if(rom.chr_rom_size == 0) { rom.chr_rom_size = 8; }
        if(rom.prg_ram_size == 0) { rom.prg_ram_size = 8; }

        printf("ROM Information: =============================\n");
        printf("Type: %c%c%c\n", rom.header[0], rom.header[1], rom.header[2]);
        printf("PRG ROM Size: %d KB\n", rom.prg_rom_size);
        printf("CHR ROM Size: %d KB\n", rom.chr_rom_size);
        printf("PRG RAM Size: %d KB\n", rom.prg_ram_size);
        printf("==============================================\n\n");
    }

    /* 关闭 NES ROM */
    fclose(fp);


    printf("Hello NES!");
    return 0;
}