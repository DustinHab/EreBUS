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

/* The card. */
bool      e1000_init(void);
bool      e1000_up(void);
const u8 *e1000_mac(void);
bool      e1000_send(const void *frame, u32 len);
i32       e1000_recv(void *out, u32 max);

/* The service. Prepare early -- the port has to exist before the
 * fetch program starts holding a way to it -- and start late, once
 * the bus has been scanned and the card found. */
void    net_prepare(domain *kernel);
bool    net_start(void);
object *net_port(void);
bool    net_up(void);

#endif /* EB_NET_H */
