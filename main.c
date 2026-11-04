/*
 * TEST DIAG - DualShock 2 controller tester for the PlayStation 2
 * Built with ps2sdk + gsKit / libpad.
 *
 * The screen shows a stylised top view of a DualShock 2 (body, grips,
 * shoulder buttons, d-pad, action symbols and analog sticks) on a 320x224
 * grid (640x448 at 2x).  Every element is driven by the same PadState (the
 * "control" pad: port 1 if connected, otherwise port 2), so the lit button
 * and its symbol can never disagree.
 *
 * The analog sticks move their caps with the axes, L3/R3 light their rings,
 * and the pressure-sensitive buttons change the green intensity with the
 * pressure value.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "gfx_ps2.h"
#include "pad_ps2.h"
#include "audio.h"
#include "modules.h"

#define SPLASH_FRAMES 90

/* Palette */
#define C_BG          3,   3,   8
#define C_DIM         118, 124, 146
#define C_TEXT        224, 230, 240
#define C_AMBER       255, 170, 0
#define C_GREEN       0,   220, 110
#define C_RED         220, 80,  80

#define C_BODY_EDGE   56,  60,  82
#define C_BODY        32,  34,  48
#define C_FACEPLATE   40,  42,  58
#define C_BTN         22,  22,  34
#define C_BTN_EDGE    82,  86, 112

/* Active-low button test: pressed when the bit is cleared. */
#define PRESSED(btn, mask) (!((btn) & (mask)))

/* ------------------------------------------------------------------------- */
/* Button table                                                              */
/* ------------------------------------------------------------------------- */

typedef struct {
    uint16_t    mask;
    const char *list;   /* short name used by the BTN: line / pressure line */
} ButtonInfo;

static const ButtonInfo BUTTONS[] = {
    { PAD_L1,       "L1"  },
    { PAD_L2,       "L2"  },
    { PAD_R1,       "R1"  },
    { PAD_R2,       "R2"  },
    { PAD_UP,       "UP"  },
    { PAD_DOWN,     "DN"  },
    { PAD_LEFT,     "LT"  },
    { PAD_RIGHT,    "RT"  },
    { PAD_TRIANGLE, "TRI" },
    { PAD_SQUARE,   "SQR" },
    { PAD_CIRCLE,   "CIR" },
    { PAD_CROSS,    "X"   },
    { PAD_SELECT,   "SEL" },
    { PAD_START,    "STR" },
    { PAD_L3,       "L3"  },
    { PAD_R3,       "R3"  },
};

#define NUM_BUTTONS ((int)(sizeof(BUTTONS) / sizeof(BUTTONS[0])))

/* Pressure for a button bitmask, or -1 when the pad does not report it. */
static int pad_pressure(const PadState *pad, uint16_t mask)
{
    if (!pad || !pad->press_mode)
        return -1;

    switch (mask) {
    case PAD_L1:       return pad->l1_p;
    case PAD_L2:       return pad->l2_p;
    case PAD_R1:       return pad->r1_p;
    case PAD_R2:       return pad->r2_p;
    case PAD_UP:       return pad->up_p;
    case PAD_DOWN:     return pad->down_p;
    case PAD_LEFT:     return pad->left_p;
    case PAD_RIGHT:    return pad->right_p;
    case PAD_TRIANGLE: return pad->triangle_p;
    case PAD_CIRCLE:   return pad->circle_p;
    case PAD_CROSS:    return pad->cross_p;
    case PAD_SQUARE:   return pad->square_p;
    default:           return -1;
    }
}

static const char *pad_type_short(uint8_t type, int connected)
{
    if (!connected)
        return "NON";

    switch (type) {
    case PAD_ID_DIGITAL:      return "DIG";
    case PAD_ID_ANALOG_STICK: return "STK";
    case PAD_ID_ANALOG:       return "ANA";
    default:                  return "?";
    }
}

/* Small integer square root, used to keep the stick cursor inside its well. */
static int int_sqrt(int v)
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

/* Overlay opacity: stronger while the button is pressed harder. */
static int lit_alpha(int press)
{
    return press > 0 ? (0x20 + press * 0x10 / 255) : 0x30;
}

/* The d-pad is a large area: keep its tint lighter so the arrows stay clear. */
static int dpad_alpha(int press)
{
    /* Same translucent look as the shoulder buttons. */
    return press > 0 ? (0x28 + press * 0x14 / 255) : 0x38;
}

/* ------------------------------------------------------------------------- */
/* Controller drawing                                                        */
/* ------------------------------------------------------------------------- */

