/*
 * asn1.c -- DER reading for X.509: elements, integers, object identifiers, times.
 * - definite, minimal lengths only; indefinite form and lengths past four octets are refused
 * - no allocation, no copying: every element points into the bytes it was read from
 */
#include <eb/asn1.h>

void asn1_span_of(asn1_span *s, const u8 *p, u32 len) { s->p = p; s->end = p + len; }
bool asn1_done(const asn1_span *s) { return s->p >= s->end; }
void asn1_inside(const asn1_tlv *t, asn1_span *s) { s->p = t->p; s->end = t->p + t->len; }

bool asn1_next(asn1_span *s, asn1_tlv *out)
{
    const u8 *p = s->p;
    if (p >= s->end) return false;
    u32 avail = (u32)(s->end - p);
    if (avail < 2) return false;

    u8 tag = p[0];
    if ((tag & 0x1F) == 0x1F) return false;        /* high tag numbers: not used here */

    u32 len, hl;
    u8 l0 = p[1];
    if (l0 < 0x80) {
        len = l0;
        hl = 2;
    } else {
        u32 nb = l0 & 0x7F;
        if (nb == 0 || nb > 4) return false;        /* indefinite, or absurd */
        if (avail < 2 + nb) return false;
        len = 0;
        for (u32 i = 0; i < nb; i++) len = (len << 8) | p[2 + i];
        /* DER: the shortest form that holds the length. */
        if (nb == 1 && len < 0x80) return false;
        if (nb >= 2 && len < (1u << (8 * (nb - 1)))) return false;
        hl = 2 + nb;
    }
    if (len > avail - hl) return false;

    out->tag = tag;
    out->head = p;
    out->total = hl + len;
    out->p = p + hl;
    out->len = len;
    s->p = p + hl + len;
    return true;
}

bool asn1_expect(asn1_span *s, u8 tag, asn1_tlv *out)
{
    asn1_span save = *s;
    if (!asn1_next(s, out)) return false;
    if (out->tag != tag) { *s = save; return false; }
    return true;
}

bool asn1_peek(const asn1_span *s, u8 *tag)
{
    if (s->p >= s->end) return false;
    *tag = s->p[0];
    return true;
}

bool asn1_oid_is(const asn1_tlv *t, const u8 *oid, u32 oidlen)
{
    if (t->tag != ASN1_OID || t->len != oidlen) return false;
    for (u32 i = 0; i < oidlen; i++) if (t->p[i] != oid[i]) return false;
    return true;
}

bool asn1_uint(const asn1_tlv *t, const u8 **p, u32 *len)
{
    if (t->tag != ASN1_INTEGER || t->len == 0) return false;
    if (t->p[0] & 0x80) return false;               /* negative */
    const u8 *q = t->p;
    u32 n = t->len;
    while (n > 1 && q[0] == 0) { q++; n--; }
    *p = q;
    *len = n;
    return true;
}

bool asn1_bits(const asn1_tlv *t, const u8 **p, u32 *len)
{
    if (t->tag != ASN1_BITSTRING || t->len < 1) return false;
    if (t->p[0] != 0) return false;                 /* unused bits: none expected here */
    *p = t->p + 1;
    *len = t->len - 1;
    return true;
}

/* Days since 1970-01-01 of a civil date, for any year of the era. */
static i64 days_from_civil(i64 y, u32 m, u32 d)
{
    y -= m <= 2;
    i64 era = (y >= 0 ? y : y - 399) / 400;
    u64 yoe = (u64)(y - era * 400);
    u64 doy = (153 * (m + (m > 2 ? (u32)-3 : 9)) + 2) / 5 + d - 1;
    u64 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (i64)doe - 719468;
}

i64 asn1_unix_time(u32 year, u32 month, u32 day, u32 hour, u32 min, u32 sec)
{
    return days_from_civil((i64)year, month, day) * 86400
         + (i64)hour * 3600 + (i64)min * 60 + (i64)sec;
}

static bool two_digits(const u8 *p, u32 *out)
{
    if (p[0] < '0' || p[0] > '9' || p[1] < '0' || p[1] > '9') return false;
    *out = (u32)(p[0] - '0') * 10 + (u32)(p[1] - '0');
    return true;
}

bool asn1_time(const asn1_tlv *t, i64 *out)
{
    const u8 *p = t->p;
    u32 year;
    if (t->tag == ASN1_UTCTIME) {
        /* YYMMDDHHMMSSZ; years 50..99 are 1950..1999, 00..49 are 2000..2049 (RFC 5280). */
        if (t->len != 13) return false;
        u32 yy;
        if (!two_digits(p, &yy)) return false;
        year = yy >= 50 ? 1900 + yy : 2000 + yy;
        p += 2;
    } else if (t->tag == ASN1_GENTIME) {
        /* YYYYMMDDHHMMSSZ. */
        if (t->len != 15) return false;
        u32 hi, lo;
        if (!two_digits(p, &hi) || !two_digits(p + 2, &lo)) return false;
        year = hi * 100 + lo;
        p += 4;
    } else {
        return false;
    }
    u32 mo, d, h, mi, s;
    if (!two_digits(p, &mo) || !two_digits(p + 2, &d) || !two_digits(p + 4, &h) ||
        !two_digits(p + 6, &mi) || !two_digits(p + 8, &s) || p[10] != 'Z')
        return false;
    if (mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || s > 60) return false;
    *out = asn1_unix_time(year, mo, d, h, mi, s);
    return true;
}
