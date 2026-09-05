/*
 * pki.h -- the signatures under certificates, and the certificates themselves.
 * - ECDSA over P-256 and RSA (PKCS#1 v1.5 and PSS), both with SHA-256: what the chains of github.com and its release cdn use
 * - X.509: parsing, host names, dates, and the walk from a server's chain to a trusted authority
 * - an authority is a SubjectPublicKeyInfo; the built-in ones are the intermediates that sign the release host and cdn
 */
#ifndef EB_PKI_H
#define EB_PKI_H

#include <eb/types.h>

/* ECDSA P-256 with SHA-256. pub is the uncompressed point (04, x, y);
 * r and s are big-endian with any leading zeros. */
bool p256_verify(const u8 pub[65], const u8 hash[32],
                 const u8 *r, u32 rlen, const u8 *s, u32 slen);
bool p256_point_ok(const u8 pub[65]);
/* The same with the signature as DER (SEQUENCE of r and s), the form certificates and TLS carry. */
bool p256_verify_der(const u8 pub[65], const u8 hash[32], const u8 *sig, u32 siglen);

/* RSA with SHA-256 under a modulus n and exponent e (big-endian, leading
 * zeros allowed) of 2048 to 4096 bits. The signature must be as long as
 * the modulus. PSS with a 32-byte salt, as TLS 1.3 requires. */
bool rsa_verify_pkcs1_sha256(const u8 *n, u32 nlen, const u8 *e, u32 elen,
                             const u8 hash[32], const u8 *sig, u32 siglen);
bool rsa_verify_pss_sha256(const u8 *n, u32 nlen, const u8 *e, u32 elen,
                           const u8 hash[32], const u8 *sig, u32 siglen);

#define KEY_NONE 0
#define KEY_P256 1
#define KEY_RSA  2

#define SIG_NONE         0
#define SIG_ECDSA_SHA256 1
#define SIG_RSA_SHA256   2

typedef struct {
    u8        kind;
    u8        point[65];          /* KEY_P256 */
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
    u8        sigalg;                   /* SIG_* */
    const u8 *sig;     u32 siglen;
    const u8 *spki;    u32 spkilen;     /* the SubjectPublicKeyInfo, whole element */
    pki_key   key;                      /* kind KEY_NONE when the key is not supported */
    const u8 *san;     u32 sanlen;      /* the GeneralNames, or NULL */
    bool      is_ca;                    /* basicConstraints cA */
    bool      has_key_usage, may_sign_certs;
    bool      has_eku, eku_server;      /* extendedKeyUsage present; serverAuth or any in it */
    bool      unknown_critical;         /* a critical extension not understood here */
} x509_cert;

bool x509_parse(const u8 *der, u32 len, x509_cert *out);
bool x509_matches_host(const x509_cert *c, const char *host, u32 hlen);
bool x509_check_signature(const x509_cert *c, const pki_key *issuer);

/* A trusted authority: a name for the log and its SubjectPublicKeyInfo. */
typedef struct { const char *name; const u8 *spki; u32 len; } pki_authority;

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
    X509_NOT_A_SERVER       /* the leaf is not issued for server use */
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

/* Known-answer tests: RFC 6979 for the curve, fixed RSA vectors, dates. */
bool pki_selftest(void);

#endif /* EB_PKI_H */
