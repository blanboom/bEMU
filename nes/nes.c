/* 提供装在 NES ROM, NES 初始化, NES 运行等程序
 */

#include "nes.h"
#include <stdio.h>
#include <stdlib.h>

struct _cartridge cartridge;

int nes_load_rom(char *rom) {
    FILE *fp;

    /* 打开 NES ROM 文件 */
    fp = fopen(rom,"r");
    if(fp == NULL) {
        return ERR_FILE_NOT_EXIST;
    }

    /* 读取 NES ROM 的 Header */
    if(fread(cartridge.header, sizeof(uint8_t), 16, fp) != 16) {
        return ERR_NES_FILE_HEADER_READ_FAILED;
    } else {
        /* 读取 ROM 信息 */
        cartridge.prg_rom_size = 16 * 1024 * cartridge.header[4];
        cartridge.chr_rom_size = 8  * 1024 * cartridge.header[5];
        cartridge.prg_ram_size = 8  * 1024 * cartridge.header[8];
        if(cartridge.chr_rom_size == 0) { cartridge.chr_rom_size = 8 * 1024; }
        if(cartridge.prg_ram_size == 0) { cartridge.prg_ram_size = 8 * 1024; }

    }

    /* 分配内存 */
    cartridge.prg_rom = (uint8_t *)malloc((size_t)cartridge.prg_rom_size);
    cartridge.chr_rom = (uint8_t *)malloc((size_t)cartridge.chr_rom_size);
    if(cartridge.prg_rom == NULL || cartridge.chr_rom == NULL) {
        return ERR_MEMORY_ALLOCATE_FAILED;
    }

    /* 装入 PRG ROM 与 CHR ROM */
    if(fread(cartridge.prg_rom, sizeof(uint8_t), (size_t)cartridge.prg_rom_size, fp) != (size_t)cartridge.prg_rom_size) {
        return ERR_PRG_ROM_LOAD_FAILED;
    }
    if(fread(cartridge.chr_rom, sizeof(uint8_t), (size_t)cartridge.chr_rom_size, fp) != (size_t)cartridge.chr_rom_size) {
        return ERR_CHR_ROM_LOAD_FAILED;
    }

    /* 关闭 NES ROM */
    fclose(fp);

    return 0;
}

void nes_print_rom_metadata() {
    /* 显示 ROM 信息 */
    printf("ROM Metadata: =============================\n");
    printf("Sinature: %c%c%c\n", cartridge.header[0], cartridge.header[1], cartridge.header[2]);
    printf("PRG ROM Size: %d KB\n", cartridge.prg_rom_size / 1024);
    printf("CHR ROM Size: %d KB\n", cartridge.chr_rom_size / 1024);
    printf("PRG RAM Size: %d KB\n", cartridge.prg_ram_size / 1024);
    printf("==============================================\n\n");
}

void nes_exit() {
    /* 释放内存 */
    free(cartridge.prg_rom);
    free(cartridge.chr_rom);
    cartridge.prg_rom = NULL;
    cartridge.chr_rom = NULL;
}

void nes_init() {
    memory_init(cartridge.prg_rom, cartridge.prg_rom_size);

    /* 将 CHR ROM 装入 PPU 内存 */
    ppu_copy(0x0000, cartridge.chr_rom, 0x2000);

    ppu_init();
    ppu_set_mirroring(cartridge.header[6] & 1);
    cpu_init();
}