/* Overlays drawn on top of the real controller render.  Only the pressed
 * state is added: the controller itself comes from assets/controller.jpg. */

static void tint_round(int x, int y, int w, int h, int radius, int alpha)
{
    gfx_ps2_set_alpha(alpha);
    fill_round_rect(x, y, w, h, radius, 0, 220, 90);
    gfx_ps2_set_alpha(0x80);
}

static void tint_disc(int cx, int cy, int radius, int alpha)
{
    gfx_ps2_set_alpha(alpha);
    fill_disc(cx, cy, radius, 0, 220, 90);
    gfx_ps2_set_alpha(0x80);
    draw_ring(cx, cy, radius, 0, 255, 150, 0);
}

static void tint_dpad(int dir, int alpha)
{
    int x, y, w, h;

    /* Raised button faces of the d-pad (rounded rectangles), traced from
     * the artwork.  They stop short of the centre diamond. */
    switch (dir) {
    case 0:  x = 78;  y = 65;  w = 20; h = 17; break; /* up */
    case 1:  x = 78;  y = 105; w = 20; h = 17; break; /* down */
    case 2:  x = 65;  y = 87;  w = 16; h = 17; break; /* left */
    default: x = 100; y = 87;  w = 16; h = 17; break; /* right */
    }

    /* Same translucent fill as the other buttons, no opaque border. */
    gfx_ps2_set_alpha(alpha);
    fill_round_rect(x, y, w, h, 4, 0, 220, 90);
    gfx_ps2_set_alpha(0x80);
}

static void draw_stick_overlay(int cx, int cy, const PadState *pad,
                               int right_stick, int clicked)
{
    int is_analog;
    uint8_t ax = 128, ay = 128;
    int dx = 0, dy = 0;

    is_analog = pad && pad->connected &&
                (pad->type == PAD_ID_ANALOG || pad->type == PAD_ID_ANALOG_STICK);

    if (is_analog) {
        ax = right_stick ? pad->rs_x : pad->ls_x;
        ay = right_stick ? pad->rs_y : pad->ls_y;

        dx = ((int)ax - 128) * 20 / 128;
        dy = ((int)ay - 128) * 20 / 128;

        /* Clamp the diagonal to the well perimeter. */
        {
            int d2 = dx * dx + dy * dy;

            if (d2 > 20 * 20) {
                int d = int_sqrt(d2);

                dx = dx * 20 / d;
                dy = dy * 20 / d;
            }
        }
    }

    /* L3/R3 lights the cap itself, respecting its round shape. */
    if (clicked) {
        gfx_ps2_set_alpha(0x38);
        fill_disc(cx, cy, 22, 0, 220, 90);
        gfx_ps2_set_alpha(0x80);
        draw_ring(cx, cy, 22, 0, 255, 150, 0);
    }

    if (is_analog) {
        draw_line(cx, cy, cx + dx, cy + dy, 90, 70, 20);

        fill_disc(cx + dx, cy + dy, 5, 8, 8, 8);
        fill_disc(cx + dx, cy + dy, 4, 255, 150, 0);
        fill_disc(cx + dx, cy + dy, 1, 255, 255, 235);
    } else {
        fill_disc(cx, cy, 2, 70, 72, 96);
    }
}

/* Horizontal level bar used for the vibration motors. */
static void draw_level_bar(int x, int y, int w, int h, int value, int active)
{
    draw_rect(x, y, w, h, 14, 14, 26);
    draw_frame(x, y, w, h, 84, 84, 120);

    if (!active)
        return;

    if (value > 0) {
        int fill = (w - 2) * value / 255;

        if (fill > 0)
            draw_rect(x + 1, y + 1, fill, h - 2, 255, 128, 0);
    }
}

/* Main screen                                                               */
/* ------------------------------------------------------------------------- */

static void build_btn_list(uint16_t buttons, char *buf)
{
    int len = 4;
    int i;

    strcpy(buf, "BTN:");

    for (i = 0; i < NUM_BUTTONS; i++) {
        int n = (int)strlen(BUTTONS[i].list);

        if (buttons & BUTTONS[i].mask)
            continue;

        if (len + n + 1 > 20) {
            strcat(buf, " ..");
            return;
        }

        buf[len++] = ' ';
        strcpy(buf + len, BUTTONS[i].list);
        len += n;
    }
}

