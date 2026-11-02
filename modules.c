/*
 * IOP module loader that does not depend on where the ELF was launched from.
 *
 * An ELF started as an OPL app has no cdrom0: with the modules on it, and the
 * USB stick can be mass0:, mass1:, ... depending on the driver in use, so a
 * single hardcoded path is not enough.  Every candidate is tried in order and
 * the first one that loads wins.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <loadfile.h>

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

static void lower_copy(char *dst, const char *src, size_t size)
{
    size_t i;

    for (i = 0; src[i] && i + 1 < size; i++)
        dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = '\0';
}

int mod_load(const char *name)
{
    static const char *const fmt[] = {
        "%s%s",              /* directory of the ELF, when known */
        "cdrom0:\\%s;1",     /* self-contained ISO */
        "mass:/APPS/%s", "mass0:/APPS/%s", "mass1:/APPS/%s",
        "mass:/%s",      "mass0:/%s",      "mass1:/%s",
        "mc0:/APPS/%s",  "mc0:/%s",        "mc1:/%s",
    };
    char path[160];
    char lower[48];
    int ret;
    size_t i;

    lower_copy(lower, name, sizeof(lower));

    for (i = 0; i < sizeof(fmt) / sizeof(fmt[0]); i++) {
        if (i == 0 && base_dir[0] == '\0')
            continue;

        snprintf(path, sizeof(path), fmt[i], base_dir, name);
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

    last_path[0] = '\0';
    return -1;
}
