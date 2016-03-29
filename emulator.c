#include "emulator.h"
#include "nes/nes.h"
#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>

ALLEGRO_EVENT_QUEUE *nes_event_queue;
ALLEGRO_TIMER *nes_timer = NULL;
static ALLEGRO_VERTEX vtx[1000000];
ALLEGRO_COLOR color_map[64];
int vtx_sz = 0;

/* 等待一帧结束, (allegro timer event) */
void wait_for_frame() {
    for(;;) {
        ALLEGRO_EVENT event;
        al_wait_for_event(nes_event_queue, &event);
        if (event.type == ALLEGRO_EVENT_TIMER) break;
    }
}

/* 设置背景颜色，具体的 RGB 色彩在 ppu.h 中定义 */
void set_bg_color(int c) {
    al_clear_to_color(color_map[c]);
}

/* 把 ppu.c 中 PPU 的图像输出，转移到 allegro 的缓冲区中 */
void flush_buf(PixelBuf *buf) {
    int i;
    for (i = 0; i < buf->size; i ++) {
        Pixel *p = &buf->buf[i];
        int x = p->x, y = p->y;
        ALLEGRO_COLOR c = color_map[p->color];

        vtx[vtx_sz].x = x*2; vtx[vtx_sz].y = y*2;
        vtx[vtx_sz ++].color = c;
        vtx[vtx_sz].x = x*2+1; vtx[vtx_sz].y = y*2;
        vtx[vtx_sz ++].color = c;
        vtx[vtx_sz].x = x*2; vtx[vtx_sz].y = y*2+1;
        vtx[vtx_sz ++].color = c;
        vtx[vtx_sz].x = x*2+1; vtx[vtx_sz].y = y*2+1;
        vtx[vtx_sz ++].color = c;
    }
}

void emu_init() {
    nes_init();

    al_init();
    al_init_primitives_addon();
    al_install_keyboard();
    al_create_display(SCREEN_WIDTH * 2, SCREEN_HEIGHT * 2);

    nes_timer = al_create_timer(1.0 / FPS);
    nes_event_queue = al_create_event_queue();
    al_register_event_source(nes_event_queue, al_get_timer_event_source(nes_timer));
    al_start_timer(nes_timer);
}

void flip_display() {
    al_draw_prim(vtx, NULL, NULL, 0, vtx_sz, ALLEGRO_PRIM_POINT_LIST);
    al_flip_display();
    vtx_sz = 0;
    int i;
    for(i = 0; i < 64; i++) {
        color_rgb color = palette[i];
        color_map[i] = al_map_rgb(color.r, color.g, color.b);
    }
}

int get_key_state(int b) {
    ALLEGRO_KEYBOARD_STATE state;
    al_get_keyboard_state(&state);
    switch(b) {
        case 0:  // On / Off
            return 1;
        case 1:  // A
            return al_key_down(&state, ALLEGRO_KEY_K);
        case 2:  // B
            return al_key_down(&state, ALLEGRO_KEY_J);
        case 3:  // SELECT
            return al_key_down(&state, ALLEGRO_KEY_U);
        case 4:  // START
            return al_key_down(&state, ALLEGRO_KEY_I);
        case 5:  // UP
            return al_key_down(&state, ALLEGRO_KEY_W);
        case 6:  // DOWN
            return al_key_down(&state, ALLEGRO_KEY_S);
        case 7:  // LEFT
            return al_key_down(&state, ALLEGRO_KEY_A);
        case 8:
            return al_key_down(&state, ALLEGRO_KEY_D);
        default:
            return 1;
    }
}

void emu_run() {
    for(;;) {
        wait_for_frame();
        int scanlines = 262;
        while(scanlines-- > 0) {
            // TODO: 能否用多线程进行优化？
            ppu_run(1);
            cpu_run(1364 / 12);
        }
    }
}

void emu_update_screen()
{
    int idx = ppu_ram_read(0x3f00);
    set_bg_color(idx);

    if (ppu_show_sprites())
        flush_buf(&bbg);

    if (ppu_show_background())
        flush_buf(&bg);

    if (ppu_show_sprites())
        flush_buf(&fg);

    flip_display();

    pixelbuf_clean(bbg);
    pixelbuf_clean(bg);
    pixelbuf_clean(fg);
}
