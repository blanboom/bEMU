/*
 * PPU 相关程序
 * 参考了 https://github.com/NJUOS/LiteNES 中的代码
 */

#include "ppu.h"
#include "cpu.h"
#include "nes.h"
#include "emulator.h"
#include <string.h>

/* 像素缓冲区，存储 PPU 最终生成的图像，供外部调用
 * bg:  背景
 * bbg: 背景后的 Sprites
 * fg:  背景前的 Sprites
 */
PixelBuf bg, bbg, fg;

/* PPU 内存 */
uint8_t ppu_sprram[0x100];
uint8_t ppu_ram[0x4000];

/* 画面渲染相关 */
/* For sprite-0-hit checks*/
uint8_t ppu_screen_background[264][248];
/* Precalculated tile high and low bytes addition for pattern tables */
uint8_t ppu_l_h_addition_table[256][256][8];
uint8_t ppu_l_h_addition_flip_table[256][256][8];

bool ppu_sprite_hit_occured = false;
uint8_t ppu_latch;
bool ppu_2007_first_read;
uint8_t ppu_addr_latch;

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

    bool scroll_received_x;
    bool addr_received_high_byte;
    bool ready;

    int mirroring, mirroring_xor;

    int x, scanline;
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
uint8_t ppu_vram_address_increment() { return (ppu.ppuctrl & 0x04) ? 32 : 1; }

/* 第 3 位：Sprite Pattern Table 首地址
 * 0: VRAM 0x0000
 * 1: VRAM 0x1000
 */
uint16_t ppu_sprite_pattern_table_address() { return (ppu.ppuctrl & 0x08) ? 0x1000 : 0x0000; }

/* 第 4 位：背景 Pattern Table 首地址
 * 0: VRAM 0x0000
 * 1: VRAM 0x1000
 */
uint16_t ppu_background_pattern_table_address() { return (ppu.ppuctrl & 0x10) ? 0x1000 : 0x0000; }

/* 第 5 位：Sprite 大小
 * 0: 8x8
 * 1: 8x16
 */
uint8_t ppu_sprite_height() { return (ppu.ppuctrl & 0x20) ? 16 : 8; }

/* 第 6 位：PPU 主从模式选择，NES 中没有使用 */

/* 第 7 位：发生 VBlank 时是否执行 NMI
 * 0: Disabled
 * 1: Enabled
 */
bool ppu_generate_nmi() { return (ppu.ppuctrl & 0x80) ? true : false; }

/* PPUMASK ***/

/* 第 0 位：色彩模式
 * 0: 彩色模式
 * 1: 灰度模式
 */
bool ppu_render_grayscale() { return (ppu.ppumask & 0x01) ? true : false; }

/* 第 1 位：背景切除
 * 0: 切除左边 8 个像素列
 * 1: 不切除
 */
bool ppu_show_background_in_leftmost_8px() { return (ppu.ppumask & 0x02) ? true : false; }

/* 第 2 位：主角切除
 * 0: 切除左边 8 个像素列
 * 1: 不切除
 */
bool ppu_show_sprites_in_leftmost_8px() { return (ppu.ppumask & 0x04) ? true : false; }

/* 第 3 位：背景可见
 * 0: 不显示
 * 1: 显示
 */
bool ppu_show_background() { return (ppu.ppumask & 0x08) ? true : false; }

/* 第 4 位：主角可见
 * 0: 不显示
 * 1: 显示
 */
bool ppu_show_sprites() { return (ppu.ppumask & 0x10) ? true : false; }

/* 第 5 ~ 7 位：色彩增强 */
bool ppu_intensify_red() { return (ppu.ppumask & 0x20) ? true : false; }
bool ppu_intensify_green() { return (ppu.ppumask & 0x40) ? true : false; }
bool ppu_intensify_blue() { return (ppu.ppumask & 0x80) ? true : false; }

void ppu_set_render_grayscale(bool val) {
    if(val) { ppu.ppumask |= 0x01;  }
    else    { ppu.ppumask &= ~0x01; }
}

