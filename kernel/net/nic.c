/*
 * nic.c -- whichever card answered.
 *
 * The drivers probe in turn and the first that finds its silicon
 * registers here. Everything above -- frames, protocols, the fetch
 * service -- speaks through these four calls and never learns which
 * family it is talking to, which is exactly as much as it needs to
 * not know.
 */
#include <eb/net.h>

static const nic_ops *card;

void nic_register(const nic_ops *ops) { card = ops; }

bool        nic_up(void)   { return card != NULL; }
const char *nic_name(void) { return card ? card->name : "none"; }
const u8   *nic_mac(void)  { return card ? card->mac() : (const u8 *)"\0\0\0\0\0\0"; }

bool nic_send(const void *frame, u32 len)
{
    return card ? card->send(frame, len) : false;
}

i32 nic_recv(void *out, u32 max)
{
    return card ? card->recv(out, max) : -1;
}
