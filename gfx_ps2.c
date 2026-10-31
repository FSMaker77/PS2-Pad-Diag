/*
 * TEST DIAG - graphics layer for the PlayStation 2 (gsKit).
 *
 * The whole UI is written for a 320x224 logical screen which is scaled 2x to
 * the real 640x448 NTSC frame.  Rectangles are emitted with the triangle-strip
 * quad primitive and text uses a compact 5x10 bitmap font compiled into the
 * executable (font6x8.h), so the tool does not depend on rom0:FONTM and the
 * labels are always crisp and exactly where they are placed.
 *
 * Rendering is immediate mode on the persistent queue: every frame resets the
 * queue and rebuilds it.  Quads are used instead of gsKit sprites because the
 * sprite packet was not reliable on every GS/emulator configuration, and the
 * persistent queue (with an explicit per-frame reset) is what actually gets
 * displayed by all renderers.
 */

#include <string.h>
#include <malloc.h>
#include <gsKit.h>
#include <dmaKit.h>

#include "gfx_ps2.h"
#include "font6x8.h"

static GSGLOBAL *gs_global;

#define UI_SCALE 2
#define BG_W 640
#define BG_H 448

/* Opaque CT32 snapshots embedded at link time.  The splash is uploaded first,
 * then released so the tester background can take its place in VRAM. */
extern unsigned char controller_bg[];
extern unsigned int controller_bg_size;
extern unsigned char splash_bg[];
extern unsigned int splash_bg_size;

static GSTEXTURE bg_tex;
static u32 *bg_mem;
static u32 *splash_mem;
static int bg_needs_upload;

static void setup_texture(GSTEXTURE *tex, u32 *mem)
{
    memset(tex, 0, sizeof(*tex));
    tex->Width  = BG_W;
    tex->Height = BG_H;
    tex->PSM    = GS_PSM_CT32;
    tex->Filter = GS_FILTER_LINEAR;
    tex->Mem    = mem;
}

/* Alpha used by the solid primitives (0x80 = opaque, 0x40 = half). */
static int cur_alpha = 0x80;

void gfx_ps2_set_alpha(int alpha)
{
    if (alpha < 0) alpha = 0;
    if (alpha > 0x80) alpha = 0x80;
    cur_alpha = alpha;
}

void gfx_ps2_init(void)
{
    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);

    gs_global = gsKit_init_global();

    gs_global->Mode      = GS_MODE_NTSC;
    gs_global->Interlace = GS_INTERLACED;
    gs_global->Field     = GS_FIELD;
    gs_global->Width     = 640;
    gs_global->Height    = 448;

    /* Pure 2D UI: draw in submission order, no Z buffer, no texture filtering
     * (the font is already pixel-exact). */
    gs_global->ZBuffering     = GS_SETTING_OFF;
    gs_global->DoubleBuffering = GS_SETTING_ON;
    gs_global->PrimAlphaEnable = GS_SETTING_ON;

    gsKit_init_screen(gs_global);
    gsKit_mode_switch(gs_global, GS_PERSISTENT);
    gsKit_set_test(gs_global, GS_ZTEST_OFF);
    gsKit_set_primalpha(gs_global, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);

    /* Both images live in RAM; the same VRAM slot is reused for the splash
     * first and then for the tester background. */
    bg_mem = (u32 *)memalign(64, BG_W * BG_H * 4);
    memcpy(bg_mem, controller_bg, BG_W * BG_H * 4);

    splash_mem = (u32 *)memalign(64, BG_W * BG_H * 4);
    memcpy(splash_mem, splash_bg, BG_W * BG_H * 4);

    setup_texture(&bg_tex, splash_mem);
    gsKit_TexManager_bind(gs_global, &bg_tex);

    gsKit_clear(gs_global, GS_SETREG_RGBAQ(0, 0, 0, 0x80, 0));
    gsKit_queue_exec(gs_global);
    gsKit_sync_flip(gs_global);
}

void gfx_ps2_begin_frame(int r, int g, int b)
{
    /* The persistent queue is not reset automatically by gsKit, so drop the
     * previous frame's primitives before rebuilding the screen. */
    gsKit_queue_reset(gs_global->Per_Queue);

    /* A texture transfer queued outside a frame would be wiped by the reset
     * above, so inject it into this frame's queue before drawing. */
    if (bg_needs_upload) {
        gsKit_TexManager_bind(gs_global, &bg_tex);
        bg_needs_upload = 0;
    }

    gsKit_clear(gs_global, GS_SETREG_RGBAQ(r, g, b, 0x80, 0));
}

void gfx_ps2_end_frame(void)
{
    gsKit_queue_exec(gs_global);
    gsKit_sync_flip(gs_global);
}

