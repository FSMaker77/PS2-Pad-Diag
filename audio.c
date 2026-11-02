/*
 * Short UI click through the PS2 SPU2 (audsrv).
 *
 * The IOP modules are loaded from the disc (self-contained ISO) or from the
 * host device when running the ELF with ps2link/PCSX2.
 */

#include <stdio.h>
#include <string.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <audsrv.h>

#include "audio.h"
#include "modules.h"

extern unsigned char click_adp[];
extern unsigned int size_click_adp;

static audsrv_adpcm_t click_sample;
static int   audio_ok;
static int   audio_stage;

void audio_init(void)
{
    int ret;
    int resident;

    sceSifInitRpc(0);

    /* OPL already has libsd/audsrv in the IOP: reuse them instead of loading
     * a second libsd, which would fight over the SPU2. */
    resident = mod_rpc_alive(AUDSRV_RPC_ID);

    if (!resident) {
        ret = mod_load("LIBSD.IRX");
        if (ret < 0)
            ret = SifLoadModule("rom0:LIBSD", 0, NULL);
        if (ret < 0) {
            audio_stage = 1;
            audio_ok = 0;
            return;
        }
    }
    audio_stage = 2;

    if (!resident) {
        ret = mod_load("AUDSRV.IRX");
        if (ret < 0) {
            audio_stage = 3;
            audio_ok = 0;
            return;
        }
    }
    audio_stage = 4;

    if (audsrv_init() != 0) {
        audio_stage = 5;
        audio_ok = 0;
        return;
    }

    if (audsrv_adpcm_init() != 0) {
        audsrv_quit();
        audio_stage = 6;
        audio_ok = 0;
        return;
    }

    audsrv_set_volume(MAX_VOLUME);

    if (audsrv_load_adpcm(&click_sample, click_adp, size_click_adp) != 0) {
        audsrv_quit();
        audio_stage = 7;
        audio_ok = 0;
        return;
    }

    audio_ok = 1;
    audio_stage = 8;
}

int audio_active(void)
{
    return audio_ok;
}

int audio_stage_get(void)
{
    return audio_stage;
}

void audio_click(void)
{
    int ch;

    if (!audio_ok)
        return;

    /* One-shot ADPCM voice: it never loops or overlaps the previous click. */
    ch = audsrv_play_adpcm(&click_sample);
    if (ch >= 0)
        audsrv_adpcm_set_volume_and_pan(ch, MAX_VOLUME, 0);
}
