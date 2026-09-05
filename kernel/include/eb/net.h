#ifndef EB_NET_H
#define EB_NET_H

#include <eb/types.h>
#include <eb/object.h>
#include <eb/cap.h>

/* The way out, and what it deliberately is not.
 *
 * There is no socket call and no address family. The network is a
 * service behind one port, and reaching it takes a capability like
 * reaching anything else: the fetch program is handed a send-only way
 * in when it starts, and whoever does not hold one cannot so much as
 * ask. Outbound only, one conversation at a time, plain http -- a
 * door, not a doorway standing open.
 *
 * A request is a text object whose first line names the page:
 *
 *   example.com
 *   example.com/some/path
 *   http://example.com/some/path
 *
 * The service fetches it and writes what came back into the same
 * object, after the asking line. The answer lands where the question
 * stood, which is the only addressing this system has.
 */

/* One card among possible cards. Each driver knows one family; the
 * first whose init finds silicon registers itself, and everything
 * above speaks to whichever card answered. */
typedef struct {
    const char *name;
    const u8 *(*mac)(void);
    bool (*send)(const void *frame, u32 len);
    i32  (*recv)(void *out, u32 max);
} nic_ops;

void        nic_register(const nic_ops *ops);
bool        nic_up(void);
const char *nic_name(void);
const u8   *nic_mac(void);
bool        nic_send(const void *frame, u32 len);
bool        nic_send_raw(const void *frame, u32 len);   /* past the station, to the card */
i32         nic_recv(void *out, u32 max);

/* The link changed underneath -- a wireless network joined or left:
 * the address is asked for anew. */
void net_relink(void);

/* The drivers. Intel's older family covers the emulators and a great
 * many cards besides; its later one covers the I210 and I350 that sit
 * on desktop boards; the two Realtek families cover most of the rest.
 * Each answers false quietly when its chip is not there.
 *
 * need_link asks a driver to pass over a card that has no cable in it.
 * A board with two sockets has one cable more often than not, and the
 * card that is plugged in is the one worth having; nic_start asks
 * first with the demand and then without it, so a machine with nothing
 * plugged in anywhere still comes up with a card rather than none. */
bool e1000_init(bool need_link);
bool igb_init(bool need_link);
bool rtl8139_init(bool need_link);
bool rtl8169_init(bool need_link);

/* Every driver in turn, cabled cards first. False when no card here
 * knows the silicon; what was seen and not driven is named in the log
 * either way. */
bool nic_start(void);

/* The service. Prepare early -- the port has to exist before the
 * fetch program starts holding a way to it -- and start late, once
 * the bus has been scanned and the card found. */
void    net_prepare(domain *kernel);
bool    net_start(void);
object *net_port(void);
bool    net_up(void);

/* The tcp stream underneath http and tls alike. One conversation at a
 * time; net.c owns the state, tls.c borrows the four calls. */
bool tcp_open(const u8 addr[4], u16 port);
bool tcp_write(const u8 *buf, u32 len);
i32  tcp_read(u8 *buf, u32 max);
bool tcp_eof(void);
void tcp_close(void);

/* One turn of looking at the wire and standing aside. The waiting
 * loops in tls.c call this between records. */
void net_breathe(void);

/* The door: server-side streams on port 22 for ssh to speak through.
 * Several visitors at once, one connection per slot, 0 .. door_count-1;
 * a knock takes a free slot, or the longest-idle one when all are busy.
 * door_visit counts the knocks on a slot so the protocol above notices a
 * new connection there. Reads and writes never block -- door_room says
 * how much a write may carry right now. */
u32  door_count(void);
bool door_alive(u32 c);
bool door_finished(u32 c);           /* the visitor sent their fin */
u64  door_visit(u32 c);
void door_peer(u32 c, u8 ip[4]);
u32  door_room(u32 c);
u32  door_read(u32 c, u8 *buf, u32 max);
bool door_write(u32 c, const u8 *buf, u32 len);
void door_close(u32 c);

/* One udp datagram to dst. The pipe rides on this. */
bool net_udp_send(const u8 dst[4], u16 sport, u16 dport,
                  const u8 *data, u32 len);

/* Whether the crypto primitives proved themselves at start. The pipe
 * refuses to carry anything without this; a seal that cannot vouch
 * for itself seals nothing. */
bool net_crypto_ok(void);

/* Our own address, once the machine has one. */
bool net_own_address(u8 ip[4]);

/* TLS 1.3 over that stream: connect to addr:443, run the handshake,
 * send the http request sealed, and hand back the decrypted response
 * exactly as http_fetch would hand back a plain one. The channel is
 * sealed against reading and tampering; the server's certificate is
 * NOT yet verified, so this proves privacy, not identity -- said in
 * the readme, and the next milestone. */
bool tls_get(const u8 addr[4], const char *host, u32 hlen,
             const char *path, u32 plen, u8 *out, u32 max, u32 *got);
bool tls_last_verified(void);   /* whether the server Finished checked out */

/* How the last page arrived, for the browser to mark: sealed means it
 * came over tls; verified means the handshake's own integrity check
 * passed. Neither means the server's identity was proven -- that waits
 * for certificate checking. */
bool net_last_secure(void);
bool net_last_verified(void);

#endif /* EB_NET_H */