static void draw_axes(const PadState *ctl, uint16_t b)
{
    char buf[24];
    int is_analog = ctl && ctl->connected &&
                    (ctl->type == PAD_ID_ANALOG ||
                     ctl->type == PAD_ID_ANALOG_STICK);
    uint8_t lx = 128, ly = 128, rx = 128, ry = 128;

    if (is_analog) {
        lx = ctl->ls_x; ly = ctl->ls_y;
        rx = ctl->rs_x; ry = ctl->rs_y;
    }

    if (is_analog) {
        sprintf(buf, "X%3d Y%3d", lx, ly);
    } else {
        strcpy(buf, "X--- Y---");
    }
    {
        int on = PRESSED(b, PAD_L3);

        text_out_c(76, 196, "L3", on ? 0 : 118, on ? 220 : 124, on ? 110 : 146);
    }
    text_out_c(94, 196, buf, is_analog ? 210 : 118, is_analog ? 216 : 124,
               is_analog ? 230 : 146);

    if (is_analog) {
        sprintf(buf, "X%3d Y%3d", rx, ry);
    } else {
        strcpy(buf, "X--- Y---");
    }
    {
        int on = PRESSED(b, PAD_R3);

        text_out_c(186, 196, "R3", on ? 0 : 118, on ? 220 : 124, on ? 110 : 146);
    }
    text_out_c(204, 196, buf, is_analog ? 210 : 118, is_analog ? 216 : 124,
               is_analog ? 230 : 146);
}