void ppu_set_show_background_in_leftmost_8px(bool val) {
    if(val) { ppu.ppumask |= 0x01;  }
    else    { ppu.ppumask &= ~0x01; }
}

void ppu_set_show_sprites_in_leftmost_8px(bool val) {
    if(val) { ppu.ppumask |= 0x01;  }
    else    { ppu.ppumask &= ~0x01; }
}

void ppu_set_show_background(bool val) {
    if(val) { ppu.ppumask |= 0x01;  }
    else    { ppu.ppumask &= ~0x01; }
}

void ppu_set_show_sprites(bool val) {
    if(val) { ppu.ppumask |= 0x01;  }
    else    { ppu.ppumask &= ~0x01; }
}

void ppu_set_intensify_red(bool val) {
    if(val) { ppu.ppumask |= 0x01;  }
    else    { ppu.ppumask &= ~0x01; }
}

void ppu_set_intensify_green(bool val) {
    if(val) { ppu.ppumask |= 0x01;  }
    else    { ppu.ppumask &= ~0x01; }
}

void ppu_set_intensify_blue(bool val) {
    if(val) { ppu.ppumask |= 0x01;  }
    else    { ppu.ppumask &= ~0x01; }
}

/* PPUSTATUS ***/

/* 第 5 位：Sprite Overflow
 * 同一个 Scanline 上出现超过 8 个 Sprites，该位会置一
 */
bool ppu_sprite_overflow() { return (ppu.ppustatus & 0x20) ? 1 : 0; }

/* 第 6 位：Hit Flag
 * 当 Sprite 0 的非 0 像素与背景的非零像素重叠时置 1（Set when a nonzero pixel of sprite 0 overlaps a nonzero background pixel;）
 * cleared at dot 1 of the pre-render line
 */
bool ppu_sprite_0_hit() { return (ppu.ppustatus & 0x40) ? 1 : 0; }

/* 第 7 位：Vblank 标志
 * 0: 没有进行 Vblank
 * 1: 正在进行 Vblank
 * cleared after reading $2002 and at dot 1 of the pre-render line
 */
bool ppu_in_vblank() { return (ppu.ppustatus & 0x80) ? 1 : 0; }

void ppu_set_sprite_overflow(bool val) {
    if(val) { ppu.ppustatus |= 0x20;  }
    else    { ppu.ppustatus &= ~0x20; }
}

void ppu_set_sprite_0_hit(bool val) {
    if(val) { ppu.ppustatus |= 0x40;  }
    else    { ppu.ppustatus &= ~0x40; }
}

void ppu_set_in_vblank(bool val) {
    if(val) { ppu.ppustatus |= 0x80;  }
    else    { ppu.ppustatus &= ~0x80; }
}

/******** PPU 内存操作 ********/

// 参考资料：http://wiki.nesdev.com/w/index.php/PPU_memory_map

/* PPU 具有 16KB 的内存地址空间，地址范围为：0000~3FFF
 * NES 内置了 2KB 的 PPU 内存，通常情况下映射到 2000~2FFF 中，但可以更改
 *
 * Memory Map
 * 地址范围     | 大小   | 简介
 * $0000-$0FFF | $1000 | Pattern table 0
 * $1000-$1FFF | $1000 | Pattern Table 1
 * $2000-$23FF | $0400 | Nametable 0
 * $2400-$27FF | $0400 | Nametable 1
 * $2800-$2BFF | $0400 | Nametable 2
 * $2C00-$2FFF | $0400 | Nametable 3
 * $3000-$3EFF | $0F00 | Mirrors of $2000-$2EFF
 * $3F00-$3F1F | $0020 | Palette RAM indexes
 * $3F20-$3FFF | $00E0 | Mirrors of $3F00-$3F1F
 *
 * 其中，$3F20-$3F1F 为调色板 (Palette) 内存索引，其作用如下：
 * 地址         | 用途
 * $3F00       | Universal background color
 * $3F01-$3F03 | Background palette 0
 * $3F05-$3F07 | Background palette 1
 * $3F09-$3F0B | Background palette 2
 * $3F0D-$3F0F | Background palette 3
 * $3F11-$3F13 | Sprite palette 0
 * $3F15-$3F17 | Sprite palette 1
 * $3F19-$3F1B | Sprite palette 2
 * $3F1D-$3F1F | Sprite palette 3
 *
 * 地址 $3F10/$3F14/$3F18/$3F1C 是地址 $3F00/$3F04/$3F08/$3F0C 的镜像
 */


