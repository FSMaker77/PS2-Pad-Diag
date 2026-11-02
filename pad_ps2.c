#include <stdio.h>
#include <string.h>
#include <sifrpc.h>
#include <loadfile.h>
#include "pad_ps2.h"
#include "modules.h"

static char pad_area[2][256] __attribute__((aligned(64)));
static uint8_t act_align[2][6] __attribute__((aligned(64)));
static uint8_t act_align_sent[2];
static uint8_t press_enabled[2];
static PadState pad_state[2];
static uint8_t motor_small[2];
static uint8_t motor_big[2];
static uint8_t port_open[2];

/* Diagnostics shown on screen when no pad is detected. */
static int dbg_sio = -1;
static int dbg_pad = -1;
static int dbg_init = -1;
static int dbg_open[2] = { -1, -1 };

static void pad_open_port(int port);

static int pad_mode_supported(int port, int mode)
{
    int count = padInfoMode(port, 0, PAD_MODETABLE, -1);
    int i;

    if (count <= 0) {
        return 0;
    }

    for (i = 0; i < count; i++) {
        if (padInfoMode(port, 0, PAD_MODETABLE, i) == mode) {
            return 1;
        }
    }

    return 0;
}

static void pressure_reset(int port)
{
    PadState *st = &pad_state[port];

    st->left_p = st->right_p = st->up_p = st->down_p = 0;
    st->triangle_p = st->circle_p = st->cross_p = st->square_p = 0;
    st->l1_p = st->r1_p = st->l2_p = st->r2_p = 0;
    st->press_mode = 0;
}

static void pad_update_type(int port)
{
    switch (padInfoMode(port, 0, PAD_MODECURID, 0)) {
    case PAD_TYPE_DIGITAL:
        pad_state[port].type = PAD_ID_DIGITAL;
        break;
    case PAD_TYPE_ANALOG:
        pad_state[port].type = PAD_ID_ANALOG_STICK;
        break;
    case PAD_TYPE_DUALSHOCK:
        pad_state[port].type = PAD_ID_ANALOG;
        break;
    default:
        pad_state[port].type = PAD_ID_NONE;
        break;
    }
}

static void pad_send_motors(int port)
{
    uint8_t prev_small;
    uint8_t prev_big;

    if (!pad_state[port].connected) {
        return;
    }

    if (padInfoAct(port, 0, -1, 0) == 0) {
        return;
    }

    /* Do not queue a new actuator command while the previous one is still
     * being executed by the pad. */
    if (padGetState(port, 0) != PAD_STATE_STABLE) {
        return;
    }

    if (act_align[port][0] == motor_small[port] &&
        act_align[port][1] == motor_big[port]) {
        return;
    }

    prev_small = act_align[port][0];
    prev_big = act_align[port][1];

    act_align[port][0] = motor_small[port];
    act_align[port][1] = motor_big[port];

    if (!padSetActDirect(port, 0, (const char *)act_align[port])) {
        act_align[port][0] = prev_small;
        act_align[port][1] = prev_big;
    }
}

/* Best-effort loading of the IOP modules needed by libpad. They may already
 * be loaded by the BIOS/host-loader (in which case these calls just fail and
 * are ignored).  mod_load() looks on the disc image, next to the ELF on the
 * USB stick / memory card and on the ps2link host device; the modules built
 * into the ROM are the last resort. */
static void pad_load_modules(void)
{
    dbg_sio = mod_load("SIO2MAN.IRX");
    if (dbg_sio < 0)
        dbg_sio = SifLoadModule("rom0:XSIO2MAN", 0, NULL);

    dbg_pad = mod_load("PADMAN.IRX");
    if (dbg_pad < 0)
        dbg_pad = SifLoadModule("rom0:XPADMAN", 0, NULL);
}

void pad_ps2_init(void)
{
    int port;

    /* libpad talks to the padman server over SIF RPC; without this the
     * padInit/padPortOpen calls block forever (the program looked stuck on
     * the splash screen). */
    sceSifInitRpc(0);
    pad_load_modules();

    memset(pad_state, 0, sizeof(pad_state));
    memset(act_align, 0, sizeof(act_align));
    memset(motor_small, 0, sizeof(motor_small));
    memset(motor_big, 0, sizeof(motor_big));
    memset(port_open, 0, sizeof(port_open));

    for (port = 0; port < 2; port++) {
        pad_state[port].type = PAD_ID_NONE;
        pad_state[port].buttons = 0xffff;
        pad_state[port].ls_x = 128;
        pad_state[port].ls_y = 128;
        pad_state[port].rs_x = 128;
        pad_state[port].rs_y = 128;

        /* Pressure data is filled only when the pad reports it. */
        pressure_reset(port);
        press_enabled[port] = 0;

        /* Actuator alignment buffer: [0]=small engine on/off, [1]=big engine
         * speed (0-255), bytes [2..5]=0xff disable the unused actuators. */
        act_align[port][2] = 0xff;
        act_align[port][3] = 0xff;
        act_align[port][4] = 0xff;
        act_align[port][5] = 0xff;
        act_align_sent[port] = 0;
    }

    dbg_init = padInit(0);
    for (port = 0; port < 2; port++)
        pad_open_port(port);
}