void draw_rect(int x, int y, int w, int h, int r, int g, int b)
{
    float x1, y1, x2, y2;

    if (w <= 0 || h <= 0)
        return;

    x1 = (float)(x * UI_SCALE);
    y1 = (float)(y * UI_SCALE);
    x2 = (float)((x + w) * UI_SCALE);
    y2 = (float)((y + h) * UI_SCALE);

    /* Triangle-strip order: top-left, bottom-left, top-right, bottom-right. */
    gsKit_prim_quad(gs_global, x1, y1, x1, y2, x2, y1, x2, y2,
                    0, GS_SETREG_RGBAQ(r, g, b, cur_alpha, 0));
}

void gfx_ps2_draw_splash(void)
{
    gfx_ps2_set_alpha(0x80);
    gsKit_prim_quad_texture(gs_global, &bg_tex,
                            0.0f,   0.0f,   0.0f,   0.0f,
                            0.0f,   448.0f, 0.0f,   448.0f,
                            640.0f, 0.0f,   640.0f, 0.0f,
                            640.0f, 448.0f, 640.0f, 448.0f,
                            0, GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x80, 0));
}

void gfx_ps2_begin_controller(void)
{
    /* Reuse the same texture slot for the tester background.  The actual
     * transfer is injected into the next frame (see gfx_ps2_begin_frame). */
    bg_tex.Mem = bg_mem;
    gsKit_TexManager_invalidate(gs_global, &bg_tex);
    bg_needs_upload = 1;

    free(splash_mem);
    splash_mem = NULL;
}

void gfx_ps2_draw_background(void)
{
    gfx_ps2_set_alpha(0x80);
    gsKit_prim_quad_texture(gs_global, &bg_tex,
                            0.0f,   0.0f,   0.0f,   0.0f,
                            0.0f,   448.0f, 0.0f,   448.0f,
                            640.0f, 0.0f,   640.0f, 0.0f,
                            640.0f, 448.0f, 640.0f, 448.0f,
                            0, GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x80, 0));
}

void draw_frame(int x, int y, int w, int h, int r, int g, int b)
{
    draw_rect(x, y, w, 1, r, g, b);
    draw_rect(x, y + h - 1, w, 1, r, g, b);
    draw_rect(x, y, 1, h, r, g, b);
    draw_rect(x + w - 1, y, 1, h, r, g, b);
}

static int isqrt_i(int v)
{
    int r = 0;
    int b = 1 << 14;

    if (v <= 0)
        return 0;

    while (b > v)
        b >>= 2;

    while (b) {
        if (v >= r + b) {
            v -= r + b;
            r = (r >> 1) + b;
        } else {
            r >>= 1;
        }
        b >>= 2;
    }

    return r;
}

void draw_line(int x1, int y1, int x2, int y2, int r, int g, int b)
{
    int dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
    int sx = (x1 < x2) ? 1 : -1;
    int dy = -((y2 > y1) ? (y2 - y1) : (y1 - y2));
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        draw_rect(x1, y1, 1, 1, r, g, b);

        if (x1 == x2 && y1 == y2)
            break;

        {
            int e2 = err * 2;

            if (e2 >= dy) {
                err += dy;
                x1 += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y1 += sy;
            }
        }
    }
}

/* Scanline fill for a small convex polygon (used for the controller body). */
static void fill_poly(const int *px, const int *py, int n, int r, int g, int b)
{
    int ymin = py[0];
    int ymax = py[0];
    int y;
    int i;

    for (i = 1; i < n; i++) {
        if (py[i] < ymin) ymin = py[i];
        if (py[i] > ymax) ymax = py[i];
    }

    for (y = ymin; y <= ymax; y++) {
        int xs[8];
        int cnt = 0;
        int a;

        for (i = 0; i < n; i++) {
            int j = (i + 1) % n;
            int y1 = py[i];
            int y2 = py[j];

            if ((y1 <= y && y2 > y) || (y2 <= y && y1 > y)) {
                int x1 = px[i];
                int x2 = px[j];

                xs[cnt++] = x1 + (y - y1) * (x2 - x1) / (y2 - y1);
            }
        }

        /* insertion sort, many spans are tiny */
        for (a = 1; a < cnt; a++) {
            int v = xs[a];
            int k = a - 1;

            while (k >= 0 && xs[k] > v) {
                xs[k + 1] = xs[k];
                k--;
            }
            xs[k + 1] = v;
        }

        for (a = 0; a + 1 < cnt; a += 2) {
            int x0 = xs[a];
            int x1 = xs[a + 1];

            if (x1 >= x0)
                draw_rect(x0, y, x1 - x0 + 1, 1, r, g, b);
        }
    }
}

void fill_triangle(int x1, int y1, int x2, int y2, int x3, int y3,
                   int r, int g, int b)
{
    int xs[3] = { x1, x2, x3 };
    int ys[3] = { y1, y2, y3 };

    fill_poly(xs, ys, 3, r, g, b);
}