/* 另外，PPU 还有 256 Byte 的 Object Attribute Memory (OAM)
 * CPU 可通过 OAMADDR, OAMDATA, OAMDMA 对其进行操作
 * OAM 决定了 Sprites 如何被渲染
 *
 * OAM Memory Map:
 * 地址范围          | 大小 | 简介
 * $00-$0C (0 of 4) | $40 | Sprite Y coordinate
 * $01-$0D (1 of 4) | $40 | Sprite tile #
 * $02-$0E (2 of 4) | $40 | Sprite attribute
 * $03-$0F (3 of 4) | $40 | Sprite X coordinate
 */


uint16_t ppu_get_real_ram_address(uint16_t address) {
    if(address < 0x3f00) { return address; }
    else if(address < 0x4000) {
        /* Palettes (调色板) 内存 */
        address = 0x3f00 | (address & 0x1f); // 地址 $3F20-$3FFF 是地址 3F00-$3F1F 的镜像
        if(address == 0x3f10 || address == 0x3f14 || address == 0x3f18 || address == 0x3f1c) {
            // 地址 $3F10/$3F14/$3F18/$3F1C 是地址 $3F00/$3F04/$3F08/$3F0C 的镜像
            return address - 0x10;
        } else {
            return address;
        }
    }
    return 0xffff;  // 地址错误
}

uint8_t ppu_ram_read(uint16_t address) {
    return ppu_ram[ppu_get_real_ram_address(address)];
}

void ppu_ram_write(uint16_t address, uint8_t data) {
    ppu_ram[ppu_get_real_ram_address(address)] = data;
}

/******** 图像渲染 ********/

void ppu_draw_background_scanline(bool mirror) {
    int tile_x;
    for(tile_x = (ppu_show_background_in_leftmost_8px() ? 0 : 1); tile_x < 32; tile_x++) {
        /* 跳过屏幕外的像素 */
        if(((tile_x << 3) - ppu.ppuscroll_x + (mirror ? 256 : 0)) > 256) continue;

        /* 通过 tile 在屏幕上的位置，获取 tile 在内存中的地址 */
        int tile_y = ppu.scanline >> 3;
        int tile_index = ppu_ram_read(ppu_base_nametable_address() + tile_x + (tile_y << 5) + (mirror ? 0x400 : 0));
        uint16_t tile_address = ppu_background_pattern_table_address() + 16 * tile_index;

        /* */
        int y_in_tile = ppu.scanline & 0x7;
        uint8_t l = ppu_ram_read(tile_address + y_in_tile);
        uint8_t h = ppu_ram_read(tile_address + y_in_tile + 8);

        int x;
        for(x = 0; x < 8; x++) {
            uint8_t color = ppu_l_h_addition_table[l][h][x];

            if(color != 0) {  // 颜色 0 为透明
                uint16_t attribute_address = (ppu_base_nametable_address() + (mirror ? 0x400 : 0) + 0x3c0 + (tile_x >> 2) + (ppu.scanline >> 5) * 8);
                bool top = (ppu.scanline % 32) < 16;
                bool left = (tile_x % 32) < 16;

                uint8_t palette_attribute = ppu_ram_read(attribute_address);

                if(!top) { palette_attribute >>= 4; }
                if(!left) { palette_attribute >>= 2;}
                palette_attribute &= 3;

                uint16_t palette_address = 0x3f00 + (palette_attribute << 2);
                int idx = ppu_ram_read(palette_address + color);

                ppu_screen_background[(tile_x << 3) + x][ppu.scanline] = color;

                pixelbuf_add(bg, (tile_x << 3) + x - ppu.ppuscroll_x + (mirror ? 256 : 0), ppu.scanline + 1, idx);
            }
        }
    }
}

