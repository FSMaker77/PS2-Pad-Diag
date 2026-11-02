#ifndef MODULES_H
#define MODULES_H

/* RPC server ids of the modules we care about. */
#define PADMAN_RPC_ID 0x80000100
#define AUDSRV_RPC_ID 0x0870884e

/* Load an IOP module by file name ("SIO2MAN.IRX") from every place it can
 * plausibly live: the disc image, the USB stick / memory card next to the
 * ELF, and the host: device used by ps2link.
 *
 * Callers should only use this when the matching RPC server is not already
 * alive (see mod_rpc_alive): loading a second copy of sio2man/padman puts two
 * drivers on the SIO2 bus and the pads stop answering with PAD_STATE_ERROR.
 *
 * Returns 0 when the module is loaded, or a negative error. */
int mod_load(const char *name);

/* Path that produced the last successful mod_load(), or "" . */
const char *mod_last_path(void);

/* Directory that holds the ELF, used as the first place to look.  Pass the
 * value of argv[0] (or NULL when the loader gives no arguments). */
void mod_set_base_dir(const char *dir);

/* Non blocking: is an RPC server with this id already running in the IOP?
 * Unlike sceSifBindRpc(..., 0) this never waits forever when the server is
 * absent. */
int mod_rpc_alive(int sid);

/* Can one of the candidate locations for this module be opened?  Used before
 * unloading a module, so we only do it when we can put it back. */
int mod_can_open(const char *name);

/* Unload a resident module by name ("padman").  Returns 1 when something was
 * unloaded. */
int mod_unload(const char *name);

#endif
