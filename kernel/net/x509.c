/*
 * x509.c -- certificates: parsing, host names, dates, and the walk to a trusted authority.
 * - parsing keeps pointers into the DER; nothing is copied but the point of an EC key
 * - the walk starts at the server's first certificate; each step needs a signature that verifies under a trusted
 *   authority's key (done) or under the key of another certificate in the chain that is marked as an authority
 * - names select the next certificate; signatures are the proof
 * - the built-in authorities (kernel/net/authorities.h) are the intermediates that sign github.com and its release cdn
 */
#include <eb/pki.h>
#include <eb/asn1.h>
#include <eb/crypto.h>
#include <eb/string.h>
#include "authorities.h"

static const u8 OID_ECDSA_SHA256[] = { 0x2A,0x86,0x48,0xCE,0x3D,0x04,0x03,0x02 };
static const u8 OID_RSA_SHA256[]   = { 0x2A,0x86,0x48,0x86,0xF7,0x0D,0x01,0x01,0x0B };
static const u8 OID_EC_KEY[]       = { 0x2A,0x86,0x48,0xCE,0x3D,0x02,0x01 };
static const u8 OID_P256[]         = { 0x2A,0x86,0x48,0xCE,0x3D,0x03,0x01,0x07 };
static const u8 OID_RSA_KEY[]      = { 0x2A,0x86,0x48,0x86,0xF7,0x0D,0x01,0x01,0x01 };
static const u8 OID_SAN[]          = { 0x55,0x1D,0x11 };
static const u8 OID_BASIC[]        = { 0x55,0x1D,0x13 };
static const u8 OID_KEY_USAGE[]    = { 0x55,0x1D,0x0F };
static const u8 OID_EKU[]          = { 0x55,0x1D,0x25 };
static const u8 OID_SERVER_AUTH[]  = { 0x2B,0x06,0x01,0x05,0x05,0x07,0x03,0x01 };
static const u8 OID_ANY_EKU[]      = { 0x55,0x1D,0x25,0x00 };

/* Extensions that carry nothing this checker acts on, so that one of
 * them being critical is not a reason to refuse the certificate. */
static const u8 OID_SKI[]        = { 0x55,0x1D,0x0E };
static const u8 OID_AKI[]        = { 0x55,0x1D,0x23 };
static const u8 OID_POLICIES[]   = { 0x55,0x1D,0x20 };
static const u8 OID_CRL_DP[]     = { 0x55,0x1D,0x1F };
static const u8 OID_POLICY_CON[] = { 0x55,0x1D,0x24 };
static const u8 OID_INHIBIT[]    = { 0x55,0x1D,0x36 };
static const u8 OID_AIA[]        = { 0x2B,0x06,0x01,0x05,0x05,0x07,0x01,0x01 };
static const u8 OID_SCT[]        = { 0x2B,0x06,0x01,0x04,0x01,0xD6,0x79,0x02,0x04,0x02 };

#define OID(t, o) asn1_oid_is((t), (o), sizeof (o))

u32 pki_builtin_count(void) { return (u32)(sizeof AUTHORITIES / sizeof AUTHORITIES[0]); }
const pki_authority *pki_builtin(u32 i) { return &AUTHORITIES[i]; }