static void draw_ui(const PadState pads[2], const PadState *ctl, int port,
                    uint8_t motor_l, uint8_t motor_r, uint32_t frame)
{
    char     buf[80];
    uint16_t b = (ctl && ctl->connected) ? ctl->buttons : 0xffff;

    /* Real controller render first, then the HUD and the dynamic overlays. */
    gfx_ps2_draw_background();

    /* Header: HUD bar between the separate L/R button clusters. */
    draw_rect(48, 0, 224, 28, 10, 10, 18);
    draw_rect(48, 28, 224, 1, 0, 120, 65);

    sprintf(buf, "P1:%s", pad_type_short(pads[0].type, pads[0].connected));
    text_out_c(116, 4, buf, port == 0 ? 0 : 110, port == 0 ? 220 : 112,
               port == 0 ? 110 : 130);

    sprintf(buf, "P2:%s", pad_type_short(pads[1].type, pads[1].connected));
    text_out_c(158, 4, buf, port == 1 ? 0 : 110, port == 1 ? 220 : 112,
               port == 1 ? 110 : 130);

    sprintf(buf, "%s F%05d", gfx_ps2_mode_name(), (int)(frame % 100000));
    text_right_c(268, 4, buf, C_DIM);

    /* Separate shoulder buttons: tint the exact rounded silhouettes. */
    if (PRESSED(b, PAD_L1))
        tint_round(21, 16, 24, 12, 4, lit_alpha(pad_pressure(ctl, PAD_L1)));
    if (PRESSED(b, PAD_L2))
        tint_round(21, 40, 24, 22, 5, lit_alpha(pad_pressure(ctl, PAD_L2)));
    if (PRESSED(b, PAD_R1))
        tint_round(275, 16, 24, 12, 4, lit_alpha(pad_pressure(ctl, PAD_R1)));
    if (PRESSED(b, PAD_R2))
        tint_round(275, 40, 24, 22, 5, lit_alpha(pad_pressure(ctl, PAD_R2)));

    /* D-pad: one rounded arm per direction. */
    if (PRESSED(b, PAD_UP))
        tint_dpad(0, dpad_alpha(pad_pressure(ctl, PAD_UP)));
    if (PRESSED(b, PAD_DOWN))
        tint_dpad(1, dpad_alpha(pad_pressure(ctl, PAD_DOWN)));
    if (PRESSED(b, PAD_LEFT))
        tint_dpad(2, dpad_alpha(pad_pressure(ctl, PAD_LEFT)));
    if (PRESSED(b, PAD_RIGHT))
        tint_dpad(3, dpad_alpha(pad_pressure(ctl, PAD_RIGHT)));

    /* Action buttons: green discs on the measured button circles. */
    if (PRESSED(b, PAD_TRIANGLE))
        tint_disc(232, 77, 13, lit_alpha(pad_pressure(ctl, PAD_TRIANGLE)));
    if (PRESSED(b, PAD_SQUARE))
        tint_disc(212, 92, 13, lit_alpha(pad_pressure(ctl, PAD_SQUARE)));
    if (PRESSED(b, PAD_CIRCLE))
        tint_disc(254, 95, 13, lit_alpha(pad_pressure(ctl, PAD_CIRCLE)));
    if (PRESSED(b, PAD_CROSS))
        tint_disc(232, 111, 13, lit_alpha(pad_pressure(ctl, PAD_CROSS)));

    /* SELECT is a rounded rectangle, START is a triangle. */
    if (PRESSED(b, PAD_SELECT))
        tint_round(134, 88, 12, 7, 2, 0x50);
    if (PRESSED(b, PAD_START)) {
        gfx_ps2_set_alpha(0x50);
        fill_triangle(177, 90, 177, 97, 189, 94, 0, 220, 90);
        gfx_ps2_set_alpha(0x80);
        draw_line(177, 90, 177, 97, 0, 255, 150);
        draw_line(177, 90, 189, 94, 0, 255, 150);
        draw_line(177, 97, 189, 94, 0, 255, 150);
    }

    /* Analog sticks: cursor centred on the measured caps. */
    draw_stick_overlay(125, 129, ctl, 0, PRESSED(b, PAD_L3));
    draw_stick_overlay(196, 130, ctl, 1, PRESSED(b, PAD_R3));

    /* Status line: BTN and PRS grouped and centred in the HUD band. */
    {
        char btn[48];
        char prs[40];
        int  prs_len = 0;
        int  prs_r = 118, prs_g = 124, prs_b = 146;
        int  bw, pw, total, x;

        build_btn_list(b, btn);
        if (btn[4] == '\0')
            strcat(btn, " ---");

        if (!ctl || !ctl->connected) {
            char diag[32];

            prs[0] = '\0';
            pad_ps2_diag(diag, sizeof(diag));
            snprintf(btn, sizeof(btn), "NO PAD %s", diag);
            prs_r = 220; prs_g = 90; prs_b = 90;
        } else {
            const char *strong = NULL;
            int strong_val = 0;
            int i;

            for (i = 0; i < NUM_BUTTONS; i++) {
                int p;

                if (b & BUTTONS[i].mask)
                    continue;
                p = pad_pressure(ctl, BUTTONS[i].mask);
                if (p > strong_val) {
                    strong_val = p;
                    strong = BUTTONS[i].list;
                }
            }

            if (strong && strong_val > 0) {
                sprintf(prs, "PRS:%s %3d", strong, strong_val);
                prs_r = 255; prs_g = 170; prs_b = 0;
            } else if (ctl->press_mode) {
                strcpy(prs, "PRS:---");
            } else {
                strcpy(prs, "NO PRESSURE");
            }
            prs_len = (int)strlen(prs);
        }

        bw = text_width(btn);
        pw = text_width(prs);
        total = bw + (prs_len ? 8 + pw : 0);
        x = 160 - total / 2;
        if (x < 52)
            x = 52;

        text_out_c(x, 16, btn,
                   (!ctl || !ctl->connected) ? prs_r : 224,
                   (!ctl || !ctl->connected) ? prs_g : 230,
                   (!ctl || !ctl->connected) ? prs_b : 240);

        if (prs_len)
            text_out_c(x + bw + 8, 16, prs, prs_r, prs_g, prs_b);
    }

    draw_axes(ctl, b);

    /* Vibration motor bars ----------------------------------------------- */
    if (!ctl || !ctl->connected) {
        /* No pad: use the bottom line to show where the IOP modules came
         * from, which is what decides whether the pad can work at all. */
        pad_ps2_diag_modules(buf, sizeof(buf));
        text_out_c(6, 210, buf, C_DIM);
        return;
    }

    sprintf(buf, "B7 A%d R%d", pad_ps2_actuators(port), pad_ps2_act_result(port));
    text_out_c(6, 198, buf, C_DIM);

    sprintf(buf, "%3d", motor_l);
    text_out_c( 10, 208, "BIG", C_DIM);
    draw_level_bar(34, 208, 100, 9, motor_l, ctl && ctl->connected);
    text_out_c(139, 208, buf, motor_l ? 255 : 120, motor_l ? 170 : 126,
               motor_l ? 0 : 148);

    sprintf(buf, "%3d", motor_r);
    text_out_c(170, 208, "SML", C_DIM);
    draw_level_bar(194, 208, 96, 9, motor_r, ctl && ctl->connected);
    text_out_c(296, 208, buf, motor_r ? 255 : 120, motor_r ? 170 : 126,
               motor_r ? 0 : 148);
}

/* ------------------------------------------------------------------------- */
/* Boot screen                                                               */
/* ------------------------------------------------------------------------- */

