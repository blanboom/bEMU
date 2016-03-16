#include <stdio.h>
#include <stdlib.h>
#include "nes/disassembler.h"

/**
 * 存储游戏 ROM 中的信息
 */
struct _cartridge {
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
    uint8_t header[16];
    int prg_rom_size; // PRG ROM 大小 (Byte)
    int chr_rom_size; // CHR ROM 大小 (Byte)
    int prg_ram_size; // PRG RAM 大小 (Byte)
    uint8_t *prg_rom;
    uint8_t *chr_rom;
} cartridge;

int main(int argc, char *argv[]) {
    FILE *fp;

    /* 打开 NES ROM 文件 */
    fp = fopen(argv[1],"r");
    if(fp == NULL) {
        printf("ERROR: File does not exit.\n");
        exit(0);
    }

    /* 读取 NES ROM 的 Header */
    if(fread(cartridge.header, sizeof(uint8_t), 16, fp) != 16) {
        printf("ERROR: Header read failed.");
        exit(0);
    } else {
        /* 读取 ROM 信息 */
        cartridge.prg_rom_size = 16 * 1024 * cartridge.header[4];
        cartridge.chr_rom_size = 8  * 1024 * cartridge.header[5];
        cartridge.prg_ram_size = 8  * 1024 * cartridge.header[8];
        if(cartridge.chr_rom_size == 0) { cartridge.chr_rom_size = 8 * 1024; }
        if(cartridge.prg_ram_size == 0) { cartridge.prg_ram_size = 8 * 1024; }

        /* 显示 ROM 信息 */
        printf("ROM Metadata: =============================\n");
        printf("Sinature: %c%c%c\n", cartridge.header[0], cartridge.header[1], cartridge.header[2]);
        printf("PRG ROM Size: %d KB\n", cartridge.prg_rom_size / 1024);
        printf("CHR ROM Size: %d KB\n", cartridge.chr_rom_size / 1024);
        printf("PRG RAM Size: %d KB\n", cartridge.prg_ram_size / 1024);
        printf("==============================================\n\n");
    }

    /* 分配内存 */
    cartridge.prg_rom = (uint8_t *)malloc((size_t)cartridge.prg_rom_size);
    cartridge.chr_rom = (uint8_t *)malloc((size_t)cartridge.chr_rom_size);
    if(cartridge.prg_rom == NULL || cartridge.chr_rom == NULL) {
        printf("Memor allocate failed.\n");
        exit(0);
    }

    /* 装入 PRG ROM 与 CHR ROM */
    if(fread(cartridge.prg_rom, sizeof(uint8_t), (size_t)cartridge.prg_rom_size, fp) != (size_t)cartridge.prg_rom_size) {
        printf("ERROR: PRG ROM load failed.");
        exit(0);
    }
    if(fread(cartridge.chr_rom, sizeof(uint8_t), (size_t)cartridge.chr_rom_size, fp) != (size_t)cartridge.chr_rom_size) {
        printf("ERROR: CHR ROM load failed.");
        exit(0);
    }

    /* 反汇编 */
    disasm(cartridge.prg_rom, cartridge.prg_rom_size);

    /* 释放内存 */
    free(cartridge.prg_rom);
    free(cartridge.chr_rom);
    cartridge.prg_rom = NULL;
    cartridge.chr_rom = NULL;

    /* 关闭 NES ROM */
    fclose(fp);

    printf("Good bye!\n");

    return 0;
}