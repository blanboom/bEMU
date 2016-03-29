/* bEMU: Blanboom's NES Emulator
 *
 * 本项目是一个基本的 NES 模拟器, 具有以下功能:
 *   1. 运行部分 NES 软件
 *   2. 反汇编
 *   3. 在运行过程中可显示 CPU 和 PPU 寄存器中信息
 */

#include <stdio.h>
#include <stdlib.h>
#include <allegro5/allegro.h>
#include "nes/disassembler.h"
#include "nes/nes.h"
#include "emulator.h"
#include "stdio.h"

int main(int argc, char *argv[]) {
    int tmp;

    /* 读入 NES ROM */
    tmp = nes_load_rom(argv[1]);
    if(tmp != 0) {
        printf("NES rom load failed, error code: %d", tmp);
        exit(0);
    }

    /* 显示 NES ROM 的相关信息 */
    //nes_print_rom_metadata();

    /* 反汇编 */
    //disasm(cartridge.prg_rom, cartridge.prg_rom_size);

    emu_init();
    emu_run();

    // TODO: 如果收到 SIGINT，也应该执行 nes_exit()
    nes_exit();
    return 0;
}
