/*
 * web.c -- the browser's requests, carried by the network thread; the cookie jar; the bookmarks.
 * - one ask at a time: the shell asks, the network thread fetches (net_fetch_ex), the shell polls
 * - the answer is unpacked in place: chunked transfer coding undone, gzip inflated
 * - cookies: name, value, domain, path, expiry, secure; matched the way RFC 6265 says; those with an
 *   expiry are written to "the cookies" on the system shelf and read back at the next start
 */
#include <eb/web.h>
#include <eb/net.h>
#include <eb/inflate.h>
#include <eb/pmm.h>
#include <eb/mm.h>
#include <eb/time.h>
#include <eb/io.h>
#include <eb/fmt.h>
#include <eb/string.h>

/* ------------------------------------------------------------------ */
/* The one ask                                                         */
/* ------------------------------------------------------------------ */

#define BODY_MAX 4096
#define UNPACK_PAGES 512                          /* 2 MiB for the unpacked body */

static struct {
    volatile bool busy, done;
    u32  id;
    char url[512];
    u8   method;
    u8   body[BODY_MAX];
    u32  blen;
    u8  *out; u32 max;
    web_answer a;
} ask;
static u32 next_id = 1;
static u8 *unpack;

u32 web_ask(const char *url, u8 method, const u8 *body, u32 blen, u8 *out, u32 max)
{
    if (!url || !out || max < 4096 || blen > BODY_MAX || !net_up()) return 0;
    u64 fl = irq_save();
    if (ask.busy) { irq_restore(fl); return 0; }
    ask.busy = true;
    ask.done = false;
    irq_restore(fl);

    u32 n = 0;
    while (url[n] && n < sizeof(ask.url) - 1) { ask.url[n] = url[n]; n++; }
    ask.url[n] = 0;
    ask.method = method;
    ask.blen = blen;
    if (blen) memcpy(ask.body, body, blen);
    ask.out = out; ask.max = max;
    memset(&ask.a, 0, sizeof(ask.a));
    ask.id = next_id++;
    if (next_id == 0) next_id = 1;
    return ask.id;
}

bool web_finished(u32 id, web_answer *a)
{
    if (!ask.busy || ask.id != id || !ask.done) return false;
    if (a) *a = ask.a;
    u64 fl = irq_save();
    ask.busy = false;
    ask.done = false;
    irq_restore(fl);
    return true;
}

bool web_busy(void) { return ask.busy; }

/* ------------------------------------------------------------------ */
/* The cookie jar                                                      */
/* ------------------------------------------------------------------ */

#define JAR_MAX 64
typedef struct {
    bool used;
    char domain[64];              /* the host, or the Domain attribute without its dot */
    bool host_only;
    char path[64];
    char name[48];
    char value[192];
    u64  expires;                 /* seconds since 1970; 0 for the session */
    bool secure;
    u64  set_ns;                  /* when it came, for making room */
} cookie;

static cookie jar[JAR_MAX];
static object *jar_obj, *marks_obj;
static bool jar_dirty;

static u32 slen(const char *s) { u32 n = 0; while (s[n]) n++; return n; }

static bool same_ci(const char *a, u32 alen, const char *b)
{
    u32 n = slen(b);
    if (n != alen) return false;
    for (u32 i = 0; i < n; i++) {
        char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x = (char)(x + 32);
        if (y >= 'A' && y <= 'Z') y = (char)(y + 32);
        if (x != y) return false;
    }
    return true;
}

/* Whether host lies under domain: equal, or ends in ".domain". */
static bool domain_match(const char *host, u32 hlen, const char *domain)
{
    u32 dl = slen(domain);
    if (dl == 0 || dl > hlen) return false;
    if (dl == hlen) return same_ci(host, hlen, domain);
    if (host[hlen - dl - 1] != '.') return false;
    return same_ci(host + hlen - dl, dl, domain);
}

/* Whether the request path lies under the cookie's path. */
static bool path_match(const char *path, u32 plen, const char *cpath)
{
    u32 cl = slen(cpath);
    if (cl == 0) return true;
    if (cl > plen) return false;
    for (u32 i = 0; i < cl; i++) if (path[i] != cpath[i]) return false;
    if (cl == plen) return true;
    if (cpath[cl - 1] == '/') return true;
    return path[cl] == '/';
}

