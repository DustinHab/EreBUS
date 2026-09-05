#!/bin/sh
# pkitest.sh -- the certificate checker on the host: openssl-made chains, the live github chains, CertificateVerify signatures.
# - builds build/pkihost from the kernel's own pki files (tools/pkihost.c)
# - a test authority (EC root) with an EC and an RSA intermediate; leaves that are good, expired, not yet valid,
#   for the wrong host, wildcard, by address, signed by a non-authority, signed by an unrelated authority, tampered
# - the fixtures in tools/pki/fixtures are the chains seen from github.com and the release cdn on 2026-09-05,
#   checked against the built-in authorities at that date
# - CertificateVerify signatures made with openssl over the TLS 1.3 content: ecdsa (0403) and rsa-pss (0804)
cd "$(dirname "$0")/.."
ROOT=$(pwd)
BUILD=${BUILD:-build}
case "$BUILD" in /*) W=$BUILD/pki ;; *) W=$ROOT/$BUILD/pki ;; esac
rm -rf $W; mkdir -p $W
H=$W/pkihost

clang -O1 -g -std=c11 -Wall -Wno-incompatible-library-redeclaration -Ikernel/include -o $H tools/pkihost.c \
    kernel/net/asn1.c kernel/net/bn.c kernel/net/p256.c kernel/net/rsa.c kernel/net/x509.c \
    kernel/net/sha256.c kernel/net/pki_selftest.c || { echo "FAILED: pkihost did not build"; exit 1; }

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

# --- a test authority and its chains ---
cd $W
openssl ecparam -name prime256v1 -genkey -noout -out root.key 2>/dev/null
openssl req -x509 -new -key root.key -sha256 -days 3650 -subj "/CN=Test Root" -out root.pem \
    -addext "basicConstraints=critical,CA:TRUE" -addext "keyUsage=critical,keyCertSign,cRLSign" 2>/dev/null
openssl ecparam -name prime256v1 -genkey -noout -out other.key 2>/dev/null
openssl req -x509 -new -key other.key -sha256 -days 3650 -subj "/CN=Other Root" -out other.pem \
    -addext "basicConstraints=critical,CA:TRUE" 2>/dev/null

printf 'basicConstraints=critical,CA:TRUE\nkeyUsage=critical,keyCertSign\n' > ca.ext
openssl ecparam -name prime256v1 -genkey -noout -out iec.key 2>/dev/null
openssl req -new -key iec.key -subj "/CN=Test EC Intermediate" -out iec.csr 2>/dev/null
openssl x509 -req -in iec.csr -CA root.pem -CAkey root.key -CAcreateserial -days 1000 -sha256 -extfile ca.ext -out iec.pem 2>/dev/null
openssl genrsa -out irsa.key 2048 2>/dev/null
openssl req -new -key irsa.key -subj "/CN=Test RSA Intermediate" -out irsa.csr 2>/dev/null
openssl x509 -req -in irsa.csr -CA root.pem -CAkey root.key -CAcreateserial -days 1000 -sha256 -extfile ca.ext -out irsa.pem 2>/dev/null

printf 'subjectAltName=DNS:example.test,DNS:*.wild.test,IP:10.0.2.100\nbasicConstraints=CA:FALSE\nkeyUsage=digitalSignature\nextendedKeyUsage=serverAuth\n' > leaf.ext
mkleaf() {   # mkleaf <name> <key type ec|rsa> <issuer name> [ext file]
    if [ "$2" = ec ]; then openssl ecparam -name prime256v1 -genkey -noout -out $1.key 2>/dev/null
    else openssl genrsa -out $1.key 2048 2>/dev/null; fi
    openssl req -new -key $1.key -subj "/CN=$1" -out $1.csr 2>/dev/null
    openssl x509 -req -in $1.csr -CA $3.pem -CAkey $3.key -CAcreateserial -days 30 -sha256 -extfile ${4:-leaf.ext} -out $1.pem 2>/dev/null
    openssl x509 -in $1.pem -outform DER -out $1.der 2>/dev/null
}
for c in root other iec irsa; do openssl x509 -in $c.pem -outform DER -out $c.der 2>/dev/null; done
mkleaf leafec ec iec
mkleaf leafrsa rsa irsa
mkleaf leafbyroot ec root
mkleaf fakeca ec iec                       # a leaf that then signs another: not an authority
mkleaf leafbyfake ec fakeca
mkleaf leafother ec other
cp leafec.der tampered.der
printf '\377' | dd of=tampered.der bs=1 seek=$(( $(wc -c < leafec.der) - 1 )) conv=notrunc 2>/dev/null
NOW=$(date +%s)
cd $ROOT

expect verified "ec leaf, ec intermediate, root authority"      example.test $NOW -a $W/root.der $W/leafec.der $W/iec.der
expect verified "rsa leaf, rsa intermediate, root authority"    example.test $NOW -a $W/root.der $W/leafrsa.der $W/irsa.der
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
expect "no host that matches" "wrong host"                      wrong.test $NOW -a $W/root.der $W/leafec.der $W/iec.der
expect "no host that matches" "wildcard does not span two labels" b.a.wild.test $NOW -a $W/root.der $W/leafec.der $W/iec.der
expect "no host that matches" "wildcard needs a label"          wild.test $NOW -a $W/root.der $W/leafec.der $W/iec.der
expect "no host that matches" "wrong address"                   10.0.2.101 $NOW -a $W/root.der $W/leafec.der $W/iec.der
expect "has expired"    "expired"                               example.test $(( NOW + 40 * 86400 )) -a $W/root.der $W/leafec.der $W/iec.der
expect "not yet valid"  "not yet valid"                         example.test $(( NOW - 2 * 86400 )) -a $W/root.der $W/leafec.der $W/iec.der
expect "not marked as an authority" "signed by a leaf"          example.test $NOW -a $W/root.der $W/leafbyfake.der $W/fakeca.der $W/iec.der
expect "did not verify" "tampered signature"                    example.test $NOW -a $W/root.der $W/tampered.der $W/iec.der
expect "could not be read" "not a certificate"                  example.test $NOW -a $W/root.der $W/leaf.ext

# --- the live chains, as seen on 2026-09-05, against the built-in authorities ---
F=tools/pki/fixtures
THEN=1788609600     # 2026-09-05 00:00 utc
expect verified "github.com leaf under Sectigo E36"             github.com $THEN $F/github-leaf.der $F/github-e36.der $F/github-e46.der
expect verified "github.com leaf alone"                         github.com $THEN $F/github-leaf.der
expect verified "www.github.com"                                www.github.com $THEN $F/github-leaf.der
expect verified "release cdn leaf under Let's Encrypt YR1"      objects.githubusercontent.com $THEN $F/cdn-leaf.der $F/cdn-yr1.der $F/cdn-rootyr.der
expect verified "release-assets host on the same leaf"          release-assets.githubusercontent.com $THEN $F/cdn-leaf.der $F/cdn-yr1.der
expect "no host that matches" "the cdn leaf for another host"   evil.example $THEN $F/cdn-leaf.der $F/cdn-yr1.der
expect "has expired"    "the github leaf a year on"             github.com $(( THEN + 365 * 86400 )) $F/github-leaf.der $F/github-e36.der
expect "not supported" "the sectigo intermediate itself (signed with sha-384) is not checked" github.com $THEN $F/github-e36.der $F/github-e46.der

# --- CertificateVerify signatures over the TLS 1.3 content ---
cd $W
{ printf '%64s' ''; printf 'TLS 1.3, server CertificateVerify'; printf '\000'; head -c 32 /dev/urandom; } > content
cp content content2; printf 'x' | dd of=content2 bs=1 seek=100 conv=notrunc 2>/dev/null
openssl pkey -in leafec.key -pubout -outform DER -out leafec.spki 2>/dev/null
openssl pkey -in leafrsa.key -pubout -outform DER -out leafrsa.spki 2>/dev/null
openssl pkeyutl -sign -inkey leafec.key -rawin -digest sha256 -in content -out cv.ec 2>/dev/null
openssl pkeyutl -sign -inkey leafrsa.key -rawin -digest sha256 -pkeyopt rsa_padding_mode:pss -pkeyopt rsa_pss_saltlen:32 -in content -out cv.pss 2>/dev/null
cd $ROOT
sig() {   # sig <expect 0|1> <what> <args>
    want=$1; what=$2; shift 2
    out=$($H sig "$@" 2>&1); rc=$?
    [ $rc = $want ] && echo "ok   $what: $out" || fail "$what: $out"
}
sig 0 "certificate verify, ecdsa"          $W/leafec.spki 0403 $W/content $W/cv.ec
sig 1 "certificate verify, ecdsa, content changed" $W/leafec.spki 0403 $W/content2 $W/cv.ec
sig 0 "certificate verify, rsa-pss"        $W/leafrsa.spki 0804 $W/content $W/cv.pss
sig 1 "certificate verify, rsa-pss, content changed" $W/leafrsa.spki 0804 $W/content2 $W/cv.pss
sig 1 "rsa-pss signature under the ec scheme" $W/leafrsa.spki 0403 $W/content $W/cv.pss

# --- many fresh keys and signatures, so that no single curve point is the only one ever checked ---
cd $W
n=0; bad=0
while [ $n -lt 25 ]; do
    openssl ecparam -name prime256v1 -genkey -noout -out r.key 2>/dev/null
    openssl pkey -in r.key -pubout -outform DER -out r.spki 2>/dev/null
    head -c $((n * 7 + 1)) /dev/urandom > r.msg
    openssl pkeyutl -sign -inkey r.key -rawin -digest sha256 -in r.msg -out r.sig 2>/dev/null
    $H sig r.spki 0403 r.msg r.sig >/dev/null 2>&1 || bad=$((bad + 1))
    n=$((n + 1))
done
cd $ROOT
[ $bad = 0 ] && echo "ok   25 fresh ecdsa signatures verified" || fail "$bad of 25 fresh ecdsa signatures did not verify"

[ $ok = 1 ] && echo "the certificate checker walks chains, matches hosts and refuses what it should" || echo "certificate checker FAILED"
