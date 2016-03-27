#include <stdio.h>
#include <stdlib.h>
#include "nes/disassembler.h"
#include "nes/nes.h"


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
