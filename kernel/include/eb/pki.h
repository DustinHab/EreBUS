/*
 * pki.h -- the signatures under certificates, and the certificates themselves.
 * - ECDSA over P-256 and P-384, RSA (PKCS#1 v1.5 and PSS) with SHA-256, SHA-384 or SHA-512
 * - X.509: parsing, host names, dates, and the walk from a server's chain to a trusted authority
 * - an authority is a certificate (the built-in roots) or a bare SubjectPublicKeyInfo (from the settings)
 */
#ifndef EB_PKI_H
#define EB_PKI_H

#include <eb/types.h>

#define HASH_NONE   0
#define HASH_SHA256 1
#define HASH_SHA384 2
#define HASH_SHA512 3

u32  pki_hash_len(u8 kind);
void pki_hash(u8 kind, const void *data, u64 len, u8 *out);   /* out holds pki_hash_len bytes */

/* The curves. pub is the uncompressed point: 04, x, y -- 65 bytes for
 * P-256, 97 for P-384. The hash is taken by its leftmost bytes when it
 * is longer than the curve; r and s are big-endian with any leading zeros. */
#define EC_P256 0
#define EC_P384 1
bool ec_point_ok(u32 curve, const u8 *pub, u32 publen);
bool ec_verify(u32 curve, const u8 *pub, u32 publen, const u8 *hash, u32 hlen,
               const u8 *r, u32 rlen, const u8 *s, u32 slen);
/* The same with the signature as DER (SEQUENCE of r and s), the form certificates and TLS carry. */
bool ec_verify_der(u32 curve, const u8 *pub, u32 publen, const u8 *hash, u32 hlen,
                   const u8 *sig, u32 siglen);

/* RSA under a modulus n and exponent e (big-endian, leading zeros
 * allowed) of 2048 to 4096 bits; the signature must be as long as the
 * modulus. PSS with a salt as long as the hash, as TLS 1.3 requires. */
bool rsa_verify_pkcs1(const u8 *n, u32 nlen, const u8 *e, u32 elen,
                      u8 hash_kind, const u8 *hash, const u8 *sig, u32 siglen);
bool rsa_verify_pss(const u8 *n, u32 nlen, const u8 *e, u32 elen,
                    u8 hash_kind, const u8 *hash, const u8 *sig, u32 siglen);

#define KEY_NONE 0
#define KEY_RSA  1
#define KEY_P256 2
#define KEY_P384 3

#define SIG_NONE      0
#define SIG_ECDSA     1
#define SIG_RSA_PKCS1 2

typedef struct {
    u8        kind;
    u8        point[97];          /* KEY_P256, KEY_P384: the uncompressed point */
    u32       pointlen;
    const u8 *n; u32 nlen;        /* KEY_RSA: modulus and exponent, into the source bytes */
    const u8 *e; u32 elen;
} pki_key;

/* A public key out of a SubjectPublicKeyInfo. False for a kind not supported. */
bool pki_key_parse(const u8 *spki, u32 len, pki_key *out);

typedef struct {
    const u8 *der;     u32 derlen;
    const u8 *tbs;     u32 tbslen;      /* what the signature covers */
    const u8 *issuer;  u32 issuerlen;   /* the Name, whole element */
    const u8 *subject; u32 subjectlen;
    i64       not_before, not_after;    /* seconds since 1970 */
    u8        sig, hash;                /* SIG_* and HASH_* of the signature */
    const u8 *sigbytes; u32 siglen;
    const u8 *spki;    u32 spkilen;     /* the SubjectPublicKeyInfo, whole element */
    pki_key   key;                      /* kind KEY_NONE when the key is not supported */
    const u8 *san;     u32 sanlen;      /* the GeneralNames, or NULL */
    const u8 *nc;      u32 nclen;       /* nameConstraints content, or NULL */
    bool      is_ca;                    /* basicConstraints cA */
    bool      has_key_usage, may_sign_certs;
    bool      has_eku, eku_server;      /* extendedKeyUsage present; serverAuth or any in it */
    bool      unknown_critical;         /* a critical extension not understood here */
} x509_cert;

bool x509_parse(const u8 *der, u32 len, x509_cert *out);
bool x509_matches_host(const x509_cert *c, const char *host, u32 hlen);
bool x509_check_signature(const x509_cert *c, const pki_key *issuer);

/* A trusted authority: a name for the log and its bytes -- a whole
 * certificate (whose subject then narrows where it is tried) or a bare
 * SubjectPublicKeyInfo. */
typedef struct { const char *name; const u8 *der; u32 len; } pki_authority;

u32                  pki_builtin_count(void);
const pki_authority *pki_builtin(u32 i);

typedef enum {
    X509_VERIFIED = 0,
    X509_UNREADABLE,        /* the first certificate could not be parsed */
    X509_NO_AUTHORITY,      /* the chain reaches no trusted authority */
    X509_BAD_SIGNATURE,     /* a signature in the chain did not verify */
    X509_EXPIRED,
    X509_NOT_YET,
    X509_WRONG_HOST,        /* no name in the certificate matches the host */
    X509_UNSUPPORTED,       /* a key, signature or critical extension not supported */
    X509_NOT_AN_AUTHORITY,  /* a certificate in the middle is not marked as an authority */
    X509_NOT_A_SERVER,      /* the leaf is not issued for server use */
    X509_NAME_NOT_PERMITTED /* a name in the leaf lies outside an authority's name constraints */
} x509_status;

#define X509_MAX_CHAIN 8

/* Walks from the server's first certificate to a trusted authority:
 * the built-in ones and the extra ones given. now is seconds since 1970.
 * On success leaf holds the parsed first certificate (pointing into
 * ders[0]) and by names the authority. */
x509_status x509_verify_chain(const u8 *const *ders, const u32 *lens, u32 count,
                              const char *host, u32 hlen, i64 now,
                              const pki_authority *extra, u32 nextra,
                              x509_cert *leaf, const char **by);
const char *x509_status_text(x509_status s);

/* Known-answer tests: RFC 6979 for both curves, fixed RSA vectors, the hashes, dates. */
bool pki_selftest(void);

#endif /* EB_PKI_H */
