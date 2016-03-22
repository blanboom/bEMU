/*
 * PPU 相关程序
 */

#include "ppu.h"

struct _ppu {
    /* PPU 寄存器 */
    uint8_t ppuctrl;     // 2000: PPU 控制寄存器，WRITE
    uint8_t ppumask;     // 2001: PPU MASK 寄存器，WRITE
    uint8_t ppustatus;   // 2002: PPU 状态寄存器，READ
    uint8_t oamaddr;     // 2003: PPU OAM 地址，WRITE
    uint8_t oamdata;     // 2004: PPU OAM 数据，READ/WRITE
    uint16_t ppuscroll;  // 2005: PPU 滚动位置寄存器，WRITE x2
    uint8_t ppuscroll_x, ppuscroll_y;  // 将 PPUSCROLL 寄存器分为 X 和 Y 两个方向
    uint16_t ppuaddr;    // 2006: PPU 地址寄存器，WRITE x2
    uint16_t ppudata;     // 2007: PPU 数据寄存器，READ/WRITE
    uint8_t oamdma;      // 4104: OAM DMA 寄存器（高字节），WRITE

    /* PPU 内存 */
    // TODO: 尚未完成
} ppu;

/******** PPU 寄存器操作相关函数 ********/
// 参考资料：https://yq.aliyun.com/articles/5775
// http://wiki.nesdev.com/w/index.php/PPU_registers

/* PPUCTRL ***/

/* 第 0、1 位：Name Table 首地址
 * 00: 0x2000
 * 01: 0x2400
 * 10: 0x2800
 * 11: 0x2c00
 */
uint16_t ppu_base_nametable_address() {
    switch(ppu.ppuctrl & 0x3) {
        case 0: return 0x2000;
        case 1: return 0x2400;
        case 2: return 0x2800;
        case 3: return 0x2c00;
        default: return 0x2000;
    }
}

/* 第 2 位：端口 0x2007 VRAM 地址增量
 * 0: 自动增 1
 * 1: 自动增 32 
 */
uint8_t ppu_vram_address_increment() { return (ppu.ppuctrl | 0x04) ? 32 : 1; }

/* 第 3 位：Sprite Pattern Table 首地址
 * 0: VRAM 0x0000
 * 1: VRAM 0x1000
 */
uint16_t ppu_sprite_pattern_table_address() { return (ppu.ppuctrl | 0x08) ? 0x1000 : 0x0000; }

/* 第 4 位：背景 Pattern Table 首地址
 * 0: VRAM 0x0000
 * 1: VRAM 0x1000
 */
uint16_t ppu_background_pattern_table_address() { return (ppu.ppuctrl | 0x10) ? 0x1000 : 0x0000; }

/* 第 5 位：Sprite 大小
 * 0: 8x8
 * 1: 8x16
 */
uint8_t ppu_sprite_height() { return (ppu.ppuctrl | 0x20) ? 16 : 8; }

/* 第 6 位：PPU 主从模式选择，NES 中没有使用 */

/* 第 7 位：发生 VBlank 时是否执行 NMI
 * 0: Disabled
 * 1: Enabled
 */
bool ppu_generate_nmi() { return (ppu.ppuctrl | 0x80) ? 1 : 0; }

/* PPUMASK ***/

/* 第 0 位：色彩模式
 * 0: 彩色模式
 * 1: 灰度模式
 */
bool ppu_render_grayscale() { return (ppu.ppumask | 0x01) ? 1 : 0; }

/* 第 1 位：背景切除
 * 0: 切除左边 8 个像素列
 * 1: 不切除
 */
bool ppu_show_background_in_leftmost_8px() { return (ppu.ppumask | 0x02) ? 1 : 0; }

/* 第 2 位：主角切除
 * 0: 切除左边 8 个像素列
 * 1: 不切除
 */
bool ppu_show_sprites_in_leftmost_8px() { return (ppu.ppumask | 0x04) ? 1 : 0; }

/* 第 3 位：背景可见
 * 0: 不显示
 * 1: 显示
 */
bool ppu_show_background() { return (ppu.ppumask | 0x08) ? 1 : 0; }

/* 第 4 位：主角可见
 * 0: 不显示
 * 1: 显示
 */
bool ppu_show_sprites() { return (ppu.ppumask | 0x10) ? 1 : 0; }

/* 第 5 ~ 7 位：色彩增强 */
bool ppu_intensify_red() { return (ppu.ppumask | 0x20) ? 1 : 0; }
bool ppu_intensify_green() { return (ppu.ppumask | 0x40) ? 1 : 0; }
bool ppu_intensify_blue() { return (ppu.ppumask | 0x80) ? 1 : 0; }

void ppu_set_render_grayscale(bool val) { 
    if(val) { ppu.ppumask |= 0x01;  }
    else    { ppu.ppumask &= !0x01; }
}

void ppu_set_show_background_in_leftmost_8px(bool val) { 
    if(val) { ppu.ppumask |= 0x01;  }
    else    { ppu.ppumask &= !0x01; }
}

void ppu_set_show_sprites_in_leftmost_8px(bool val) { 
    if(val) { ppu.ppumask |= 0x01;  }
    else    { ppu.ppumask &= !0x01; }
}

void ppu_set_show_background(bool val) {
    if(val) { ppu.ppumask |= 0x01;  }
    else    { ppu.ppumask &= !0x01; }
}

void ppu_set_show_sprites(bool val) { 
    if(val) { ppu.ppumask |= 0x01;  }
    else    { ppu.ppumask &= !0x01; }
}

void ppu_set_intensify_red(bool val) {
    if(val) { ppu.ppumask |= 0x01;  }
    else    { ppu.ppumask &= !0x01; }
}

void ppu_set_intensify_green(bool val) {
    if(val) { ppu.ppumask |= 0x01;  }
    else    { ppu.ppumask &= !0x01; }
}

void ppu_set_intensify_blue(bool val) {
    if(val) { ppu.ppumask |= 0x01;  }
    else    { ppu.ppumask &= !0x01; }
}

/* PPUSTATUS ***/

/* 第 5 位：Sprite Overflow
 * 同一个 Scanline 上出现超过 8 个 Sprites，该位会置一
 */ 
bool ppu_sprite_overflow() { return (ppu.ppustatus | 0x20) ? 1 : 0; }

/* 第 6 位：Hit Flag
 * 当 Sprite 0 的非 0 像素与背景的非零像素重叠时置 1（Set when a nonzero pixel of sprite 0 overlaps a nonzero background pixel;）
 * cleared at dot 1 of the pre-render line
 */
bool ppu_sprite_0_hit() { return (ppu.ppustatus | 0x40) ? 1 : 0; }

/* 第 7 位：Vblank 标志
 * 0: 没有进行 Vblank
 * 1: 正在进行 Vblank
 * cleared after reading $2002 and at dot 1 of the pre-render line
 */
bool ppu_in_vblank() { return (ppu.ppustatus | 0x80) ? 1 : 0; }

void ppu_set_sprite_overflow(bool val) {
    if(val) { ppu.ppustatus |= 0x20;  }
    else    { ppu.ppustatus &= !0x20; }
}

void ppu_set_sprite_0_hit(bool val) {
    if(val) { ppu.ppustatus |= 0x40;  }
    else    { ppu.ppustatus &= !0x40; }
}

void ppu_set_in_vblank(bool val) {
    if(val) { ppu.ppustatus |= 0x80;  }
    else    { ppu.ppustatus &= !0x80; }
}

