/*
 * IOP module loader that does not depend on where the ELF was launched from.
 *
 * An ELF started as an OPL app has no cdrom0: with the modules on it, the USB
 * stick can be mass0:, mass1:, ... depending on the driver in use, and OPL /
 * FMCB may already have sio2man/padman/libsd/audsrv resident in the IOP.
 *
 * Loading a second copy of sio2man or padman is fatal: two drivers end up on
 * the SIO2 bus and the controller stops answering with PAD_STATE_ERROR.  So
 * the callers first check mod_rpc_alive() and only load when nothing is
 * running.  SifSearchModuleByName() would be the obvious API for that, but it
 * blocks forever on PCSX2, hence the non blocking bind probe below.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <loadfile.h>
#include <sifrpc.h>
#include <kernel.h>

#include "modules.h"

static char last_path[160];

const char *mod_last_path(void)
{
    return last_path;
}

/* Directory holding the ELF, as reported by the loader (may be empty). */
static char base_dir[80];

void mod_set_base_dir(const char *dir)
{
    const char *slash;
    size_t n;

    base_dir[0] = '\0';
    if (!dir || !*dir)
        return;

    /* Accept either a path to the ELF ("mass:/APPS/X.ELF") or a directory. */
    slash = strrchr(dir, '/');
    n = slash ? (size_t)(slash - dir) + 1 : 0;
    if (n == 0 || n >= sizeof(base_dir))
        return;

    memcpy(base_dir, dir, n);
    base_dir[n] = '\0';
}

int mod_rpc_alive(int sid)
{
    static SifRpcClientData_t cd __attribute__((aligned(64)));

    memset(&cd, 0, sizeof(cd));

    /* A blocking bind is safe here: the IOP always answers a bind request,
     * with sd == NULL when no server has that id, so this returns quickly
     * either way.  The SIF_RPC_M_NOWAIT variant used before was a trap: the
     * reply often arrived after the spin ended, giving a false "not there"
     * and making us load a second padman on top of the resident one. */
    if (sceSifBindRpc(&cd, sid, 0) < 0)
        return 0;

    return cd.server != NULL;
}

static void lower_copy(char *dst, const char *src, size_t size)
{
    size_t i;

    for (i = 0; src[i] && i + 1 < size; i++)
        dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = '\0';
}

/* Candidate paths, in the order they are tried. */
static const char *const candidate_fmt[] = {
    "%s%s",              /* directory of the ELF, when known */
    "cdrom0:\\%s;1",     /* self-contained ISO */
    "mass:/APPS/%s", "mass0:/APPS/%s", "mass1:/APPS/%s",
    "mass:/%s",      "mass0:/%s",      "mass1:/%s",
    "mc0:/APPS/%s",  "mc0:/%s",        "mc1:/%s",
};

#define CANDIDATE_COUNT (sizeof(candidate_fmt) / sizeof(candidate_fmt[0]))

static int build_candidate(char *path, size_t size, const char *name, size_t i)
{
    snprintf(path, size, candidate_fmt[i], base_dir, name);
    return 0;
}

int mod_load(const char *name)
{
    char path[160];
    char lower[48];
    int ret;
    size_t i;

    last_path[0] = '\0';
    lower_copy(lower, name, sizeof(lower));

    for (i = 0; i < CANDIDATE_COUNT; i++) {
        if (i == 0 && base_dir[0] == '\0')
            continue;

        build_candidate(path, sizeof(path), name, i);
        ret = SifLoadModule(path, 0, NULL);
        if (ret >= 0) {
            snprintf(last_path, sizeof(last_path), "%s", path);
            return ret;
        }
    }

    /* ps2link / PCSX2 host device uses lower case file names. */
    snprintf(path, sizeof(path), "host:%s", lower);
    ret = SifLoadModule(path, 0, NULL);
    if (ret >= 0) {
        snprintf(last_path, sizeof(last_path), "%s", path);
        return ret;
    }

    snprintf(path, sizeof(path), "host0:%s", lower);
    ret = SifLoadModule(path, 0, NULL);
    if (ret >= 0) {
        snprintf(last_path, sizeof(last_path), "%s", path);
        return ret;
    }

    return -1;
}

int mod_can_open(const char *name)
{
    char path[160];
    char lower[48];
    int fd = -1;
    size_t i;

    lower_copy(lower, name, sizeof(lower));

    for (i = 0; i < CANDIDATE_COUNT; i++) {
        if (i == 0 && base_dir[0] == '\0')
            continue;

        build_candidate(path, sizeof(path), name, i);
        fd = open(path, O_RDONLY);
        if (fd >= 0) {
            close(fd);
            return 1;
        }
    }

    snprintf(path, sizeof(path), "host:%s", lower);
    fd = open(path, O_RDONLY);
    if (fd >= 0) {
        close(fd);
        return 1;
    }

    snprintf(path, sizeof(path), "host0:%s", lower);
    fd = open(path, O_RDONLY);
    if (fd >= 0) {
        close(fd);
        return 1;
    }

    return 0;
}

/* ---- bounded "search module by name" ------------------------------------
 * ps2sdk's SifSearchModuleByName() blocks forever on PCSX2 (the emulator
 * does not answer that LOADFILE function), so the call is issued nowait and
 * polled for a limited time. */

#define LOADFILE_SID            0x80000006
#define LF_F_SEARCH_MOD_BY_NAME 9

struct lf_name_arg {
    int  id;
    int  dummy1;
    char name[64];
    char dummy2[64];
} __attribute__((aligned(16)));

static int search_module(const char *name)
{
    static SifRpcClientData_t cd __attribute__((aligned(64)));
    static struct lf_name_arg arg __attribute__((aligned(64)));
    int i;

    memset(&cd, 0, sizeof(cd));
    if (sceSifBindRpc(&cd, LOADFILE_SID, 0) < 0 || !cd.server)
        return -1;

    memset(&arg, 0, sizeof(arg));
    strncpy(arg.name, name, sizeof(arg.name) - 1);

    if (sceSifCallRpc(&cd, LF_F_SEARCH_MOD_BY_NAME, 1 /* nowait */,
                      &arg, sizeof(arg), &arg, sizeof(arg), NULL, NULL) < 0)
        return -1;

    for (i = 0; i < 400; i++) {
        int j;

        if (!sceSifCheckStatRpc(&cd))
            return arg.id;

        for (j = 0; j < 2000; j++)
            __asm__ __volatile__("nop\nnop\nnop\nnop");
    }

    return -1;
}

int mod_unload(const char *name)
{
    int id = search_module(name);

    if (id < 0)
        return 0;

    return SifUnloadModule(id) >= 0;
}