bool pki_key_parse(const u8 *spki, u32 len, pki_key *out)
{
    asn1_span s, in, alg;
    asn1_tlv t, a, oid, bits;
    memset(out, 0, sizeof *out);

    asn1_span_of(&s, spki, len);
    if (!asn1_expect(&s, ASN1_SEQUENCE, &t) || !asn1_done(&s)) return false;
    asn1_inside(&t, &in);
    if (!asn1_expect(&in, ASN1_SEQUENCE, &a)) return false;
    asn1_inside(&a, &alg);
    if (!asn1_expect(&alg, ASN1_OID, &oid)) return false;
    if (!asn1_expect(&in, ASN1_BITSTRING, &bits) || !asn1_done(&in)) return false;
    const u8 *kp;
    u32 klen;
    if (!asn1_bits(&bits, &kp, &klen)) return false;

    if (OID(&oid, OID_EC_KEY)) {
        asn1_tlv curve;
        if (!asn1_expect(&alg, ASN1_OID, &curve) || !OID(&curve, OID_P256)) return false;
        if (klen != 65 || kp[0] != 0x04) return false;
        memcpy(out->point, kp, 65);
        if (!p256_point_ok(out->point)) return false;
        out->kind = KEY_P256;
        return true;
    }
    if (OID(&oid, OID_RSA_KEY)) {
        asn1_span ks, seq;
        asn1_tlv kseq, ni, ei;
        asn1_span_of(&ks, kp, klen);
        if (!asn1_expect(&ks, ASN1_SEQUENCE, &kseq)) return false;
        asn1_inside(&kseq, &seq);
        if (!asn1_expect(&seq, ASN1_INTEGER, &ni) || !asn1_expect(&seq, ASN1_INTEGER, &ei)) return false;
        if (!asn1_uint(&ni, &out->n, &out->nlen) || !asn1_uint(&ei, &out->e, &out->elen)) return false;
        if (out->nlen < 256 || out->nlen > 512) return false;
        out->kind = KEY_RSA;
        return true;
    }
    return false;
}

static bool parse_extensions(x509_cert *c, const asn1_tlv *wrap)
{
    asn1_span outer, list, ext;
    asn1_tlv t, e, oid, val;
    asn1_inside(wrap, &outer);
    if (!asn1_expect(&outer, ASN1_SEQUENCE, &t) || !asn1_done(&outer)) return false;
    asn1_inside(&t, &list);

    while (!asn1_done(&list)) {
        if (!asn1_expect(&list, ASN1_SEQUENCE, &e)) return false;
        asn1_inside(&e, &ext);
        if (!asn1_expect(&ext, ASN1_OID, &oid)) return false;
        bool critical = false;
        u8 tag;
        if (asn1_peek(&ext, &tag) && tag == ASN1_BOOLEAN) {
            asn1_tlv b;
            asn1_next(&ext, &b);
            critical = b.len == 1 && b.p[0] != 0;
        }
        if (!asn1_expect(&ext, ASN1_OCTETSTRING, &val) || !asn1_done(&ext)) return false;

        asn1_span v;
        asn1_tlv in;
        asn1_span_of(&v, val.p, val.len);
        if (OID(&oid, OID_SAN)) {
            if (!asn1_expect(&v, ASN1_SEQUENCE, &in)) return false;
            c->san = in.p;
            c->sanlen = in.len;
        } else if (OID(&oid, OID_BASIC)) {
            if (!asn1_expect(&v, ASN1_SEQUENCE, &in)) return false;
            asn1_span bs;
            asn1_inside(&in, &bs);
            if (asn1_peek(&bs, &tag) && tag == ASN1_BOOLEAN) {
                asn1_tlv b;
                asn1_next(&bs, &b);
                c->is_ca = b.len == 1 && b.p[0] != 0;
            }
        } else if (OID(&oid, OID_KEY_USAGE)) {
            /* A bit string: the first byte counts unused bits, the next
             * holds the flags with digitalSignature at the top;
             * keyCertSign is bit five. */
            if (!asn1_expect(&v, ASN1_BITSTRING, &in) || in.len < 1) return false;
            c->has_key_usage = true;
            c->may_sign_certs = in.len >= 2 && (in.p[1] & 0x04);
        } else if (OID(&oid, OID_EKU)) {
            if (!asn1_expect(&v, ASN1_SEQUENCE, &in)) return false;
            c->has_eku = true;
            asn1_span es;
            asn1_tlv o;
            asn1_inside(&in, &es);
            while (asn1_next(&es, &o))
                if (OID(&o, OID_SERVER_AUTH) || OID(&o, OID_ANY_EKU)) c->eku_server = true;
        } else if (critical &&
                   !OID(&oid, OID_SKI) && !OID(&oid, OID_AKI) && !OID(&oid, OID_POLICIES) &&
                   !OID(&oid, OID_CRL_DP) && !OID(&oid, OID_POLICY_CON) && !OID(&oid, OID_INHIBIT) &&
                   !OID(&oid, OID_AIA) && !OID(&oid, OID_SCT)) {
            c->unknown_critical = true;
        }
    }
    return true;
}