/* The days since 1970 of a civil date, and a date the way http writes
 * one: "Wed, 21 Oct 2026 07:28:00 GMT". 0 when it does not parse. */
static i64 days_from_civil(i64 y, u32 m, u32 d)
{
    y -= m <= 2;
    i64 era = (y >= 0 ? y : y - 399) / 400;
    u64 yoe = (u64)(y - era * 400);
    u64 doy = (153 * (m + (m > 2 ? (u32)-3 : 9)) + 2) / 5 + d - 1;
    u64 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (i64)doe - 719468;
}

static u64 parse_http_date(const char *s, u32 n)
{
    /* Skip the weekday; then day, month, year, time in any of the
     * usual orders (RFC 1123, RFC 850, asctime). */
    u32 at = 0;
    while (at < n && s[at] != ' ') at++;
    u32 day = 0, mon = 0, year = 0, hh = 0, mm = 0, ss = 0;
    static const char *months = "janfebmaraprmayjunjulaugsepoctnovdec";
    u32 numbers[4]; u32 nn = 0;
    while (at < n) {
        while (at < n && (s[at] == ' ' || s[at] == '-' || s[at] == ',')) at++;
        if (at >= n) break;
        if (s[at] >= '0' && s[at] <= '9') {
            u32 v = 0;
            while (at < n && s[at] >= '0' && s[at] <= '9') v = v * 10 + (u32)(s[at++] - '0');
            if (at < n && s[at] == ':') {
                hh = v; at++;
                while (at < n && s[at] >= '0' && s[at] <= '9') mm = mm * 10 + (u32)(s[at++] - '0');
                if (at < n && s[at] == ':') { at++; while (at < n && s[at] >= '0' && s[at] <= '9') ss = ss * 10 + (u32)(s[at++] - '0'); }
            } else if (nn < 4) {
                numbers[nn++] = v;
            }
        } else {
            char w[4] = { 0, 0, 0, 0 };
            u32 k = 0;
            while (at < n && ((s[at] >= 'a' && s[at] <= 'z') || (s[at] >= 'A' && s[at] <= 'Z'))) {
                char c = s[at++];
                if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
                if (k < 3) w[k] = c;
                k++;
            }
            if (k == 3) for (u32 i = 0; i < 12; i++)
                if (months[i * 3] == w[0] && months[i * 3 + 1] == w[1] && months[i * 3 + 2] == w[2]) mon = i + 1;
        }
    }
    for (u32 i = 0; i < nn; i++) {
        if (numbers[i] >= 1000) year = numbers[i];
        else if (numbers[i] >= 70 && numbers[i] <= 99) year = 1900 + numbers[i];
        else if (numbers[i] <= 31 && !day) day = numbers[i];
        else if (numbers[i] < 70 && !year && day) year = 2000 + numbers[i];
    }
    if (!day || !mon || !year || hh > 23 || mm > 59 || ss > 60) return 0;
    return (u64)days_from_civil((i64)year, mon, day) * 86400 + hh * 3600 + mm * 60 + ss;
}

static cookie *jar_room(void)
{
    cookie *oldest = NULL;
    for (u32 i = 0; i < JAR_MAX; i++) {
        if (!jar[i].used) return &jar[i];
        if (!oldest || jar[i].set_ns < oldest->set_ns) oldest = &jar[i];
    }
    return oldest;
}

static void jar_write_out(void);

