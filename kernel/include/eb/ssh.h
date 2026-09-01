/*
 * ssh.h -- the door: the terminal reached over the network.
 *
 * Real ssh, so that any client anyone already has can knock: version
 * 2, curve25519-sha256 for the exchange, ssh-ed25519 for the host's
 * name and the visitor's, aes128-gcm@openssh.com on the wire. One
 * profile, because every algorithm beyond the needed ones is attack
 * surface wearing a feature's clothes.
 *
 * Who may come in is not a user table: it is the "door |" lines in
 * the settings, each one a public key. The key is the person. A
 * visitor who proves they hold one gets a terminal session of their
 * own, walking the same graph from the same beginning as the screen.
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

/* The protocol, run from the net thread's loop. */
void ssh_service(void);

#endif /* EB_SSH_H */
