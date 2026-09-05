/*
 * pki_fuzz.c -- libFuzzer entry for the certificate checker: bytes as a certificate, a chain, a key, a signature.
 * - the first byte picks the target; the rest is the input
 * - built and run by tools/fuzz/run.sh alongside the language tools
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <eb/pki.h>
#include <eb/asn1.h>
#include <eb/crypto.h>

void kprintf(const char *fmt, ...) { (void)fmt; }

static const char *hosts[] = { "github.com", "*.x", "10.0.2.100", "a.b.c", "" };

static const uint8_t P256_PUB[65] = {
    0x04,
    0x60,0xFE,0xD4,0xBA,0x25,0x5A,0x9D,0x31,0xC9,0x61,0xEB,0x74,0xC6,0x35,0x6D,0x68,
    0xC0,0x49,0xB8,0x92,0x3B,0x61,0xFA,0x6C,0xE6,0x69,0x62,0x2E,0x60,0xF2,0x9F,0xB6,
    0x79,0x03,0xFE,0x10,0x08,0xB8,0xBC,0x99,0xA4,0x1A,0xE9,0xE9,0x56,0x28,0xBC,0x64,
    0xF2,0xF1,0xB2,0x0C,0x2D,0x7E,0x9F,0x51,0x77,0xA3,0xC2,0x94,0xD4,0x46,0x22,0x99 };
static const uint8_t P384_PUB[97] = {
    0x04,
    0xEC,0x3A,0x4E,0x41,0x5B,0x4E,0x19,0xA4,0x56,0x86,0x18,0x02,0x9F,0x42,0x7F,0xA5,
    0xDA,0x9A,0x8B,0xC4,0xAE,0x92,0xE0,0x2E,0x06,0xAA,0xE5,0x28,0x6B,0x30,0x0C,0x64,
    0xDE,0xF8,0xF0,0xEA,0x90,0x55,0x86,0x60,0x64,0xA2,0x54,0x51,0x54,0x80,0xBC,0x13,
    0x80,0x15,0xD9,0xB7,0x2D,0x7D,0x57,0x24,0x4E,0xA8,0xEF,0x9A,0xC0,0xC6,0x21,0x89,
    0x67,0x08,0xA5,0x93,0x67,0xF9,0xDF,0xB9,0xF5,0x4C,0xA8,0x4B,0x3F,0x1C,0x9D,0xB1,
    0x28,0x8B,0x23,0x1C,0x3A,0xE0,0xD4,0xFE,0x73,0x44,0xFD,0x25,0x33,0x26,0x47,0x20 };

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1 || size > 65536) return 0;
    uint8_t pick = data[0];
    const uint8_t *in = data + 1;
    uint32_t len = (uint32_t)(size - 1);

    switch (pick % 7) {
    case 0: {                                        /* one certificate */
        x509_cert c;
        if (x509_parse(in, len, &c)) {
            const char *h = hosts[len % 5];
            x509_matches_host(&c, h, (uint32_t)strlen(h));
            pki_key k;
            if (c.key.kind != KEY_NONE) x509_check_signature(&c, &c.key);
            if (pki_key_parse(c.spki, c.spkilen, &k)) x509_check_signature(&c, &k);
        }
        break;
    }
    case 1: {                                        /* a chain: 2-byte lengths, then certificates */
        const uint8_t *ders[X509_MAX_CHAIN];
        uint32_t lens[X509_MAX_CHAIN];
        uint32_t n = 0, at = 0;
        while (at + 2 <= len && n < X509_MAX_CHAIN) {
            uint32_t l = ((uint32_t)in[at] << 8) | in[at + 1];
            at += 2;
            if (l == 0 || at + l > len) break;
            ders[n] = in + at; lens[n] = l; n++; at += l;
        }
        if (n) {
            x509_cert leaf;
            const char *by;
            const char *h = hosts[len % 5];
            pki_authority extra = { "fuzz", ders[n - 1], lens[n - 1] };
            x509_verify_chain(ders, lens, n, h, (uint32_t)strlen(h), 1788609600LL, &extra, 1, &leaf, &by);
        }
        break;
    }
    case 2: {                                        /* a key */
        pki_key k;
        pki_key_parse(in, len, &k);
        break;
    }
    case 3: {                                        /* an ecdsa signature under a fixed p-256 point, bytes as der */
        uint8_t h[32];
        sha256(in, len, h);
        ec_verify_der(EC_P256, P256_PUB, 65, h, 32, in, len);
        if (len >= 65) ec_point_ok(EC_P256, in, 65);
        break;
    }
    case 4: {                                        /* rsa: the bytes as modulus (first half) and signature (second half) */
        if (len < 512) break;
        uint32_t half = len / 2;
        if (half > 512) half = 512;
        static const uint8_t e[3] = { 1, 0, 1 };
        uint8_t h[64];
        sha384(in, len, h);
        rsa_verify_pkcs1(in, half, e, 3, HASH_SHA384, h, in + half, half);
        rsa_verify_pss(in, half, e, 3, HASH_SHA384, h, in + half, half);
        break;
    }
    case 5: {                                        /* raw der walking */
        asn1_span s;
        asn1_tlv t;
        asn1_span_of(&s, in, len);
        for (int guard = 0; guard < 64 && asn1_next(&s, &t); guard++) {
            const uint8_t *p; uint32_t l; int64_t v;
            asn1_uint(&t, &p, &l);
            asn1_bits(&t, &p, &l);
            asn1_time(&t, &v);
        }
        break;
    }
    case 6: {                                        /* an ecdsa signature under a fixed p-384 point */
        uint8_t h[48];
        sha384(in, len, h);
        ec_verify_der(EC_P384, P384_PUB, 97, h, 48, in, len);
        if (len >= 97) ec_point_ok(EC_P384, in, 97);
        break;
    }
    }
    return 0;
}