/* One Set-Cookie value, from a response of host at path. */
static void jar_set(const char *host, u32 hlen, const char *path, u32 plen,
                    bool secure, const char *line, u32 len)
{
    cookie c;
    memset(&c, 0, sizeof(c));

    /* name=value up to the first ';' */
    u32 at = 0;
    while (at < len && line[at] == ' ') at++;
    u32 n = 0;
    while (at < len && line[at] != '=' && line[at] != ';') { if (n < sizeof(c.name) - 1) c.name[n++] = line[at]; at++; }
    while (n && c.name[n - 1] == ' ') n--;
    c.name[n] = 0;
    if (n == 0 || at >= len || line[at] != '=') return;
    at++;
    u32 v = 0;
    while (at < len && line[at] != ';') { if (v < sizeof(c.value) - 1) c.value[v++] = line[at]; at++; }
    while (v && c.value[v - 1] == ' ') v--;
    c.value[v] = 0;

    /* the attributes */
    bool has_domain = false, has_path = false;
    u64 max_age = 0; bool has_max_age = false;
    while (at < len) {
        if (line[at] == ';') at++;
        while (at < len && line[at] == ' ') at++;
        u32 ks = at;
        while (at < len && line[at] != '=' && line[at] != ';') at++;
        u32 klen = at - ks;
        while (klen && line[ks + klen - 1] == ' ') klen--;
        u32 vs = at, vlen = 0;
        if (at < len && line[at] == '=') {
            vs = ++at;
            while (at < len && line[at] != ';') at++;
            vlen = at - vs;
            while (vlen && line[vs + vlen - 1] == ' ') vlen--;
        }
        if (same_ci(line + ks, klen, "domain") && vlen) {
            u32 d = 0;
            if (line[vs] == '.') { vs++; vlen--; }
            for (u32 i = 0; i < vlen && d < sizeof(c.domain) - 1; i++) {
                char ch = line[vs + i];
                c.domain[d++] = (ch >= 'A' && ch <= 'Z') ? (char)(ch + 32) : ch;
            }
            c.domain[d] = 0;
            has_domain = d > 0;
        } else if (same_ci(line + ks, klen, "path") && vlen && line[vs] == '/') {
            u32 d = 0;
            for (u32 i = 0; i < vlen && d < sizeof(c.path) - 1; i++) c.path[d++] = line[vs + i];
            c.path[d] = 0;
            has_path = true;
        } else if (same_ci(line + ks, klen, "max-age") && vlen) {
            u64 m = 0; bool neg = false; u32 i = 0;
            if (line[vs] == '-') { neg = true; i = 1; }
            for (; i < vlen; i++) if (line[vs + i] >= '0' && line[vs + i] <= '9') m = m * 10 + (u64)(line[vs + i] - '0');
            max_age = neg ? 0 : m;
            has_max_age = true;
        } else if (same_ci(line + ks, klen, "expires") && vlen) {
            if (!has_max_age) c.expires = parse_http_date(line + vs, vlen);
        } else if (same_ci(line + ks, klen, "secure")) {
            c.secure = true;
        }
    }
    if (has_max_age) c.expires = max_age ? time_unix() + max_age : 1;   /* 1: already gone */

    /* The domain: the host itself unless a Domain the host lies under
     * was given -- a cookie for somebody else's domain is refused. */
    if (has_domain) {
        if (!domain_match(host, hlen, c.domain)) return;
        c.host_only = false;
    } else {
        u32 d = 0;
        for (u32 i = 0; i < hlen && d < sizeof(c.domain) - 1; i++) {
            char ch = host[i];
            c.domain[d++] = (ch >= 'A' && ch <= 'Z') ? (char)(ch + 32) : ch;
        }
        c.domain[d] = 0;
        c.host_only = true;
    }
    if (!has_path) {
        /* the request path's directory */
        u32 d = 0, last = 0;
        for (u32 i = 0; i < plen; i++) if (path[i] == '/') last = i;
        for (u32 i = 0; i < last && d < sizeof(c.path) - 1; i++) c.path[d++] = path[i];
        if (d == 0) c.path[d++] = '/';
        c.path[d] = 0;
    }
    if (c.secure && !secure) return;             /* a secure cookie only over tls */

    /* Replace the same cookie, or take a slot. An expiry in the past
     * removes it. */
    cookie *slot = NULL;
    for (u32 i = 0; i < JAR_MAX; i++)
        if (jar[i].used && same_ci(jar[i].domain, slen(jar[i].domain), c.domain) &&
            strcmp(jar[i].path, c.path) == 0 && strcmp(jar[i].name, c.name) == 0)
            slot = &jar[i];
    bool gone = c.expires && c.expires <= time_unix();
    if (gone) {
        if (slot) { slot->used = false; jar_dirty = true; }
        return;
    }
    if (!slot) slot = jar_room();
    c.used = true;
    c.set_ns = time_ns();
    *slot = c;
    if (c.expires) jar_dirty = true;
    kprintf("web:  cookie from %s: %s (%s)\n", c.domain, c.name, c.expires ? "kept" : "for the session");
}

