/*
 * pkihost.c -- the certificate checker run on the host, for tools/pkitest.sh.
 *   pkihost self                                             the known-answer tests
 *   pkihost chain <host> <now|unix seconds> [-a auth.der]... <leaf.der> [more.der]...
 *                                                            the walk; prints the outcome, exit 0 when verified
 *   pkihost sig <spki.der> <0403|0804> <content> <sig>       a CertificateVerify-style signature over the content
 * The same kernel files as on the machine, against libc only.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <eb/pki.h>
#include <eb/crypto.h>

void kprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

static unsigned char *slurp(const char *path, unsigned *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); exit(2); }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *b = malloc((size_t)n + 1);
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { fprintf(stderr, "short read %s\n", path); exit(2); }
    fclose(f);
    *len = (unsigned)n;
    return b;
}

static int do_chain(int argc, char **argv)
{
    if (argc < 4) return 2;
    const char *host = argv[2];
    long long now = strcmp(argv[3], "now") == 0 ? (long long)time(NULL) : atoll(argv[3]);

    pki_authority extra[8];
    unsigned nextra = 0;
    const u8 *ders[X509_MAX_CHAIN];
    u32 lens[X509_MAX_CHAIN];
    unsigned count = 0;

    for (int i = 4; i < argc; i++) {
        if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) {
            unsigned n;
            unsigned char *d = slurp(argv[++i], &n);
            x509_cert c;
            if (!x509_parse(d, n, &c)) { printf("authority %s: unreadable\n", argv[i]); return 2; }
            extra[nextra].name = argv[i];
            extra[nextra].spki = c.spki;
            extra[nextra].len = c.spkilen;
            nextra++;
        } else {
            if (count >= X509_MAX_CHAIN) break;
            unsigned n;
            ders[count] = slurp(argv[i], &n);
            lens[count] = n;
            count++;
        }
    }

    x509_cert leaf;
    const char *by = "";
    x509_status s = x509_verify_chain(ders, lens, count, host, (u32)strlen(host),
                                      now, extra, nextra, &leaf, &by);
    if (s == X509_VERIFIED) { printf("verified by %s\n", by); return 0; }
    printf("%s\n", x509_status_text(s));
    return 1;
}

static int do_sig(int argc, char **argv)
{
    if (argc != 6) return 2;
    unsigned kl, cl, sl;
    unsigned char *spki = slurp(argv[2], &kl);
    const char *scheme = argv[3];
    unsigned char *content = slurp(argv[4], &cl);
    unsigned char *sig = slurp(argv[5], &sl);

    pki_key k;
    if (!pki_key_parse(spki, kl, &k)) { printf("key unreadable\n"); return 2; }
    u8 h[32];
    sha256(content, cl, h);
    bool ok = false;
    if (strcmp(scheme, "0403") == 0 && k.kind == KEY_P256)
        ok = p256_verify_der(k.point, h, sig, sl);
    else if (strcmp(scheme, "0804") == 0 && k.kind == KEY_RSA)
        ok = rsa_verify_pss_sha256(k.n, k.nlen, k.e, k.elen, h, sig, sl);
    else { printf("scheme and key do not fit\n"); return 1; }
    printf("%s\n", ok ? "signature verified" : "signature did not verify");
    return ok ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "self") == 0) {
        bool ok = pki_selftest();
        printf("%s\n", ok ? "known answers verified" : "known answers FAILED");
        return ok ? 0 : 1;
    }
    if (argc >= 2 && strcmp(argv[1], "chain") == 0) return do_chain(argc, argv);
    if (argc >= 2 && strcmp(argv[1], "sig") == 0) return do_sig(argc, argv);
    fprintf(stderr, "usage: pkihost self | chain <host> <now> [-a auth.der]... leaf.der [more.der]... | sig <spki.der> <0403|0804> <content> <sig>\n");
    return 2;
}
