#ifndef MODULES_H
#define MODULES_H

/* Load an IOP module by file name ("SIO2MAN.IRX") from every place it can
 * plausibly live: the disc image, the USB stick / memory card next to the
 * ELF, and the host: device used by ps2link.  Returns the first successful
 * result (>= 0) or a negative error. */
int mod_load(const char *name);

/* Path that produced the last successful mod_load(), or "" . */
const char *mod_last_path(void);

/* Directory that holds the ELF, used as the first place to look.  Pass the
 * value of argv[0] (or NULL when the loader gives no arguments). */
void mod_set_base_dir(const char *dir);

#endif
