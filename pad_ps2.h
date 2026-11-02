#ifndef PAD_PS2_H
#define PAD_PS2_H

#include <stdint.h>
#include <libpad.h>

/* PS1 controller type IDs kept so the diagnostic UI code is unchanged. */
#define PAD_ID_NONE 0x0f
#define PAD_ID_DIGITAL 0x04
#define PAD_ID_ANALOG_STICK 0x05
#define PAD_ID_ANALOG 0x07

typedef struct {
    uint8_t connected;
    uint8_t type;
    uint16_t buttons;
    uint8_t ls_x, ls_y;
    uint8_t rs_x, rs_y;

    /* DualShock 2 analog button pressure (0 = released, 255 = full press).
     * Valid only when press_mode is 1 (pad reports pressure data). */
    uint8_t left_p, right_p, up_p, down_p;
    uint8_t triangle_p, circle_p, cross_p, square_p;
    uint8_t l1_p, r1_p, l2_p, r2_p;
    uint8_t press_mode;
} PadState;

void pad_ps2_init(void);
void pad_ps2_poll(void);
void pad_ps2_set_motors(uint8_t small, uint8_t big);
PadState pad_ps2_get(int port);

/* Short status string used by the UI when no pad is detected:
 * S/P = SIO2MAN/PADMAN loaded, I = padInit, O = padPortOpen per port,
 * followed by padGetState() for ports 1 and 2. */
void pad_ps2_diag(char *buf, int size);

#endif