void fill_capsule(int x1, int y1, int x2, int y2, int radius, int r, int g, int b)
{
    int dx = x2 - x1;
    int dy = y2 - y1;
    int len = isqrt_i(dx * dx + dy * dy);
    int px, py;
    int xs[4], ys[4];

    if (radius <= 0)
        return;

    if (len < 1) {
        fill_disc(x1, y1, radius, r, g, b);
        return;
    }

    px = -dy * radius / len;
    py =  dx * radius / len;

    xs[0] = x1 + px; ys[0] = y1 + py;
    xs[1] = x1 - px; ys[1] = y1 - py;
    xs[2] = x2 + px; ys[2] = y2 + py;
    xs[3] = x2 - px; ys[3] = y2 - py;

    fill_poly(xs, ys, 4, r, g, b);
    fill_disc(x1, y1, radius, r, g, b);
    fill_disc(x2, y2, radius, r, g, b);
}

void fill_round_rect(int x, int y, int w, int h, int radius, int r, int g, int b)
{
    int rr;
    int yy;

    if (w <= 0 || h <= 0)
        return;

    if (radius <= 0) {
        draw_rect(x, y, w, h, r, g, b);
        return;
    }

    rr = radius;
    if (rr > w / 2) rr = w / 2;
    if (rr > h / 2) rr = h / 2;

    /* One horizontal span per row: cheap and gives a clean rounded edge. */
    for (yy = 0; yy < h; yy++) {
        int inset = 0;

        if (yy < rr) {
            int dy = rr - yy;
            inset = rr - isqrt_i(rr * rr - dy * dy);
        } else if (yy >= h - rr) {
            int dy = yy - (h - rr) + 1;
            inset = rr - isqrt_i(rr * rr - dy * dy);
        }

        draw_rect(x + inset, y + yy, w - 2 * inset, 1, r, g, b);
    }
}

void fill_disc(int cx, int cy, int radius, int r, int g, int b)
{
    int dy;

    for (dy = -radius; dy <= radius; dy++) {
        int w = isqrt_i(radius * radius - dy * dy);

        draw_rect(cx - w, cy + dy, w * 2 + 1, 1, r, g, b);
    }
}

void draw_ring(int cx, int cy, int radius, int r, int g, int b, int dots)
{
    int dy;

    for (dy = -radius; dy <= radius; dy++) {
        int w;

        if (dots && (dy & 1))
            continue;

        w = isqrt_i(radius * radius - dy * dy);
        draw_rect(cx - w, cy + dy, 2, 1, r, g, b);
        draw_rect(cx + w - 1, cy + dy, 2, 1, r, g, b);
    }
}

/* Draw one glyph at (x,y).  Each glyph is described by a handful of merged
 * rectangles, which keeps the primitive count low while staying pixel exact. */
static void draw_glyph(int x, int y, unsigned char c, int scale, int r, int g, int b)
{
    int idx;
    int i;
    int n;
    int off;

    if (c < 32 || c > 126)
        c = '?';

    idx = (int)c - 32;
    off = font_rect_off[idx];
    n   = font_rect_cnt[idx];

    for (i = 0; i < n; i++) {
        const uint8_t *rc = font_rects[off + i];

        draw_rect(x + rc[0] * scale,
                  y + rc[1] * scale,
                  rc[2] * scale,
                  rc[3] * scale,
                  r, g, b);
    }
}

static void text_at(int x, int y, const char *text, int scale, int r, int g, int b)
{
    int cx = x;

    if (!text)
        return;

    while (*text) {
        if (*text != ' ')
            draw_glyph(cx, y, (unsigned char)*text, scale, r, g, b);
        cx += FONT_ADV * scale;
        text++;
    }
}

int text_width(const char *text)
{
    return text ? (int)strlen(text) * TEXT_W : 0;
}

void text_out(int x, int y, const char *text)
{
    text_at(x, y, text, 1, 224, 230, 240);
}

void text_out_c(int x, int y, const char *text, int r, int g, int b)
{
    text_at(x, y, text, 1, r, g, b);
}

void text_center(int cx, int y, const char *text)
{
    text_at(cx - text_width(text) / 2, y, text, 1, 224, 230, 240);
}

void text_center_c(int cx, int y, const char *text, int r, int g, int b)
{
    text_at(cx - text_width(text) / 2, y, text, 1, r, g, b);
}

void text_right(int rx, int y, const char *text)
{
    text_at(rx - text_width(text), y, text, 1, 224, 230, 240);
}

void text_right_c(int rx, int y, const char *text, int r, int g, int b)
{
    text_at(rx - text_width(text), y, text, 1, r, g, b);
}

void text_big(int x, int y, const char *text, int scale, int r, int g, int b)
{
    if (scale < 1)
        scale = 1;

    text_at(x, y, text, scale, r, g, b);
}