/* The Cookie header's value for a request: every matching cookie. */
static u32 jar_header(const char *host, u32 hlen, const char *path, u32 plen,
                      bool secure, char *out, u32 max)
{
    u32 at = 0;
    u64 now = time_unix();
    for (u32 i = 0; i < JAR_MAX; i++) {
        cookie *c = &jar[i];
        if (!c->used) continue;
        if (c->expires && now && c->expires <= now) { c->used = false; jar_dirty = true; continue; }
        if (c->host_only ? !same_ci(host, hlen, c->domain) : !domain_match(host, hlen, c->domain)) continue;
        if (!path_match(path, plen, c->path)) continue;
        if (c->secure && !secure) continue;
        u32 need = slen(c->name) + 1 + slen(c->value) + 2;
        if (at + need >= max) break;
        if (at) { out[at++] = ';'; out[at++] = ' '; }
        for (u32 k = 0; c->name[k]; k++) out[at++] = c->name[k];
        out[at++] = '=';
        for (u32 k = 0; c->value[k]; k++) out[at++] = c->value[k];
    }
    out[at] = 0;
    return at;
}

u32 web_cookie_count(void)
{
    u32 n = 0;
    for (u32 i = 0; i < JAR_MAX; i++) if (jar[i].used) n++;
    return n;
}

void web_cookies_clear(void)
{
    for (u32 i = 0; i < JAR_MAX; i++) jar[i].used = false;
    jar_dirty = true;
    jar_write_out();
}

/* --- the shelf: "domain | path | name | value | expires | flags" --- */

static u32 put(char *d, u32 at, u32 max, const char *s)
{
    while (*s && at + 1 < max) d[at++] = *s++;
    return at;
}

static u32 put_num(char *d, u32 at, u32 max, u64 v)
{
    char tmp[24]; u32 n = 0;
    if (v == 0) tmp[n++] = '0';
    while (v) { tmp[n++] = (char)('0' + v % 10); v /= 10; }
    while (n && at + 1 < max) d[at++] = tmp[--n];
    return at;
}

static void jar_write_out(void)
{
    if (!jar_obj || !jar_dirty) return;
    char *d = (char *)obj_data(jar_obj);
    u64 size = obj_size(jar_obj);
    if (!d || size < 64) return;
    u32 max = (u32)size, at = 0;
    at = put(d, at, max, "domain | path | name | value | expires | flags\n");
    for (u32 i = 0; i < JAR_MAX; i++) {
        cookie *c = &jar[i];
        if (!c->used || !c->expires) continue;
        u32 need = slen(c->domain) + slen(c->path) + slen(c->name) + slen(c->value) + 48;
        if (at + need >= max) break;
        at = put(d, at, max, c->domain); at = put(d, at, max, " | ");
        at = put(d, at, max, c->path);   at = put(d, at, max, " | ");
        at = put(d, at, max, c->name);   at = put(d, at, max, " | ");
        at = put(d, at, max, c->value);  at = put(d, at, max, " | ");
        at = put_num(d, at, max, c->expires); at = put(d, at, max, " | ");
        at = put(d, at, max, c->host_only ? "host" : "domain");
        if (c->secure) at = put(d, at, max, " secure");
        at = put(d, at, max, "\n");
    }
    for (u64 i = at; i < size; i++) d[i] = 0;
    obj_touch(jar_obj);
    jar_dirty = false;
}

static u32 field(const char *s, u32 len, u32 *at, char *out, u32 max)
{
    u32 n = 0;
    while (*at < len && s[*at] != '|' && s[*at] != '\n') {
        if (n < max - 1) out[n++] = s[*at];
        (*at)++;
    }
    while (n && out[n - 1] == ' ') n--;
    out[n] = 0;
    if (*at < len && s[*at] == '|') { (*at)++; while (*at < len && s[*at] == ' ') (*at)++; }
    return n;
}

