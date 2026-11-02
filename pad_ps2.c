#include <stdio.h>
#include <string.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <iopheap.h>
#include "pad_ps2.h"
#include "modules.h"

/* The two modules are also embedded in the ELF: when launched from OPL the
 * app only sees iomanX/fileXio, so the ROM loader cannot open the .IRX
 * files on mass: ("Unknown device").  SifExecModuleBuffer loads them from
 * EE RAM instead. */
extern unsigned char sio2man_irx[];
extern unsigned int size_sio2man_irx;
extern unsigned char padman_irx[];
extern unsigned int size_padman_irx;

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
static int dbg_sio_res;
static int dbg_pad_res;
static int dbg_init = -1;
static int dbg_open[2] = { -1, -1 };
static char dbg_sio_path[56] = "";
static char dbg_pad_path[56] = "";
static int dbg_reloaded;
static int dbg_from_rom;

static void pad_open_port(int port);

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
    switch (padInfoMode(port, 0, PAD_MODECURID, 0)) {    case PAD_TYPE_DIGITAL:
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

/* padInfoAct() is an SIO2 round trip: ask once per connection and keep the
 * answer.  Calling it (and padInfoMode) every frame disturbs padman's own
 * polling and puts the port in PAD_STATE_ERROR on a real console. */
static uint8_t act_supported[2];
static uint8_t act_checked[2];
static uint16_t act_cooldown[2];
static uint8_t mode_set_done[2];
static uint8_t type_checks[2];
static uint8_t last_state[2];
static uint16_t stuck_frames[2];
static uint8_t reopen_tries[2];

#define ACT_MIN_FRAMES 15   /* ~250 ms at 60 Hz */

static void pad_send_motors(int port, unsigned char state)
{
    uint8_t prev_small;
    uint8_t prev_big;

    if (!pad_state[port].connected || state != PAD_STATE_STABLE) {
        return;
    }

    if (!act_checked[port]) {
        act_supported[port] = padInfoAct(port, 0, -1, 0) > 0;
        act_checked[port] = 1;
    }

    if (!act_supported[port]) {
        return;
    }

    if (act_align[port][0] == motor_small[port] &&
        act_align[port][1] == motor_big[port]) {
        return;
    }

    /* Give the pad time to execute the previous actuator command: poking it
     * every frame is what made the controller drop out. */
    if (act_cooldown[port] > 0) {
        act_cooldown[port]--;
        return;
    }

    prev_small = act_align[port][0];
    prev_big = act_align[port][1];

    act_align[port][0] = motor_small[port];
    act_align[port][1] = motor_big[port];

    if (padSetActDirect(port, 0, (const char *)act_align[port])) {
        act_cooldown[port] = ACT_MIN_FRAMES;
    } else {
        act_align[port][0] = prev_small;
        act_align[port][1] = prev_big;
        act_cooldown[port] = ACT_MIN_FRAMES;
    }
}

/* Best-effort loading of the IOP modules needed by libpad. They may already
 * be loaded by the BIOS/host-loader (in which case these calls just fail and
 * are ignored).  mod_load() looks on the disc image, next to the ELF on the
 * USB stick / memory card and on the ps2link host device; the modules built
 * into the ROM are the last resort. */
static void pad_load_modules(void)
{
    /* A launcher (OPL, uLaunchELF) calls padEnd() before handing over: padman
     * stays resident but its vblank handler and worker threads are gone, so
     * padPortOpen still answers while no SIO2 transfer ever completes.  The
     * only way back is to let the module run its init again, i.e. unload and
     * reload it.  Do that only when we can actually put the files back. */
    if (mod_can_open("PADMAN.IRX") && mod_can_open("SIO2MAN.IRX")) {
        mod_unload("padman");
        mod_unload("sio2man");
        dbg_reloaded = 1;
    }

    dbg_sio = SifExecModuleBuffer(sio2man_irx, size_sio2man_irx, 0, NULL, NULL);
    if (dbg_sio >= 0)
        snprintf(dbg_sio_path, sizeof(dbg_sio_path), "(built-in)");
    if (dbg_sio < 0)
        dbg_sio = mod_load("SIO2MAN.IRX");
    dbg_sio_res = 0;
    if (dbg_sio < 0) {
        dbg_sio = SifLoadModule("rom0:XSIO2MAN", 0, NULL);
        dbg_from_rom = 1;
        snprintf(dbg_sio_path, sizeof(dbg_sio_path), "rom0:XSIO2MAN");
    } else {
        snprintf(dbg_sio_path, sizeof(dbg_sio_path), "%s", mod_last_path());
    }

    dbg_pad = SifExecModuleBuffer(padman_irx, size_padman_irx, 0, NULL, NULL);
    if (dbg_pad >= 0)
        snprintf(dbg_pad_path, sizeof(dbg_pad_path), "(built-in)");
    if (dbg_pad < 0)
        dbg_pad = mod_load("PADMAN.IRX");
    dbg_pad_res = 0;
    if (dbg_pad < 0) {
        dbg_pad = SifLoadModule("rom0:XPADMAN", 0, NULL);
        dbg_from_rom = 1;
        snprintf(dbg_pad_path, sizeof(dbg_pad_path), "rom0:XPADMAN");
    } else {
        snprintf(dbg_pad_path, sizeof(dbg_pad_path), "%s", mod_last_path());
    }
}

void pad_ps2_init(void)
{
    int port;

    /* libpad talks to the padman server over SIF RPC; without this the
     * padInit/padPortOpen calls block forever (the program looked stuck on
     * the splash screen). */
    sceSifInitRpc(0);
    SifInitIopHeap();
    pad_load_modules();

    memset(pad_state, 0, sizeof(pad_state));
    memset(act_align, 0, sizeof(act_align));
    memset(motor_small, 0, sizeof(motor_small));
    memset(motor_big, 0, sizeof(motor_big));
    memset(port_open, 0, sizeof(port_open));
    memset(act_checked, 0, sizeof(act_checked));
    memset(act_cooldown, 0, sizeof(act_cooldown));
    memset(mode_set_done, 0, sizeof(mode_set_done));
    memset(type_checks, 0, sizeof(type_checks));
    memset(last_state, 0, sizeof(last_state));
    memset(stuck_frames, 0, sizeof(stuck_frames));
    memset(reopen_tries, 0, sizeof(reopen_tries));

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
    /* o = loaded by us, r = already resident in the IOP, - = failed.
     * The last pair is the request state: o=complete, b=busy, !=failed. */
    static const char rst[] = { 'o', '!', 'b' };
    unsigned char r0 = padGetReqState(0, 0);
    unsigned char r1 = padGetReqState(1, 0);

    if (r0 > 2)
        r0 = 0;
    if (r1 > 2)
        r1 = 0;

    snprintf(buf, size, "%sS:%c P:%c I:%c O:%c/%c %x/%x %c/%c",
             dbg_reloaded ? "R " : "",
             dbg_sio < 0 ? '-' : (dbg_sio_res ? 'r' : 'o'),
             dbg_pad < 0 ? '-' : (dbg_pad_res ? 'r' : 'o'),
             dbg_init > 0 ? 'o' : '-',
             dbg_open[0] > 0 ? 'o' : '-',
             dbg_open[1] > 0 ? 'o' : '-',
             last_state[0], last_state[1],
             rst[r0], rst[r1]);
}

void pad_ps2_diag_modules(char *buf, int size)
{
    if (dbg_from_rom) {
        snprintf(buf, size, "COPY SIO2MAN.IRX+PADMAN.IRX IN APPS/");
        return;
    }

    snprintf(buf, size, "SIO %s  PAD %s", dbg_sio_path, dbg_pad_path);
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
        last_state[port] = state;

        /* PAD_STATE_EXECCMD (5) means "STABLE, but a request is still
         * pending".  libpad's padInfoMode()/padSetMainMode() set that request
         * state and padman only clears it between transfers, so reading only
         * while the state is exactly STABLE deadlocks as soon as we ask for
         * any information.  Try to read in both states: padRead() returns 0
         * when no data is available. */
        if ((state == PAD_STATE_STABLE || state == PAD_STATE_EXECCMD) &&
            padRead(port, 0, &buttons) != 0) {
            PadState *sp = &pad_state[port];

            reopen_tries[port] = 0;
            sp->connected = 1;
            sp->buttons = buttons.btns;
            sp->ls_x = buttons.ljoy_h;
            sp->ls_y = buttons.ljoy_v;
            sp->rs_x = buttons.rjoy_h;
            sp->rs_y = buttons.rjoy_v;

            /* Ask for the DualShock mode once per connection, then poll the
             * pad type for a short while so the switch shows up.  These are
             * SIO2 round trips that mark the request BUSY, but reading works
             * while a request is pending, so they cannot deadlock us. */
            if (!mode_set_done[port]) {
                mode_set_done[port] = 1;
                if (padInfoMode(port, 0, PAD_MODETABLE, -1) > 0) {
                    padSetMainMode(port, 0, PAD_MMODE_DUALSHOCK,
                                   PAD_MMODE_LOCK);
                }
            }

            if (type_checks[port] < 90) {
                type_checks[port]++;
                pad_update_type(port);
            }

            /* Configure the actuators once per connection so that
             * padSetActDirect() can actually drive the vibration motors. */
            if (!act_align_sent[port] && !act_checked[port]) {
                act_supported[port] = padInfoAct(port, 0, -1, 0) > 0;
                act_checked[port] = 1;

                if (act_supported[port] &&
                    padSetActAlign(port, 0, (const char *)act_align[port])) {
                    act_align_sent[port] = 1;
                }
            }

            /* DualShock 2 analog button pressure (only on analog pads that
             * report it). Enter pressure mode once per connection. */
            if (!press_enabled[port] &&
                (sp->type == PAD_ID_ANALOG ||
                 sp->type == PAD_ID_ANALOG_STICK) &&
                padInfoPressMode(port, 0)) {
                if (padEnterPressMode(port, 0)) {
                    press_enabled[port] = 1;
                }
            }

            if (press_enabled[port]) {
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
            act_checked[port] = 0;
            act_cooldown[port] = 0;
            mode_set_done[port] = 0;
            type_checks[port] = 0;
            press_enabled[port] = 0;
            pressure_reset(port);
        }

        pad_send_motors(port, state);
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
