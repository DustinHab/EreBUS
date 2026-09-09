#!/bin/sh
# pkitest.sh -- the certificate checker on the host: openssl-made chains, live chains of the web, CertificateVerify signatures.
# - builds build/pkihost from the kernel's own pki files (tools/pkihost.c)
# - test authorities: an EC root (P-256) with an EC and an RSA intermediate, a P-384 root with an RSA-4096 intermediate
#   signed with SHA-384; leaves good, expired, not yet valid, for the wrong host, wildcard, by address, signed by a
#   non-authority, signed by an unrelated authority, tampered
# - the fixtures in tools/pki/fixtures are chains seen from github.com, its release cdn and a handful of other hosts,
#   checked against the built-in roots at the date they were taken
# - CertificateVerify signatures made with openssl over the TLS 1.3 content: ecdsa (0403, 0503) and rsa-pss (0804, 0805, 0806)
cd "$(dirname "$0")/.."
ROOT=$(pwd)
BUILD=${BUILD:-build}
case "$BUILD" in /*) W=$BUILD/pki ;; *) W=$ROOT/$BUILD/pki ;; esac
rm -rf $W; mkdir -p $W
H=$W/pkihost

clang -O1 -g -std=c11 -Wall -Wno-incompatible-library-redeclaration -Ikernel/include -o $H tools/pkihost.c \
    kernel/net/asn1.c kernel/net/bn.c kernel/net/ec.c kernel/net/rsa.c kernel/net/x509.c \
    kernel/net/sha256.c kernel/net/sha512.c kernel/net/pki_selftest.c || { echo "FAILED: pkihost did not build"; exit 1; }

ok=1
fail() { echo "FAILED: $1"; ok=0; }

# expect <verified|words of the expected outcome> <what> <pkihost chain arguments...>
expect() {
    want=$1; what=$2; shift 2
    out=$($H chain "$@" 2>&1); rc=$?
    if [ "$want" = verified ]; then
        [ $rc = 0 ] && echo "ok   $what: $out" || fail "$what: $out"
    else
        if [ $rc != 0 ] && echo "$out" | grep -q "$want"; then echo "ok   $what: $out"
        else fail "$what: expected '$want', got '$out'"; fi
    fi
}

$H self && echo "ok   known answers" || fail "known answers"

# --- test authorities and their chains ---
cd $W
openssl ecparam -name prime256v1 -genkey -noout -out root.key 2>/dev/null
openssl req -x509 -new -key root.key -sha256 -days 3650 -subj "/CN=Test Root" -out root.pem \
    -addext "basicConstraints=critical,CA:TRUE" -addext "keyUsage=critical,keyCertSign,cRLSign" 2>/dev/null
openssl ecparam -name prime256v1 -genkey -noout -out other.key 2>/dev/null
openssl req -x509 -new -key other.key -sha256 -days 3650 -subj "/CN=Other Root" -out other.pem \
    -addext "basicConstraints=critical,CA:TRUE" 2>/dev/null
openssl ecparam -name secp384r1 -genkey -noout -out root384.key 2>/dev/null
openssl req -x509 -new -key root384.key -sha384 -days 3650 -subj "/CN=Test Root 384" -out root384.pem \
    -addext "basicConstraints=critical,CA:TRUE" -addext "keyUsage=critical,keyCertSign,cRLSign" 2>/dev/null

printf 'basicConstraints=critical,CA:TRUE\nkeyUsage=critical,keyCertSign\n' > ca.ext
mkca() {   # mkca <name> <ec256|ec384|rsa2048|rsa4096> <issuer> <sha>
    case "$2" in
        ec256) openssl ecparam -name prime256v1 -genkey -noout -out $1.key 2>/dev/null ;;
        ec384) openssl ecparam -name secp384r1 -genkey -noout -out $1.key 2>/dev/null ;;
        rsa2048) openssl genrsa -out $1.key 2048 2>/dev/null ;;
        rsa4096) openssl genrsa -out $1.key 4096 2>/dev/null ;;
    esac
    openssl req -new -key $1.key -subj "/CN=Test $1" -out $1.csr 2>/dev/null
    openssl x509 -req -in $1.csr -CA $3.pem -CAkey $3.key -CAcreateserial -days 1000 -$4 -extfile ca.ext -out $1.pem 2>/dev/null
}
mkca iec ec256 root sha256
mkca irsa rsa2048 root sha256
mkca irsa4096 rsa4096 root384 sha384

printf 'subjectAltName=DNS:example.test,DNS:*.wild.test,IP:10.0.2.100\nbasicConstraints=CA:FALSE\nkeyUsage=digitalSignature\nextendedKeyUsage=serverAuth\n' > leaf.ext
mkleaf() {   # mkleaf <name> <ec256|ec384|rsa2048|rsa3072> <issuer> <sha> [ext file]
    case "$2" in
        ec256) openssl ecparam -name prime256v1 -genkey -noout -out $1.key 2>/dev/null ;;
        ec384) openssl ecparam -name secp384r1 -genkey -noout -out $1.key 2>/dev/null ;;
        rsa2048) openssl genrsa -out $1.key 2048 2>/dev/null ;;
        rsa3072) openssl genrsa -out $1.key 3072 2>/dev/null ;;
    esac
    openssl req -new -key $1.key -subj "/CN=$1" -out $1.csr 2>/dev/null
    openssl x509 -req -in $1.csr -CA $3.pem -CAkey $3.key -CAcreateserial -days 30 -$4 -extfile ${5:-leaf.ext} -out $1.pem 2>/dev/null
    openssl x509 -in $1.pem -outform DER -out $1.der 2>/dev/null
}
for c in root other root384 iec irsa irsa4096; do openssl x509 -in $c.pem -outform DER -out $c.der 2>/dev/null; done
mkleaf leafec ec256 iec sha256
mkleaf leafrsa rsa2048 irsa sha256
mkleaf leafbyroot ec256 root sha256
mkleaf leaf384 ec384 irsa4096 sha384
mkleaf leafrsa3072 rsa3072 iec sha512
mkleaf leafec512 ec256 irsa sha512
mkleaf fakeca ec256 iec sha256                    # a leaf that then signs another: not an authority
mkleaf leafbyfake ec256 fakeca sha256
mkleaf leafother ec256 other sha256
cp leafec.der tampered.der
printf '\377' | dd of=tampered.der bs=1 seek=$(( $(wc -c < leafec.der) - 1 )) conv=notrunc 2>/dev/null

# name-constrained intermediates: one permits example.test, one excludes
# evil.test. Signing an intermediate needs its own openssl run rather than
# mkca, because the constraint extension differs per CA.
mknc() {   # mknc <name> <permitted|excluded> <dns>
    printf 'basicConstraints=critical,CA:TRUE\nkeyUsage=critical,keyCertSign\nnameConstraints=critical,%s;DNS:%s\n' "$2" "$3" > $1.ext
    openssl ecparam -name prime256v1 -genkey -noout -out $1.key 2>/dev/null
    openssl req -new -key $1.key -subj "/CN=Test $1" -out $1.csr 2>/dev/null
    openssl x509 -req -in $1.csr -CA root.pem -CAkey root.key -CAcreateserial -days 1000 -sha256 -extfile $1.ext -out $1.pem 2>/dev/null
    openssl x509 -in $1.pem -outform DER -out $1.der 2>/dev/null
}
mknc ncperm permitted example.test
mknc ncexcl excluded evil.test
# leaves with a single SAN, so only the tested name is in play
printf 'subjectAltName=DNS:example.test\nbasicConstraints=CA:FALSE\nkeyUsage=digitalSignature\nextendedKeyUsage=serverAuth\n' > ncin.ext
printf 'subjectAltName=DNS:evil.test\nbasicConstraints=CA:FALSE\nkeyUsage=digitalSignature\nextendedKeyUsage=serverAuth\n' > ncbad.ext
mkleaf leafncin  ec256 ncperm sha256 ncin.ext
mkleaf leafncout ec256 ncperm sha256 ncbad.ext
mkleaf leafncx   ec256 ncexcl sha256 ncbad.ext

NOW=$(date +%s)
cd $ROOT

expect verified "ec leaf, ec intermediate, root authority"      example.test $NOW -a $W/root.der $W/leafec.der $W/iec.der
expect verified "rsa leaf, rsa intermediate, root authority"    example.test $NOW -a $W/root.der $W/leafrsa.der $W/irsa.der
expect verified "p-384 leaf under rsa-4096, sha-384 all the way, p-384 root" example.test $NOW -a $W/root384.der $W/leaf384.der $W/irsa4096.der
expect verified "rsa-3072 leaf signed with sha-512 by an ec key"  example.test $NOW -a $W/root.der $W/leafrsa3072.der $W/iec.der
expect verified "ec leaf signed with sha-512 by an rsa key"     example.test $NOW -a $W/root.der $W/leafec512.der $W/irsa.der
expect verified "chain sent out of order"                       example.test $NOW -a $W/root.der $W/leafec.der $W/irsa.der $W/iec.der
expect verified "leaf signed by the authority itself"           example.test $NOW -a $W/root.der $W/leafbyroot.der
expect verified "intermediate as the authority"                 example.test $NOW -a $W/iec.der $W/leafec.der
expect verified "wildcard, one label"                           a.wild.test $NOW -a $W/root.der $W/leafec.der $W/iec.der
expect verified "name case does not matter"                     EXAMPLE.Test $NOW -a $W/root.der $W/leafec.der $W/iec.der
expect verified "by address"                                    10.0.2.100 $NOW -a $W/root.der $W/leafec.der $W/iec.der
expect "no trusted authority" "intermediate missing"            example.test $NOW -a $W/root.der $W/leafec.der
expect "no trusted authority" "unrelated authority"             example.test $NOW -a $W/other.der $W/leafec.der $W/iec.der
expect "no trusted authority" "no authority at all"             example.test $NOW $W/leafec.der $W/iec.der
expect "no trusted authority" "leaf of another authority"       example.test $NOW -a $W/root.der $W/leafother.der $W/other.der
expect "no trusted authority" "the p-384 root does not vouch for the p-256 chain" example.test $NOW -a $W/root384.der $W/leafec.der $W/iec.der
expect "no host that matches" "wrong host"                      wrong.test $NOW -a $W/root.der $W/leafec.der $W/iec.der
expect "no host that matches" "wildcard does not span two labels" b.a.wild.test $NOW -a $W/root.der $W/leafec.der $W/iec.der
expect "no host that matches" "wildcard needs a label"          wild.test $NOW -a $W/root.der $W/leafec.der $W/iec.der
expect "no host that matches" "wrong address"                   10.0.2.101 $NOW -a $W/root.der $W/leafec.der $W/iec.der
expect "has expired"    "expired"                               example.test $(( NOW + 40 * 86400 )) -a $W/root.der $W/leafec.der $W/iec.der
expect "not yet valid"  "not yet valid"                         example.test $(( NOW - 2 * 86400 )) -a $W/root.der $W/leafec.der $W/iec.der
expect "not marked as an authority" "signed by a leaf"          example.test $NOW -a $W/root.der $W/leafbyfake.der $W/fakeca.der $W/iec.der
expect "did not verify" "tampered signature"                    example.test $NOW -a $W/root.der $W/tampered.der $W/iec.der
expect "could not be read" "not a certificate"                  example.test $NOW -a $W/root.der $W/leaf.ext
expect verified "leaf within a permitted dNSName constraint"    example.test $NOW -a $W/root.der $W/leafncin.der $W/ncperm.der
expect "outside an authority's name constraints" "leaf outside a permitted dNSName constraint" evil.test $NOW -a $W/root.der $W/leafncout.der $W/ncperm.der
expect "outside an authority's name constraints" "leaf inside an excluded dNSName constraint"  evil.test $NOW -a $W/root.der $W/leafncx.der $W/ncexcl.der

# --- the live chains against the built-in authorities, at the dates they were taken ---
F=tools/pki/fixtures
THEN=1788609600     # 2026-09-05 12:00 utc: github.com and the cdn
LATER=1788700000    # 2026-09-06 13:06 utc: the others
expect verified "github.com leaf under Sectigo E36"             github.com $THEN $F/github-leaf.der $F/github-e36.der $F/github-e46.der
expect verified "github.com leaf alone"                         github.com $THEN $F/github-leaf.der
expect verified "www.github.com"                                www.github.com $THEN $F/github-leaf.der
expect verified "github.com up to the USERTrust ECC root, past E36" github.com $THEN $F/github-leaf.der $F/github-e46.der
expect verified "release cdn leaf under Let's Encrypt YR1"      objects.githubusercontent.com $THEN $F/cdn-leaf.der $F/cdn-yr1.der $F/cdn-rootyr.der
expect verified "release-assets host on the same leaf"          release-assets.githubusercontent.com $THEN $F/cdn-leaf.der $F/cdn-yr1.der
expect verified "the cdn up to ISRG Root X1, past YR1"          objects.githubusercontent.com $THEN $F/cdn-leaf.der $F/cdn-rootyr.der
expect "no host that matches" "the cdn leaf for another host"   evil.example $THEN $F/cdn-leaf.der $F/cdn-yr1.der
expect "has expired"    "the github leaf a year on"             github.com $(( THEN + 365 * 86400 )) $F/github-leaf.der $F/github-e36.der
expect verified "wikipedia: p-256 leaf, sha-384, YE2, Root YE, ISRG Root X2" www.wikipedia.org $LATER $F/www.wikipedia.org-1.der $F/www.wikipedia.org-2.der $F/www.wikipedia.org-3.der $F/www.wikipedia.org-4.der
expect verified "google: WE2 under GTS Root R4"                 www.google.com $LATER $F/www.google.com-1.der $F/www.google.com-2.der $F/www.google.com-3.der
expect verified "amazon: rsa under Amazon Root CA 1"            www.amazon.com $LATER $F/www.amazon.com-1.der $F/www.amazon.com-2.der $F/www.amazon.com-3.der
expect verified "microsoft: rsa-4096 with sha-384, DigiCert Global Root G2" www.microsoft.com $LATER $F/www.microsoft.com-1.der $F/www.microsoft.com-2.der $F/www.microsoft.com-3.der
expect verified "bund.de: rsa-4096 leaf, rsa-3072 GEANT, HARICA root" www.bund.de $LATER $F/www.bund.de-1.der $F/www.bund.de-2.der $F/www.bund.de-3.der
expect verified "cloudflare: ISRG Root X2 by way of Root YE"    www.cloudflare.com $LATER $F/www.cloudflare.com-1.der $F/www.cloudflare.com-2.der $F/www.cloudflare.com-3.der $F/www.cloudflare.com-4.der
expect verified "heise: YR2"                                    www.heise.de $LATER $F/www.heise.de-1.der $F/www.heise.de-2.der $F/www.heise.de-3.der
expect verified "kernel.org: GlobalSign Atlas under Root R3"    www.kernel.org $LATER $F/www.kernel.org-1.der $F/www.kernel.org-2.der
expect verified "example.com: SSL.com ECC root by way of Cloudflare" example.com $LATER $F/example.com-1.der $F/example.com-2.der $F/example.com-3.der $F/example.com-4.der
expect "no host that matches" "google's leaf for another host"  www.gmail.com $LATER $F/www.google.com-1.der $F/www.google.com-2.der

# --- CertificateVerify signatures over the TLS 1.3 content ---
cd $W
{ printf '%64s' ''; printf 'TLS 1.3, server CertificateVerify'; printf '\000'; head -c 32 /dev/urandom; } > content
cp content content2; printf 'x' | dd of=content2 bs=1 seek=100 conv=notrunc 2>/dev/null
for k in leafec leafrsa leaf384; do openssl pkey -in $k.key -pubout -outform DER -out $k.spki 2>/dev/null; done
openssl pkeyutl -sign -inkey leafec.key -rawin -digest sha256 -in content -out cv.ec 2>/dev/null
openssl pkeyutl -sign -inkey leaf384.key -rawin -digest sha384 -in content -out cv.ec384 2>/dev/null
openssl pkeyutl -sign -inkey leafrsa.key -rawin -digest sha256 -pkeyopt rsa_padding_mode:pss -pkeyopt rsa_pss_saltlen:32 -in content -out cv.pss 2>/dev/null
openssl pkeyutl -sign -inkey leafrsa.key -rawin -digest sha384 -pkeyopt rsa_padding_mode:pss -pkeyopt rsa_pss_saltlen:48 -in content -out cv.pss384 2>/dev/null
openssl pkeyutl -sign -inkey leafrsa.key -rawin -digest sha512 -pkeyopt rsa_padding_mode:pss -pkeyopt rsa_pss_saltlen:64 -in content -out cv.pss512 2>/dev/null
cd $ROOT
sig() {   # sig <expect 0|1> <what> <args>
    want=$1; what=$2; shift 2
    out=$($H sig "$@" 2>&1); rc=$?
    [ $rc = $want ] && echo "ok   $what: $out" || fail "$what: $out"
}
sig 0 "certificate verify, ecdsa p-256"          $W/leafec.spki 0403 $W/content $W/cv.ec
sig 1 "certificate verify, ecdsa p-256, content changed" $W/leafec.spki 0403 $W/content2 $W/cv.ec
sig 0 "certificate verify, ecdsa p-384"          $W/leaf384.spki 0503 $W/content $W/cv.ec384
sig 1 "certificate verify, ecdsa p-384, content changed" $W/leaf384.spki 0503 $W/content2 $W/cv.ec384
sig 1 "p-384 key under the p-256 scheme"         $W/leaf384.spki 0403 $W/content $W/cv.ec384
sig 0 "certificate verify, rsa-pss sha-256"      $W/leafrsa.spki 0804 $W/content $W/cv.pss
sig 0 "certificate verify, rsa-pss sha-384"      $W/leafrsa.spki 0805 $W/content $W/cv.pss384
sig 0 "certificate verify, rsa-pss sha-512"      $W/leafrsa.spki 0806 $W/content $W/cv.pss512
sig 1 "certificate verify, rsa-pss, content changed" $W/leafrsa.spki 0804 $W/content2 $W/cv.pss
sig 1 "rsa-pss sha-384 signature under the sha-256 scheme" $W/leafrsa.spki 0804 $W/content $W/cv.pss384
sig 1 "rsa-pss signature under the ec scheme"    $W/leafrsa.spki 0403 $W/content $W/cv.pss

# --- many fresh keys and signatures, so that no single curve point is the only one ever checked ---
cd $W
bad=0
n=0
while [ $n -lt 25 ]; do
    openssl ecparam -name prime256v1 -genkey -noout -out r.key 2>/dev/null
    openssl pkey -in r.key -pubout -outform DER -out r.spki 2>/dev/null
    head -c $((n * 7 + 1)) /dev/urandom > r.msg
    openssl pkeyutl -sign -inkey r.key -rawin -digest sha256 -in r.msg -out r.sig 2>/dev/null
    $H sig r.spki 0403 r.msg r.sig >/dev/null 2>&1 || bad=$((bad + 1))
    n=$((n + 1))
done
while [ $n -lt 35 ]; do
    openssl ecparam -name secp384r1 -genkey -noout -out r.key 2>/dev/null
    openssl pkey -in r.key -pubout -outform DER -out r.spki 2>/dev/null
    head -c $((n * 7 + 1)) /dev/urandom > r.msg
    openssl pkeyutl -sign -inkey r.key -rawin -digest sha384 -in r.msg -out r.sig 2>/dev/null
    $H sig r.spki 0503 r.msg r.sig >/dev/null 2>&1 || bad=$((bad + 1))
    n=$((n + 1))
done
cd $ROOT
[ $bad = 0 ] && echo "ok   25 fresh p-256 and 10 fresh p-384 signatures verified" || fail "$bad of 35 fresh ecdsa signatures did not verify"

[ $ok = 1 ] && echo "the certificate checker walks chains to the roots of the web, matches hosts and refuses what it should" || echo "certificate checker FAILED"