/* padPortOpen() fails while padman is still starting up (or if SIO2MAN was
 * not loaded yet).  Keep trying instead of giving up after one attempt, which
 * is what made the tool report "NO PADS" on a real console. */
static void pad_open_port(int port)
{
    if (port_open[port])
        return;

    dbg_open[port] = padPortOpen(port, 0, pad_area[port]);
    if (dbg_open[port])
        port_open[port] = 1;
}

void pad_ps2_diag(char *buf, int size)
{
    snprintf(buf, size, "S:%c P:%c I:%c O:%c/%c %x/%x",
             dbg_sio >= 0 ? 'o' : '-',
             dbg_pad >= 0 ? 'o' : '-',
             dbg_init > 0 ? 'o' : '-',
             dbg_open[0] > 0 ? 'o' : '-',
             dbg_open[1] > 0 ? 'o' : '-',
             padGetState(0, 0), padGetState(1, 0));
}

void pad_ps2_poll(void)
{
    static unsigned retry;
    int port;

    retry++;

    for (port = 0; port < 2; port++) {
        struct padButtonStatus buttons;
        unsigned char state;

        /* Keep retrying the port open: on a real console it can fail while
         * padman/SIO2MAN are still coming up. */
        if (!port_open[port] && (retry & 63) == 0)
            pad_open_port(port);

        state = padGetState(port, 0);

        if ((state == PAD_STATE_STABLE || state == PAD_STATE_FINDCTP1) &&
            pad_mode_supported(port, PAD_TYPE_DUALSHOCK) &&
            padInfoMode(port, 0, PAD_MODECURID, 0) != PAD_TYPE_DUALSHOCK) {
            padSetMainMode(port, 0, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);
        }

        if (state == PAD_STATE_STABLE && padRead(port, 0, &buttons) != 0) {
            pad_state[port].connected = 1;
            pad_state[port].buttons = buttons.btns;
            pad_state[port].ls_x = buttons.ljoy_h;
            pad_state[port].ls_y = buttons.ljoy_v;
            pad_state[port].rs_x = buttons.rjoy_h;
            pad_state[port].rs_y = buttons.rjoy_v;
            pad_update_type(port);

            /* Configure the actuators once per connection so that
             * padSetActDirect() can actually drive the vibration motors. */
            if (!act_align_sent[port] &&
                padInfoAct(port, 0, -1, 0) > 0) {
                if (padSetActAlign(port, 0, (const char *)act_align[port])) {
                    act_align_sent[port] = 1;
                }
            }

            /* DualShock 2 analog button pressure (only on analog pads that
             * report it). Enter pressure mode once per connection. */
            if (!press_enabled[port] &&
                (pad_state[port].type == PAD_ID_ANALOG ||
                 pad_state[port].type == PAD_ID_ANALOG_STICK) &&
                padInfoPressMode(port, 0)) {
                if (padEnterPressMode(port, 0)) {
                    press_enabled[port] = 1;
                }
            }

            if (press_enabled[port]) {
                PadState *sp = &pad_state[port];

                sp->left_p  = buttons.left_p;
                sp->right_p = buttons.right_p;
                sp->up_p    = buttons.up_p;
                sp->down_p  = buttons.down_p;
                sp->triangle_p = buttons.triangle_p;
                sp->circle_p   = buttons.circle_p;
                sp->cross_p    = buttons.cross_p;
                sp->square_p   = buttons.square_p;
                sp->l1_p    = buttons.l1_p;
                sp->r1_p    = buttons.r1_p;
                sp->l2_p    = buttons.l2_p;
                sp->r2_p    = buttons.r2_p;
                sp->press_mode = 1;
            }
        } else if (state == PAD_STATE_DISCONN || state == PAD_STATE_ERROR) {
            pad_state[port].connected = 0;
            pad_state[port].buttons = 0xffff;
            pad_state[port].type = PAD_ID_NONE;
            pad_state[port].ls_x = 128;
            pad_state[port].ls_y = 128;
            pad_state[port].rs_x = 128;
            pad_state[port].rs_y = 128;
            motor_small[port] = 0;
            motor_big[port] = 0;
            act_align[port][0] = 0;
            act_align[port][1] = 0;
            act_align_sent[port] = 0;
            press_enabled[port] = 0;
            pressure_reset(port);
        }

        pad_send_motors(port);
    }
}

void pad_ps2_set_motors(uint8_t small, uint8_t big)
{
    motor_small[0] = small;
    motor_small[1] = small;
    motor_big[0] = big;
    motor_big[1] = big;
}

PadState pad_ps2_get(int port)
{
    PadState empty = { 0, PAD_ID_NONE, 0xffff, 128, 128, 128, 128 };

    if (port < 0 || port > 1) {
        return empty;
    }

    return pad_state[port];
}