void web_cookies_adopt(object *t)
{
    if (jar_obj) obj_release(jar_obj);
    jar_obj = t;
    if (!t) return;
    obj_retain(t);
    const char *d = (const char *)obj_data(t);
    u64 size = obj_size(t);
    if (!d) return;
    u32 len = 0;
    while (len < size && d[len]) len++;
    u32 at = 0;
    u64 now = time_unix();
    u32 loaded = 0;
    while (at < len) {
        u32 ls = at;
        cookie c;
        memset(&c, 0, sizeof(c));
        char num[24], flags[24];
        field(d, len, &at, c.domain, sizeof(c.domain));
        field(d, len, &at, c.path, sizeof(c.path));
        field(d, len, &at, c.name, sizeof(c.name));
        field(d, len, &at, c.value, sizeof(c.value));
        field(d, len, &at, num, sizeof(num));
        field(d, len, &at, flags, sizeof(flags));
        while (at < len && d[at] != '\n') at++;
        if (at < len) at++;
        if (at == ls) break;
        if (ls == 0) continue;                        /* the header line */
        u64 e = 0;
        for (u32 i = 0; num[i]; i++) if (num[i] >= '0' && num[i] <= '9') e = e * 10 + (u64)(num[i] - '0');
        if (!c.domain[0] || !c.name[0] || !e) continue;
        if (now && e <= now) continue;
        c.expires = e;
        c.host_only = flags[0] == 'h';
        c.secure = strcmp(flags, "host secure") == 0 || strcmp(flags, "domain secure") == 0;
        c.used = true;
        c.set_ns = time_ns();
        cookie *slot = jar_room();
        *slot = c;
        loaded++;
    }
    if (loaded) kprintf("web:  %u cookie%s kept from before\n", loaded, loaded == 1 ? "" : "s");
}

object *web_cookies_object(void) { return jar_obj; }

void web_bookmarks_set(object *t)
{
    if (marks_obj) obj_release(marks_obj);
    marks_obj = t;
    if (!t) return;
    obj_retain(t);
    const char *d = (const char *)obj_data(t);
    u64 size = obj_size(t);
    u32 n = 0;
    for (u64 i = 0; d && i < size && d[i]; i++) if (d[i] == '\n') n++;
    if (n) kprintf("web:  %u bookmark%s kept\n", n, n == 1 ? "" : "s");
}

object *web_bookmarks_object(void) { return marks_obj; }

/* ------------------------------------------------------------------ */
/* Carrying the ask                                                    */
/* ------------------------------------------------------------------ */

static u32 cookies_for(void *ctx, const char *host, u32 hlen, const char *path, u32 plen,
                       bool secure, char *out, u32 max)
{
    (void)ctx;
    return jar_header(host, hlen, path, plen, secure, out, max);
}

static void cookie_from(void *ctx, const char *host, u32 hlen, const char *path, u32 plen,
                        bool secure, const char *line, u32 len)
{
    (void)ctx;
    jar_set(host, hlen, path, plen, secure, line, len);
}

/* A header's value from the response head, lower-cased, or 0 length. */
static u32 header_value(const u8 *d, u32 head, const char *name, char *out, u32 max)
{
    u32 nl = slen(name);
    for (u32 at = 0; at + nl + 1 < head; at++) {
        if (at && d[at - 1] != '\n') continue;
        u32 k = 0;
        while (k < nl) {
            u8 c = d[at + k];
            if (c >= 'A' && c <= 'Z') c = (u8)(c + 32);
            if (c != (u8)name[k]) break;
            k++;
        }
        if (k < nl || d[at + nl] != ':') continue;
        u32 j = at + nl + 1;
        while (j < head && d[j] == ' ') j++;
        u32 n = 0;
        while (j < head && d[j] != '\r' && d[j] != '\n' && n < max - 1) {
            u8 c = d[j++];
            out[n++] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : (char)c;
        }
        out[n] = 0;
        return n;
    }
    out[0] = 0;
    return 0;
}

