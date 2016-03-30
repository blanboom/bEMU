/* bEMU: Blanboom's NES Emulator
 *
 * 本项目是一个基本的 NES 模拟器, 具有以下功能:
 *   1. 运行部分 NES 软件
 *   2. 反汇编
 *   3. 在运行过程中可显示 CPU 和 PPU 寄存器中信息
 *
 * 详细信息：
 *   https://github.com/blanboom/bEMU
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <allegro5/allegro.h>
#include "nes/disassembler.h"
#include "nes/nes.h"
#include "emulator.h"
#include <time.h>

void arg_error(char *app_name);
static void sig_info();

int main(int argc, char *argv[]) {
    /* 判断 Arguments 的数量是否正确 */
    if(argc != 3) { arg_error(argv[0]); }

    /* 读入 NES ROM */
    int tmp;
    tmp = nes_load_rom(argv[2]);
    if(tmp != 0) {
        printf("NES rom load failed, error code: %d\n", tmp);
        exit(tmp);
    }

    /* 根据不同的选项执行对应的操作 */
    int c;
    c = getopt(argc, argv, "rdi");
    switch(c) {
        case 'r':  // 运行
            emu_init();
            signal(SIGINFO, sig_info);
            emu_run();
            nes_exit(); // 其实这一句永远不会执行
            break;
        case 'd':  // 反汇编
            disasm(cartridge.prg_rom, cartridge.prg_rom_size);
            break;
        case 'i':  // 显示 ROM 信息
            nes_print_rom_metadata();
            break;
        default:
            arg_error(argv[0]);
    }

    return 0;
}


void arg_error(char *app_name) {
    printf("Usage: %s [options] nes_rom_file\n", app_name);
    printf("Options:\n");
    printf("  -r\tRun NES emulator\n");
    printf("  -d\tRun disassembler\n");
    printf("  -i\tShow NES ROM metadata\n");
    printf("\n");
    printf("While NES emulator is running, press Ctrl + T to show debug information.\n\n");
    printf("https://github.com/blanboom/bEMU\nhttp://blanboom.org\n");
    exit(0);
}

/* 显示调试信息 */
static void sig_info() {
    /* 显示时间 */
    static time_t timer;
    static struct tm * timeinfo;
    time(&timer);
    timeinfo = localtime(&timer);
    printf("%s\n", asctime(timeinfo));

    cpu_debugger();
    ppu_debugger();
    printf("--------------------------------------------\n\n");
}