void ppu_draw_sprite_scanline() {
    int scanline_sprite_count = 0;
    int n;
    for(n = 0; n < 0x100; n += 4) {
        uint8_t sprite_x = ppu_sprram[n + 3];
        uint8_t sprite_y = ppu_sprram[n];

        /* 跳过不在 Scanline 上的 Sprite */
        if(sprite_y > ppu.scanline || sprite_y + ppu_sprite_height() < ppu.scanline) { continue; }

        scanline_sprite_count++;

        /* 每行不能出现超过 8 个 Sprites */
        if(scanline_sprite_count > 8) { ppu_set_sprite_overflow(true); }

        bool vflip = ppu_sprram[n + 2] & 0x80;
        bool hflip = ppu_sprram[n + 2] & 0x40;

        uint16_t tile_address = ppu_sprite_pattern_table_address() + 16 * ppu_sprram[n + 1];
        int y_in_tile = ppu.scanline & 0x7;
        uint8_t l = ppu_ram_read(tile_address + (vflip ? (7 - y_in_tile) : y_in_tile));
        uint8_t h = ppu_ram_read(tile_address + (vflip ? (7 - y_in_tile) : y_in_tile) + 8);

        uint8_t palette_attribute = ppu_sprram[n + 2] & 0x3;
        uint16_t palette_address = 0x3f10 + (palette_attribute << 2);
        int x;
        for(x = 0; x < 8; x++) {
            int color = hflip ? ppu_l_h_addition_flip_table[l][h][x] : ppu_l_h_addition_table[l][h][x];

            /* color 0 为透明 */
            if(color != 0) {
                int screen_x = sprite_x + x;
                int idx = ppu_ram_read(palette_address + color);

                // http://wiki.nesdev.com/w/index.php/PPU_sprite_priority
                if(ppu_sprram[n + 2] & 0x20) { // 位于背景之后
                    pixelbuf_add(bbg, screen_x, sprite_y + y_in_tile + 1, idx);
                } else {                       // 位于背景之前
                    pixelbuf_add(fg, screen_x, sprite_y + y_in_tile + 1, idx);
                }

                /* 检查是否发生 sprite 0 hit, 并更新寄存器 */
                if(ppu_show_background() && !ppu_sprite_hit_occured && n == 0 && ppu_screen_background[screen_x][sprite_y + y_in_tile] == color) {
                    ppu_set_sprite_0_hit(true);
                    ppu_sprite_hit_occured = true;
                }
            }
        }
    }
}


/******** PPU Lifecycle ********/

void ppu_cycle() {
    // TODO: 换回 if(!ppu.ready && cpu_clock() > 29658) { ppu.ready = true; }
    if(!ppu.ready && cpu_clock() > 1) { ppu.ready = true; }
    ppu.scanline++;
    if(ppu_show_background()) {
        ppu_draw_background_scanline(false);
        ppu_draw_background_scanline(true);
    }
    if(ppu_show_sprites()) { ppu_draw_sprite_scanline(); }
    if(ppu.scanline == 241) {
        ppu_set_in_vblank(true);
        ppu_set_sprite_0_hit(false);
        cpu_interrupt();
    } else if(ppu.scanline == 262) {
        ppu.scanline = -1;
        ppu_sprite_hit_occured = false;
        ppu_set_in_vblank(false);
        /* 一帧画面扫描结束，刷新屏幕 */
        emu_update_screen();
    }
}

void ppu_run(int cycles) {
    while(cycles-- > 0) {
        ppu_cycle();
    }
}

