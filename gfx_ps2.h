#ifndef GFX_PS2_H
#define GFX_PS2_H

#include <stdint.h>

/* The UI is designed on a 320x224 grid (the visible area of a 640x448 NTSC
 * frame).  Every coordinate below is in that grid; the GS layer scales it 2x.
 * Keep at least a 6px margin because CRT televisions crop the outer edges. */
#define SCREEN_W 320
#define SCREEN_H 224

/* Built-in text metrics: 5x10 glyphs, 6px advance. */
#define TEXT_W   6
#define TEXT_H   10

void gfx_ps2_init(void);
void gfx_ps2_begin_frame(int r, int g, int b);
void gfx_ps2_end_frame(void);
void gfx_ps2_draw_background(void);
void gfx_ps2_draw_splash(void);
void gfx_ps2_begin_controller(void);
void gfx_ps2_set_alpha(int alpha);

/* Solid primitives, drawn in call order (no depth buffer). */
void draw_rect(int x, int y, int w, int h, int r, int g, int b);
void draw_frame(int x, int y, int w, int h, int r, int g, int b);
void fill_disc(int cx, int cy, int radius, int r, int g, int b);
void draw_ring(int cx, int cy, int radius, int r, int g, int b, int dots);
void draw_line(int x1, int y1, int x2, int y2, int r, int g, int b);
void fill_capsule(int x1, int y1, int x2, int y2, int radius, int r, int g, int b);
void fill_round_rect(int x, int y, int w, int h, int radius, int r, int g, int b);
void fill_triangle(int x1, int y1, int x2, int y2, int x3, int y3, int r, int g, int b);

/* Text, drawn with the built-in bitmap font.  Coordinates are the top-left
 * corner of the first glyph (text_out), the desired centre (text_center) or
 * the right edge (text_right). */
void text_out(int x, int y, const char *text);
void text_out_c(int x, int y, const char *text, int r, int g, int b);
void text_center(int cx, int y, const char *text);
void text_center_c(int cx, int y, const char *text, int r, int g, int b);
void text_right(int rx, int y, const char *text);
void text_right_c(int rx, int y, const char *text, int r, int g, int b);

/* Scaled text (scale 2 gives 10x20px glyphs) used by the splash screen. */
void text_big(int x, int y, const char *text, int scale, int r, int g, int b);

/* Width in pixels of a string at scale 1. */
int text_width(const char *text);

#endif