static void draw_splash(uint32_t frame)
{
    /* The artwork has an empty box at the bottom (physical x=118..508,
     * y=352..374); fill it progressively. */
    int w = (int)((frame * 195) / (SPLASH_FRAMES - 1));

    gfx_ps2_draw_splash();

    if (w > 0) {
        draw_rect(59, 176, w, 11, 0, 225, 150);

        if (w < 195)
            draw_rect(59 + w - 1, 176, 1, 11, 190, 255, 225);
    }
}

void show_stage(int n);

void show_stage(int n)
{
    char b[8];

    sprintf(b, "%d", n);
    gfx_ps2_begin_frame(C_BG);
    text_big(150, 100, b, 3, 255, 255, 255);
    gfx_ps2_end_frame();
}

/* ------------------------------------------------------------------------- */
/* Main                                                                      */
/* ------------------------------------------------------------------------- */

int main(int argc, const char **argv)
{
    PadState p[2];
    uint32_t frame = 0;
    uint32_t f;
    int force_mode = 0;
    int i;

    /* Optional "-ntsc" / "-pal" arguments force the video mode (the ISO
     * boots without arguments and follows the console region instead). */
    for (i = 1; i < argc; i++) {
        if (!argv[i])
            continue;
        if (strcmp(argv[i], "-ntsc") == 0 || strcmp(argv[i], "ntsc") == 0)
            force_mode = 1;
        else if (strcmp(argv[i], "-pal") == 0 || strcmp(argv[i], "pal") == 0)
            force_mode = 2;
        else if (strcmp(argv[i], "-hardreset") == 0)
            pad_force_reset = 1;
    }

    /* When the loader gives us the ELF path, look for the IOP modules in the
     * same directory first (launched from USB / memory card). */
    if (argc > 0 && argv[0])
        mod_set_base_dir(argv[0]);

    gfx_ps2_init(force_mode);

    /* Plain, texture-free frame before touching the IOP: whatever happens
     * next, the screen shows something instead of staying black. */
    gfx_ps2_begin_frame(C_BG);
    text_center_c(160, 100, "STARTING...", C_TEXT);
    gfx_ps2_end_frame();

    show_stage(1);
    pad_ps2_init();
    show_stage(2);
    audio_init();
    show_stage(3);

    for (f = 0; f < SPLASH_FRAMES; f++) {
        gfx_ps2_begin_frame(C_BG);
        draw_splash(f);
        gfx_ps2_end_frame();
    }

    /* Splash done: release its texture and upload the tester background. */
    gfx_ps2_begin_controller();

    while (1) {
        const PadState *ctl;
        int port;

        pad_ps2_poll();
        p[0] = pad_ps2_get(0);
        p[1] = pad_ps2_get(1);

        if (p[0].connected) {
            ctl  = &p[0];
            port = 0;
        } else if (p[1].connected) {
            ctl  = &p[1];
            port = 1;
        } else {
            ctl  = NULL;
            port = 0;
        }

        {
            static uint16_t prev_buttons = 0xffff;
            static uint32_t last_click;
            uint16_t cb = ctl ? ctl->buttons : 0xffff;

            /* One click per press, with a ~100 ms guard against repeated
             * pad-state edges or bouncing. */
            if ((prev_buttons & ~cb) && (frame - last_click > 5)) {
                audio_click();
                last_click = frame;
            }
            prev_buttons = cb;
        }

        if (ctl) {
            uint8_t motor_small = 0;
            uint8_t motor_big = 0;

            /* Deterministic test: L2 = big motor at half power, R2 = small
             * motor on.  Fixed values make it obvious whether the command
             * reaches the pad. */
            /* Full power: padman's overcurrent guard blocks changing the
             * value while the motor is already running, so a progressive
             * ramp would be zeroed after the first step. */
            if (PRESSED(ctl->buttons, PAD_L2))
                motor_big = 0xFF;

            if (PRESSED(ctl->buttons, PAD_R2))
                motor_small = 0x01;

            pad_ps2_set_motors(motor_small, motor_big);

            gfx_ps2_begin_frame(C_BG);
            draw_ui(p, ctl, port, motor_big, motor_small, frame);
            gfx_ps2_end_frame();
        } else {
            pad_ps2_set_motors(0, 0);
            gfx_ps2_begin_frame(C_BG);
            draw_ui(p, NULL, 0, 0, 0, frame);
            gfx_ps2_end_frame();
        }

        frame++;
    }

    return 0;
}
