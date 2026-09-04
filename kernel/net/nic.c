/*
 * nic.c -- the one driven card, and the wireless station above it.
 * - drivers probe in turn (cards with link first, then any); the first that finds its silicon registers here
 * - while joined to a wireless network, frames go sealed through wifi.c
 */
#include <eb/net.h>
#include <eb/wifi.h>
#include <eb/pci.h>
#include <eb/fmt.h>

#define ETH_RADIO 0x88B5              /* the test bench's radio: 802.11 inside ethernet */

static const nic_ops *card;

void nic_register(const nic_ops *ops) { card = ops; }

/* Everything on the bus that is a network card, named, so a machine
 * whose card nobody here drives says which card that was. Guessing
 * from "no network" is how an evening goes missing. */
static void name_what_was_not_driven(void)
{
    for (u32 i = 0; i < pci_device_count(); i++) {
        const pci_device *d = pci_get(i);
        if (!d || d->class_code != 0x02) continue;
        kprintf("net:  a network card at %02x:%02x.%u, %04x:%04x, "
                "which no driver here knows\n",
                d->bus, d->device, d->function, d->vendor, d->device_id);
    }
}

bool nic_start(void)
{
    static bool (*const probe[])(bool) = {
        e1000_init, igb_init, rtl8169_init, rtl8139_init,
    };

    /* Twice round: first only cards with a cable in them, then any
     * card at all. */
    for (u32 pass = 0; pass < 2; pass++)
        for (u32 i = 0; i < sizeof(probe) / sizeof(probe[0]); i++)
            if (probe[i](pass == 0)) return true;

    name_what_was_not_driven();
    return false;
}

bool        nic_up(void)   { return card != NULL; }
const char *nic_name(void) { return card ? card->name : "none"; }
const u8   *nic_mac(void)  { return card ? card->mac() : (const u8 *)"\0\0\0\0\0\0"; }

/* Straight to the card, whatever the station is doing: how the test
 * bench's radio puts its frames on the wire. */
bool nic_send_raw(const void *frame, u32 len)
{
    return card ? card->send(frame, len) : false;
}

bool nic_send(const void *frame, u32 len)
{
    if (wifi_up()) return wifi_send(frame, len);
    return card ? card->send(frame, len) : false;
}

i32 nic_recv(void *out, u32 max)
{
    i32 n = wifi_take(out, max);
    if (n > 0) return n;
    for (;;) {
        i32 got = card ? card->recv(out, max) : -1;
        if (got < 14) return got;
        const u8 *f = (const u8 *)out;
        u16 type = (u16)(((u16)f[12] << 8) | f[13]);
        if (type == ETH_RADIO && got >= 18 && f[14] == 'R') {
            /* four bytes of the bench's own: a mark, a version, the
             * signal, the channel; then the frame as the air had it */
            wifi_radio_input(f + 18, (u32)got - 18, (i8)f[16], f[17]);
            continue;
        }
        return got;
    }
}
