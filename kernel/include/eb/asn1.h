/*
 * asn1.h -- DER reading: elements, integers, object identifiers, times.
 * - a span is read left to right; an element is its tag, its whole extent and its content
 * - lengths must be definite and minimal (DER); tags above 30 are not needed by X.509 and refused
 */
#ifndef EB_ASN1_H
#define EB_ASN1_H

#include <eb/types.h>

#define ASN1_BOOLEAN      0x01
#define ASN1_INTEGER      0x02
#define ASN1_BITSTRING    0x03
#define ASN1_OCTETSTRING  0x04
#define ASN1_NULL         0x05
#define ASN1_OID          0x06
#define ASN1_UTF8STRING   0x0C
#define ASN1_PRINTABLE    0x13
#define ASN1_IA5STRING    0x16
#define ASN1_UTCTIME      0x17
#define ASN1_GENTIME      0x18
#define ASN1_SEQUENCE     0x30
#define ASN1_SET          0x31
#define ASN1_CONTEXT(n)   (0x80 | (n))    /* [n], primitive */
#define ASN1_CONTEXT_C(n) (0xA0 | (n))    /* [n], constructed */

typedef struct { const u8 *p; const u8 *end; } asn1_span;

typedef struct {
    u8        tag;
    const u8 *head;    /* the identifier octet */
    u32       total;   /* identifier, length octets and content together */
    const u8 *p;       /* the content */
    u32       len;
} asn1_tlv;

void asn1_span_of(asn1_span *s, const u8 *p, u32 len);
bool asn1_done(const asn1_span *s);
void asn1_inside(const asn1_tlv *t, asn1_span *s);   /* the content as a span, for descending */

/* The next element; false at the end of the span or on an encoding that is not DER. */
bool asn1_next(asn1_span *s, asn1_tlv *out);
/* The next element, which must carry the tag. */
bool asn1_expect(asn1_span *s, u8 tag, asn1_tlv *out);
/* The next element's tag without taking it. */
bool asn1_peek(const asn1_span *s, u8 *tag);

/* Whether an OID element holds the given encoded identifier. */
bool asn1_oid_is(const asn1_tlv *t, const u8 *oid, u32 oidlen);

/* An INTEGER as unsigned big-endian bytes with leading zeros removed; false if negative or empty. */
bool asn1_uint(const asn1_tlv *t, const u8 **p, u32 *len);

/* A BIT STRING with no unused bits: its bytes. */
bool asn1_bits(const asn1_tlv *t, const u8 **p, u32 *len);

/* UTCTime or GeneralizedTime as seconds since 1970 (utc). */
bool asn1_time(const asn1_tlv *t, i64 *out);

/* A civil date and time (utc) as seconds since 1970. */
i64 asn1_unix_time(u32 year, u32 month, u32 day, u32 hour, u32 min, u32 sec);

#endif /* EB_ASN1_H */