bool x509_parse(const u8 *der, u32 len, x509_cert *c)
{
    asn1_span s, cert, tbs, seq;
    asn1_tlv t, u, alg_in, alg_out;
    memset(c, 0, sizeof *c);
    c->der = der;
    c->derlen = len;

    asn1_span_of(&s, der, len);
    if (!asn1_expect(&s, ASN1_SEQUENCE, &t) || !asn1_done(&s)) return false;
    asn1_inside(&t, &cert);

    if (!asn1_expect(&cert, ASN1_SEQUENCE, &t)) return false;
    c->tbs = t.head;
    c->tbslen = t.total;
    asn1_inside(&t, &tbs);

    u8 tag;
    if (asn1_peek(&tbs, &tag) && tag == ASN1_CONTEXT_C(0)) {
        if (!asn1_next(&tbs, &u)) return false;                 /* version */
    }
    if (!asn1_expect(&tbs, ASN1_INTEGER, &u)) return false;      /* serial */

    if (!asn1_expect(&tbs, ASN1_SEQUENCE, &t)) return false;     /* signature algorithm, inner */
    asn1_inside(&t, &seq);
    if (!asn1_expect(&seq, ASN1_OID, &alg_in)) return false;

    if (!asn1_expect(&tbs, ASN1_SEQUENCE, &t)) return false;     /* issuer */
    c->issuer = t.head;
    c->issuerlen = t.total;

    if (!asn1_expect(&tbs, ASN1_SEQUENCE, &t)) return false;     /* validity */
    asn1_inside(&t, &seq);
    if (!asn1_next(&seq, &u) || !asn1_time(&u, &c->not_before)) return false;
    if (!asn1_next(&seq, &u) || !asn1_time(&u, &c->not_after)) return false;

    if (!asn1_expect(&tbs, ASN1_SEQUENCE, &t)) return false;     /* subject */
    c->subject = t.head;
    c->subjectlen = t.total;

    if (!asn1_expect(&tbs, ASN1_SEQUENCE, &t)) return false;     /* subject public key info */
    c->spki = t.head;
    c->spkilen = t.total;
    if (!pki_key_parse(t.head, t.total, &c->key)) c->key.kind = KEY_NONE;

    while (asn1_peek(&tbs, &tag)) {
        if (!asn1_next(&tbs, &t)) return false;
        if (tag == ASN1_CONTEXT_C(3)) {
            if (!parse_extensions(c, &t)) return false;
        } else if (tag != ASN1_CONTEXT(1) && tag != ASN1_CONTEXT(2)) {
            return false;                                        /* unique ids are the only others allowed */
        }
    }

    if (!asn1_expect(&cert, ASN1_SEQUENCE, &t)) return false;    /* signature algorithm, outer */
    asn1_inside(&t, &seq);
    if (!asn1_expect(&seq, ASN1_OID, &alg_out)) return false;
    if (alg_out.len != alg_in.len || memcmp(alg_out.p, alg_in.p, alg_in.len) != 0) return false;
    if (OID(&alg_out, OID_ECDSA_SHA256))    c->sigalg = SIG_ECDSA_SHA256;
    else if (OID(&alg_out, OID_RSA_SHA256)) c->sigalg = SIG_RSA_SHA256;
    else                                     c->sigalg = SIG_NONE;

    if (!asn1_expect(&cert, ASN1_BITSTRING, &t)) return false;
    if (!asn1_bits(&t, &c->sig, &c->siglen)) return false;
    return asn1_done(&cert);
}

