#ifndef EB_WEB_H
#define EB_WEB_H

#include <eb/types.h>
#include <eb/object.h>

/* The browser's side of the network: one request at a time, carried
 * by the network thread while the shell keeps drawing; the cookie
 * jar; the bookmarks. The page itself lives in the browser's own
 * memory, not in the graph -- what the person keeps, they keep on
 * purpose (a bookmark, a page laid down as a text). */

#define WEB_GET  0
#define WEB_POST 1

typedef struct {
    bool ok;                      /* an answer came and was read */
    u32  status;                  /* http status of the final hop; 0 when none */
    u8  *data;                    /* the body, unpacked, in the buffer given to web_ask */
    u32  len;
    char final_url[512];          /* where the page was, after redirects */
    char ctype[64];               /* content-type, lower case, without parameters */
    bool secure, verified;        /* over tls; and the server proved who it is */
    bool gzip, chunked;           /* how it came */
    bool cut;                     /* longer than the buffer: the rest was dropped */
    const char *reason;           /* when unverified or failed: why, in a sentence */
} web_answer;

/* Asks for a url. out is where the answer lands (the raw response
 * first, the unpacked body in place after); the id names the ask.
 * 0 when a request is already out or there is no network. */
u32  web_ask(const char *url, u8 method, const u8 *body, u32 blen, u8 *out, u32 max);

/* Whether the ask is done; then the answer is filled and the slot is
 * free for the next ask. */
bool web_finished(u32 id, web_answer *a);
bool web_busy(void);

/* From the network thread's loop: carries the outstanding ask. */
void web_service(void);

/* The cookie jar's text on the system shelf (lines the kernel writes;
 * only cookies with an expiry are kept across a restart), and the
 * bookmarks text (lines the person and the browser write). */
void    web_cookies_adopt(object *t);
object *web_cookies_object(void);
void    web_bookmarks_set(object *t);
object *web_bookmarks_object(void);
u32     web_cookie_count(void);

/* Forgets every cookie, in memory and on the shelf. */
void web_cookies_clear(void);

#endif /* EB_WEB_H */
