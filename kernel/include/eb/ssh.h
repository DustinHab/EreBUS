/*
 * ssh.h -- the door: ssh version 2 into a terminal session of the visitor's own.
 * - one profile: curve25519-sha256, ssh-ed25519, aes128-gcm@openssh.com
 * - who may come in: "door |" lines in the settings, one public key each
 * - several visitors at once (one per door slot); a client-driven rekey is honoured
 */
#ifndef EB_SSH_H
#define EB_SSH_H

#include <eb/types.h>

/* The host's key: thirty-two bytes of seed and the thirty-two of
 * public key it makes. Made once, kept in the graph, given here at
 * every start. */
void ssh_make_key(u8 key[64]);
void ssh_init(const u8 key[64]);

/* "SHA256:" and the letters -- what ssh-keygen -l would print, so the
 * person can check the client's first-visit warning against it. */
void ssh_fingerprint(char out[64]);

/* The same fingerprint for any key, and the door's key used as the
 * machine's identity: what it is, and a signature with it. Both answer
 * false before the key stands. */
void ssh_fingerprint_of(const u8 pub[32], char out[64]);
bool ssh_identity(u8 pub[32]);
bool ssh_sign(const void *msg, u32 len, u8 sig[64]);

/* The protocol, run from the net thread's loop. */
void ssh_service(void);

#endif /* EB_SSH_H */