bool p256_verify_der(const u8 pub[65], const u8 hash[32], const u8 *sig, u32 siglen)
{
    asn1_span s, in;
    asn1_tlv t, ri, si;
    asn1_span_of(&s, sig, siglen);
    if (!asn1_expect(&s, ASN1_SEQUENCE, &t) || !asn1_done(&s)) return false;
    asn1_inside(&t, &in);
    if (!asn1_expect(&in, ASN1_INTEGER, &ri) || !asn1_expect(&in, ASN1_INTEGER, &si) || !asn1_done(&in))
        return false;
    const u8 *r, *sv;
    u32 rlen, slen;
    if (!asn1_uint(&ri, &r, &rlen) || !asn1_uint(&si, &sv, &slen)) return false;
    return p256_verify(pub, hash, r, rlen, sv, slen);
}

bool x509_check_signature(const x509_cert *c, const pki_key *k)
{
    u8 h[32];
    sha256(c->tbs, c->tbslen, h);
    if (c->sigalg == SIG_ECDSA_SHA256) {
        if (k->kind != KEY_P256) return false;
        return p256_verify_der(k->point, h, c->sig, c->siglen);
    }
    if (c->sigalg == SIG_RSA_SHA256) {
        if (k->kind != KEY_RSA) return false;
        return rsa_verify_pkcs1_sha256(k->n, k->nlen, k->e, k->elen, h, c->sig, c->siglen);
    }
    return false;
}

/* ------------------------------------------------------------------ */
/* Host names                                                          */
/* ------------------------------------------------------------------ */

static u8 lower(u8 c) { return c >= 'A' && c <= 'Z' ? (u8)(c + 32) : c; }

static bool name_eq(const u8 *a, u32 alen, const char *b, u32 blen)
{
    if (alen != blen) return false;
    for (u32 i = 0; i < alen; i++) if (lower(a[i]) != lower((u8)b[i])) return false;
    return true;
}

/* A dNSName against the host: exact, or a wildcard that stands for
 * exactly one label at the front (RFC 6125). */
static bool dns_match(const u8 *pat, u32 plen, const char *host, u32 hlen)
{
    if (plen >= 2 && pat[0] == '*' && pat[1] == '.') {
        u32 dot = 0;
        while (dot < hlen && host[dot] != '.') dot++;
        if (dot == 0 || dot >= hlen) return false;
        return name_eq(pat + 2, plen - 2, host + dot + 1, hlen - dot - 1);
    }
    return name_eq(pat, plen, host, hlen);
}

static bool as_ipv4(const char *host, u32 hlen, u8 ip[4])
{
    u32 v = 0, at = 0, digits = 0;
    for (u32 i = 0; i < hlen; i++) {
        char ch = host[i];
        if (ch == '.') {
            if (digits == 0 || at == 3) return false;
            ip[at++] = (u8)v; v = 0; digits = 0;
        } else if (ch >= '0' && ch <= '9') {
            v = v * 10 + (u32)(ch - '0');
            if (v > 255) return false;
            digits++;
        } else {
            return false;
        }
    }
    if (at != 3 || digits == 0) return false;
    ip[3] = (u8)v;
    return true;
}

bool x509_matches_host(const x509_cert *c, const char *host, u32 hlen)
{
    if (!c->san || hlen == 0) return false;
    u8 ip[4];
    bool is_ip = as_ipv4(host, hlen, ip);

    asn1_span s;
    asn1_tlv g;
    asn1_span_of(&s, c->san, c->sanlen);
    while (asn1_next(&s, &g)) {
        if (g.tag == ASN1_CONTEXT(2) && !is_ip) {                /* dNSName */
            if (dns_match(g.p, g.len, host, hlen)) return true;
        } else if (g.tag == ASN1_CONTEXT(7) && is_ip) {           /* iPAddress */
            if (g.len == 4 && memcmp(g.p, ip, 4) == 0) return true;
        }
    }
    return false;
}

/* ------------------------------------------------------------------ */
/* The walk                                                            */
/* ------------------------------------------------------------------ */

static bool key_fits(u8 sigalg, u8 kind)
{
    return (sigalg == SIG_ECDSA_SHA256 && kind == KEY_P256) ||
           (sigalg == SIG_RSA_SHA256 && kind == KEY_RSA);
}

static x509_status dates_ok(const x509_cert *c, i64 now)
{
    if (now < c->not_before) return X509_NOT_YET;
    if (now > c->not_after)  return X509_EXPIRED;
    return X509_VERIFIED;
}

