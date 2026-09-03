#ifndef EB_WIFI_H
#define EB_WIFI_H

#include <eb/types.h>

/* A wireless station.
 *
 * Above: the same frames the wire carries. Below: a radio, which is
 * an interface with three verbs -- send a frame into the air, hand
 * frames that arrived up, name the antenna's address. Between them,
 * here: the search for networks, the joining of one with a passphrase
 * the WPA2 way -- the passphrase into a master key, the four-way
 * handshake into session keys, every data frame sealed with CCMP --
 * and the remembering of networks joined before.
 *
 * The one radio the machine has today is the test bench's: 802.11
 * frames carried inside ethernet frames on the wire, to a virtual
 * access point on the host. A real chip's driver plugs in underneath
 * the same three verbs.
 */

#define WIFI_OPEN  0                  /* no key at all */
#define WIFI_WPA2  1                  /* wpa2 with a passphrase: joinable */
#define WIFI_OTHER 2                  /* wep, wpa1, enterprise: seen, not joinable */

typedef struct {
    char ssid[33];
    u8   bssid[6];
    u8   channel;
    i8   rssi;
    u8   security;
    bool joined;
    bool remembered;
} wifi_net;

void wifi_init(void);                 /* the self test, and the radio */
bool wifi_radio_present(void);
const char *wifi_radio_name(void);

/* Networks heard, newest listing. wifi_scan asks the air; the answers
 * arrive over the next moments. */
void wifi_scan(void);
u32  wifi_networks(wifi_net *out, u32 max);

/* Joining. The passphrase may be empty for an open network. The work
 * happens on the net thread; wifi_state and the journal tell how it
 * went. */
void wifi_join(const char *ssid, const char *pass);
void wifi_leave(void);
bool wifi_up(void);                   /* joined, keys in place */
const char *wifi_state(char *out, u32 max);   /* a line for the person */
bool wifi_joined_name(char *out, u32 max);

/* The link's two halves, for the card layer: frames the stack sends
 * while joined go into the air; frames that came out of the air wait
 * here for the stack. */
bool wifi_send(const void *frame, u32 len);
i32  wifi_take(void *out, u32 max);

/* The radio's incoming frames, from whichever radio there is. The
 * test bench's radio calls this with what it found on the wire. */
void wifi_radio_input(const u8 *frame, u32 len, i8 rssi, u8 channel);

/* Periodic work, from the net thread. */
void wifi_poll(void);

#endif /* EB_WIFI_H */
