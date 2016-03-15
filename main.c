#include <stdio.h>
#include <stdlib.h>
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
    int prg_rom_size; // PRG ROM 大小 (Byte)
    int chr_rom_size; // CHR ROM 大小 (Byte)
    int prg_ram_size; // PRG RAM 大小 (Byte)
    char *prg_rom;
    char *chr_rom;
} rom;

int main(int argc, char *argv[]) {
    FILE *fp;
    char ch;

    /* 打开 NES ROM 文件 */
    fp = fopen(argv[1],"r");
    if(fp == NULL) {
        printf("ERROR: File does not exit.\n");
        exit(0);
    }

    /* 读取 NES ROM 的 Header */
    if(fread(rom.header, sizeof(char), 16, fp) != 16) {
        printf("ERROR: Header read failed.");
        exit(0);
    } else {
        /* 读取 ROM 信息 */
        rom.prg_rom_size = 16 * 1024 * rom.header[4];
        rom.chr_rom_size = 8  * 1024 * rom.header[5];
        rom.prg_ram_size = 8  * 1024 * rom.header[8];
        if(rom.chr_rom_size == 0) { rom.chr_rom_size = 8 * 1024; }
        if(rom.prg_ram_size == 0) { rom.prg_ram_size = 8 * 1024; }

        /* 显示 ROM 信息 */
        printf("ROM Information: =============================\n");
        printf("Type: %c%c%c\n", rom.header[0], rom.header[1], rom.header[2]);
        printf("PRG ROM Size: %d KB\n", rom.prg_rom_size / 1024);
        printf("CHR ROM Size: %d KB\n", rom.chr_rom_size / 1024);
        printf("PRG RAM Size: %d KB\n", rom.prg_ram_size / 1024);
        printf("==============================================\n\n");
    }

    /* 分配内存 */
    rom.prg_rom = (char *)malloc(rom.prg_rom_size);
    rom.chr_rom = (char *)malloc(rom.chr_rom_size);
    if(rom.prg_rom == NULL || rom.chr_rom == NULL) {
        printf("Memor allocate failed.\n");
        exit(0);
    }

    /* 装入 PRG ROM 与 CHR ROM */
    if(fread(rom.prg_rom, sizeof(char), rom.prg_rom_size, fp) != rom.prg_rom_size) {
        printf("ERROR: PRG ROM load failed.");
        exit(0);
    }
    if(fread(rom.chr_rom, sizeof(char), rom.chr_rom_size, fp) != rom.chr_rom_size) {
        printf("ERROR: CHR ROM load failed.");
        exit(0);
    }

    /* 反汇编 */
    disasm(rom.prg_rom, rom.prg_rom_size);

    /* 释放内存 */
    free(rom.prg_rom);
    free(rom.chr_rom);
    rom.prg_rom = NULL;
    rom.chr_rom = NULL;

    /* 关闭 NES ROM */
    fclose(fp);

    printf("Good bye!\n");

    return 0;
}