/* Undoes the chunked transfer coding in place; the new length, or -1. */
static i32 dechunk(u8 *b, u32 len)
{
    u32 in = 0, out = 0;
    for (;;) {
        u32 size = 0; bool digits = false;
        while (in < len && b[in] != '\r' && b[in] != '\n' && b[in] != ';') {
            u8 c = b[in++];
            u32 v;
            if (c >= '0' && c <= '9') v = c - '0';
            else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
            else if (c == ' ') continue;
            else return -1;
            if (size > 0x0FFFFFFF) return -1;
            size = size * 16 + v;
            digits = true;
        }
        if (!digits) return -1;
        while (in < len && b[in] != '\n') in++;          /* extensions, then the line end */
        if (in >= len) return -1;
        in++;
        if (size == 0) return (i32)out;                   /* the last chunk; trailers ignored */
        if (size > len - in) size = len - in;             /* a cut response: what there is */
        memmove(b + out, b + in, size);
        out += size;
        in += size;
        while (in < len && (b[in] == '\r' || b[in] == '\n')) in++;
        if (in >= len) return (i32)out;
    }
}

void web_service(void)
{
    if (!ask.busy || ask.done) return;

    net_request r;
    memset(&r, 0, sizeof(r));
    r.method = ask.method;
    r.body = ask.blen ? ask.body : NULL;
    r.blen = ask.blen;
    r.accept_gzip = true;
    r.cookies = cookies_for;
    r.cookie = cookie_from;

    web_answer *a = &ask.a;
    bool ok = net_fetch_ex(ask.url, slen(ask.url), ask.out, ask.max, &r);
    a->ok = ok;
    a->status = r.status;
    a->secure = r.secure;
    a->verified = r.verified;
    a->reason = r.reason ? r.reason : "";
    memcpy(a->final_url, r.final_url, sizeof(a->final_url));
    a->data = ask.out + r.body_off;
    a->len = ok ? r.body_len : 0;
    a->cut = r.cut;

    if (ok) {
        char v[64];
        header_value(ask.out, r.body_off, "content-type", v, sizeof(v));
        u32 n = 0;
        while (v[n] && v[n] != ';' && n < sizeof(a->ctype) - 1) { a->ctype[n] = v[n]; n++; }
        while (n && a->ctype[n - 1] == ' ') n--;
        a->ctype[n] = 0;

        header_value(ask.out, r.body_off, "transfer-encoding", v, sizeof(v));
        if (v[0] == 'c') {
            a->chunked = true;
            i32 nl = dechunk(a->data, a->len);
            a->len = nl < 0 ? 0 : (u32)nl;
        }
        header_value(ask.out, r.body_off, "content-encoding", v, sizeof(v));
        if (v[0] == 'g' || (v[0] == 'x' && v[2] == 'g')) {
            a->gzip = true;
            if (!unpack) {
                phys_addr p = pmm_alloc_contig(UNPACK_PAGES);
                if (p != PMM_NO_FRAME) unpack = (u8 *)phys_to_virt(p);
            }
            u32 room = ask.max - r.body_off;
            i64 got = unpack ? inflate_gzip(a->data, a->len, unpack, UNPACK_PAGES * PAGE_SIZE) : -1;
            if (got < 0) {
                a->len = 0;
                a->reason = "the packed body did not unpack";
            } else {
                if ((u32)got > room) { got = room; a->cut = true; }
                memcpy(a->data, unpack, (u32)got);
                a->len = (u32)got;
            }
        }
    }
    if (jar_dirty) jar_write_out();

    bool moved = ok && strcmp(a->final_url, ask.url) != 0;
    kprintf("web:  %s %s -> %s%u, %u bytes%s%s%s%s%s%s%s\n",
            ask.method == WEB_POST ? "post" : "get", ask.url,
            ok ? "" : "no answer, ", r.status, a->len,
            a->ctype[0] ? ", " : "", a->ctype,
            a->gzip ? ", gzip" : "", a->chunked ? ", chunked" : "",
            a->cut ? ", cut" : "",
            moved ? " at " : "", moved ? a->final_url : "");
    if (a->secure)
        kprintf("web:  the server is %s\n", a->verified ? "verified" : "not verified; the page is marked");

    __atomic_store_n(&ask.done, true, __ATOMIC_RELEASE);
}