void ppu_copy(uint16_t address, uint8_t *source, int length) {
    memcpy(&ppu_ram[address], source, length);
}

uint8_t ppu_io_read(uint16_t address) {
    uint8_t data; uint16_t value;
    ppu.ppuaddr &= 0x3fff;
    switch(address & 7) {
        case 2:
            value = ppu.ppustatus;
            ppu_set_in_vblank(false);
            ppu_set_sprite_0_hit(false);
            ppu.scroll_received_x = 0;
            ppu.ppuscroll = 0;
            ppu.addr_received_high_byte = 0;
            ppu_latch = value;
            ppu_addr_latch = 0;
            ppu_2007_first_read = true;
            return value;
        case 4:
            return ppu_latch = ppu_sprram[ppu.oamaddr];
        case 7:
            if(ppu.ppuaddr < 0x3f00) {
                data = ppu_ram_read(ppu.ppuaddr);
                ppu_latch = 0;
            } else {
                data = ppu_ram_read(ppu.ppuaddr);
                ppu_latch = 0;
            }
            if(ppu_2007_first_read) {
                ppu_2007_first_read = false;
            } else {
                ppu.ppuaddr += ppu_vram_address_increment();
            }
            return data;
        default:
            return 0xff;
    }
}

void ppu_io_write(uint16_t address, uint8_t data) {
    address &= 7;
    ppu_latch = data;
    ppu.ppuaddr &= 0x3fff;
    switch(address) {
        case 0: if(ppu.ready) { ppu.ppuctrl = data; } break;
        case 1: if(ppu.ready) { ppu.ppumask = data; } break;
        case 3: ppu.oamaddr = data; break;
        case 4: ppu_sprram[ppu.oamaddr++] = data; break;
        case 5:
            if(ppu.scroll_received_x) { ppu.ppuscroll_y = data; }
            else { ppu.ppuscroll_x = data; }
            ppu.scroll_received_x ^= 1;
            break;
        case 6:
            if(!ppu.ready) { return; }
            if(ppu.addr_received_high_byte) { ppu.ppuaddr = (ppu_addr_latch << 8) + data; }
            else { ppu_addr_latch = data; }
            ppu.addr_received_high_byte ^= 1;
            ppu_2007_first_read = true;
            break;
        case 7:
            if(ppu.ppuaddr > 0x1fff || ppu.ppuaddr < 0x4000) {
                ppu_ram_write(ppu.ppuaddr ^ ppu.mirroring_xor, data);
                ppu_ram_write(ppu.ppuaddr, data);
            } else {
                ppu_ram_write(ppu.ppuaddr, data);
            }
    }
    ppu_latch = data;
}

void ppu_init() {
    ppu.ppuctrl = 0; ppu.ppumask = 0; ppu.ppustatus = 0; ppu.oamaddr = 0;
    ppu.ppuscroll = 0; ppu.ppuscroll_x = 0; ppu.ppuscroll_y = 0; ppu.ppuaddr = 0;
    ppu.ppustatus |= 0xa0;
    ppu.ppudata = 0;
    ppu_2007_first_read = 0;

    ppu.ready = false;

    int l, h, x;
    for(h = 0; h < 0x100; h++) {
        for(l = 0; l < 0x100; l++) {
            for(x = 0; x < 8; x++) {
                ppu_l_h_addition_table[l][h][x] = (((h >> (7 - x)) & 1) << 1) | ((l >> (7 - x)) & 1);
                ppu_l_h_addition_flip_table[l][h][x] = (((h >> x) & 1) << 1) | ((l >> x) & 1);
            }
        }
    }
}

void ppu_sprram_write(uint8_t data) {
    ppu_sprram[ppu.oamaddr++] = data;
}

void ppu_set_background_color(uint8_t color) {
    set_bg_color(color);
}

void ppu_set_mirroring(uint8_t mirroring) {
    ppu.mirroring = mirroring;
    ppu.mirroring_xor = 0x400 << mirroring;
}
