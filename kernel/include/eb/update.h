#ifndef EB_UPDATE_H
#define EB_UPDATE_H

#include <eb/types.h>

/* Self-update from a release over the network.
 *
 * A machine with `update | auto` set checks, now and then, whether a
 * newer release exists; if so it fetches the kernel and a detached
 * signature, verifies the signature against a public key built into this
 * kernel, and only then installs it and restarts. The transport (tls,
 * without certificate verification yet) provides privacy; the signature
 * provides authenticity -- a kernel that does not verify is refused, so
 * no man in the middle can plant one. The loader's two-failed-boots
 * rollback to kernel.old is the last net under all of this.
 *
 * The base is `https://github.com/DustinHab/EreBUS/releases/latest/
 * download` by default; `update from <base>` in settings points it
 * elsewhere (a local server, for a test). Under the base it expects
 * three files: `version` (the newest version, one line), `kernel.elf`,
 * and `kernel.elf.sig` (64 raw bytes). */

/* Asks for a check at the next network turn (the terminal's
 * 'update check'). Runs in the network thread, not the caller's. */
void update_request(void);

/* Called once per network-thread turn: services a requested check, the
 * periodic automatic check, and the pending restart. */
void update_tick(void);

/* The outcome of a hand-asked check, waiting for the shell to print it
 * in the terminal (the check runs elsewhere and cannot answer on the
 * same line). True and filled once when there is one, then cleared. */
bool update_report(char *out, u32 max);

#endif /* EB_UPDATE_H */