x509_status x509_verify_chain(const u8 *const *ders, const u32 *lens, u32 count,
                              const char *host, u32 hlen, i64 now,
                              const pki_authority *extra, u32 nextra,
                              x509_cert *leaf, const char **by)
{
    static x509_cert certs[X509_MAX_CHAIN];       /* kept off the thread stack */
    bool ok[X509_MAX_CHAIN], used[X509_MAX_CHAIN];

    if (count == 0) return X509_UNREADABLE;
    if (count > X509_MAX_CHAIN) count = X509_MAX_CHAIN;
    for (u32 i = 0; i < count; i++) {
        ok[i] = x509_parse(ders[i], lens[i], &certs[i]);
        used[i] = false;
    }
    if (!ok[0]) return X509_UNREADABLE;

    const x509_cert *l = &certs[0];
    if (l->unknown_critical || l->key.kind == KEY_NONE || l->sigalg == SIG_NONE) return X509_UNSUPPORTED;
    x509_status d = dates_ok(l, now);
    if (d != X509_VERIFIED) return d;
    if (!x509_matches_host(l, host, hlen)) return X509_WRONG_HOST;
    if (l->has_eku && !l->eku_server) return X509_NOT_A_SERVER;

    u32 cur = 0;
    used[0] = true;
    u32 builtin = pki_builtin_count();
    for (u32 depth = 0; depth < X509_MAX_CHAIN; depth++) {
        const x509_cert *c = &certs[cur];
        if (c->sigalg == SIG_NONE) return X509_UNSUPPORTED;

        /* A trusted authority whose key fits the signature? */
        for (u32 a = 0; a < builtin + nextra; a++) {
            const pki_authority *au = a < builtin ? &AUTHORITIES[a] : &extra[a - builtin];
            pki_key k;
            if (!pki_key_parse(au->spki, au->len, &k)) continue;
            if (!key_fits(c->sigalg, k.kind)) continue;
            if (x509_check_signature(c, &k)) {
                if (leaf) *leaf = certs[0];
                if (by) *by = au->name;
                return X509_VERIFIED;
            }
        }

        /* Else the certificate in the chain named as the issuer. */
        u32 next = count;
        for (u32 j = 0; j < count; j++) {
            if (j == cur || !ok[j] || used[j]) continue;
            if (certs[j].subjectlen == c->issuerlen &&
                memcmp(certs[j].subject, c->issuer, c->issuerlen) == 0) { next = j; break; }
        }
        if (next == count) return X509_NO_AUTHORITY;

        const x509_cert *p = &certs[next];
        if (p->unknown_critical || p->key.kind == KEY_NONE) return X509_UNSUPPORTED;
        if (!p->is_ca || (p->has_key_usage && !p->may_sign_certs)) return X509_NOT_AN_AUTHORITY;
        d = dates_ok(p, now);
        if (d != X509_VERIFIED) return d;
        if (!key_fits(c->sigalg, p->key.kind)) return X509_UNSUPPORTED;
        if (!x509_check_signature(c, &p->key)) return X509_BAD_SIGNATURE;
        used[next] = true;
        cur = next;
    }
    return X509_NO_AUTHORITY;
}

const char *x509_status_text(x509_status s)
{
    switch (s) {
    case X509_VERIFIED:         return "verified";
    case X509_UNREADABLE:       return "the certificate could not be read";
    case X509_NO_AUTHORITY:     return "no trusted authority signs the chain";
    case X509_BAD_SIGNATURE:    return "a signature in the chain did not verify";
    case X509_EXPIRED:          return "a certificate in the chain has expired";
    case X509_NOT_YET:          return "a certificate in the chain is not yet valid";
    case X509_WRONG_HOST:       return "the certificate names no host that matches";
    case X509_UNSUPPORTED:      return "the chain uses an algorithm or extension not supported";
    case X509_NOT_AN_AUTHORITY: return "a certificate in the chain is not marked as an authority";
    case X509_NOT_A_SERVER:     return "the certificate is not issued for a server";
    }
    return "not verified";
}
