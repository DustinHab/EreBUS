#!/bin/sh
# tlstest.sh -- the tls client against a server of our own: verified under an authority of one's own, refused without.
# - a test authority (EC) with an EC and an RSA intermediate, and a server leaf under each for the address 10.0.2.100
# - a python tls 1.3 server per chain on the host loopback; qemu's guestfwd hands the guest's 10.0.2.100:443 to one of them
#   (one connection per boot, which is why every case is its own boot)
# - the machine is pointed at https://10.0.2.100 as its release source and asked to check for an update; that fetch runs through tls
#  boot 1: authority written, tls strict, the ec chain  -> "the server is verified" (ecdsa signatures, ecdsa certificate verify)
#  boot 2: authority written, tls strict, the rsa chain -> verified (rsa pkcs1 in the chain, rsa-pss certificate verify)
#  boot 3: no authority, tls strict                     -> "not verified: no trusted authority", the fetch refused
cd "$(dirname "$0")/.."
. tools/testlib.sh
W=$BUILD/tls
PORT_EC=${TLSPORT:-8443}
PORT_RSA=$(( PORT_EC + 1 ))
rm -rf $W; mkdir -p $W

# --- the certificates ---
(
cd $W
openssl ecparam -name prime256v1 -genkey -noout -out root.key 2>/dev/null
openssl req -x509 -new -key root.key -sha256 -days 3650 -subj "/CN=Test Root" -out root.pem \
    -addext "basicConstraints=critical,CA:TRUE" -addext "keyUsage=critical,keyCertSign,cRLSign" 2>/dev/null
printf 'basicConstraints=critical,CA:TRUE\nkeyUsage=critical,keyCertSign\n' > ca.ext
printf 'subjectAltName=IP:10.0.2.100,DNS:test.local\nbasicConstraints=CA:FALSE\nkeyUsage=digitalSignature\nextendedKeyUsage=serverAuth\n' > leaf.ext
openssl ecparam -name prime256v1 -genkey -noout -out iec.key 2>/dev/null
openssl req -new -key iec.key -subj "/CN=Test EC Intermediate" -out iec.csr 2>/dev/null
openssl x509 -req -in iec.csr -CA root.pem -CAkey root.key -CAcreateserial -days 1000 -sha256 -extfile ca.ext -out iec.pem 2>/dev/null
openssl genrsa -out irsa.key 2048 2>/dev/null
openssl req -new -key irsa.key -subj "/CN=Test RSA Intermediate" -out irsa.csr 2>/dev/null
openssl x509 -req -in irsa.csr -CA root.pem -CAkey root.key -CAcreateserial -days 1000 -sha256 -extfile ca.ext -out irsa.pem 2>/dev/null
openssl ecparam -name prime256v1 -genkey -noout -out leafec.key 2>/dev/null
openssl req -new -key leafec.key -subj "/CN=test.local" -out leafec.csr 2>/dev/null
openssl x509 -req -in leafec.csr -CA iec.pem -CAkey iec.key -CAcreateserial -days 30 -sha256 -extfile leaf.ext -out leafec.pem 2>/dev/null
openssl genrsa -out leafrsa.key 2048 2>/dev/null
openssl req -new -key leafrsa.key -subj "/CN=test.local" -out leafrsa.csr 2>/dev/null
openssl x509 -req -in leafrsa.csr -CA irsa.pem -CAkey irsa.key -CAcreateserial -days 30 -sha256 -extfile leaf.ext -out leafrsa.pem 2>/dev/null
cat leafec.pem iec.pem > chain-ec.pem
cat leafrsa.pem irsa.pem > chain-rsa.pem
)
AUTH=$(openssl x509 -in $W/root.pem -pubkey -noout | openssl pkey -pubin -outform DER | base64 -w0)
echo "authority line: ${#AUTH} letters"

# --- the servers ---
cat > $W/serve.py <<'PY'
import ssl, socket, sys
port, chain, key = int(sys.argv[1]), sys.argv[2], sys.argv[3]
ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
ctx.minimum_version = ssl.TLSVersion.TLSv1_3
ctx.load_cert_chain(chain, key)
s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("127.0.0.1", port))
s.listen(4)
# big enough for the updater to read it as a reply that is not a package, rather than as no reply
body = b"EreBUS tls test page\n" * 250
while True:
    c, _ = s.accept()
    try:
        t = ctx.wrap_socket(c, server_side=True)
        t.recv(4096)
        t.sendall(b"HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %d\r\nConnection: close\r\n\r\n" % len(body) + body)
        try: t.unwrap()
        except Exception: pass
        t.close()
        sys.stderr.write("server: served one page\n"); sys.stderr.flush()
    except Exception as e:
        sys.stderr.write("server: %s\n" % e); sys.stderr.flush()
        try: c.close()
        except Exception: pass
PY
python3 $W/serve.py $PORT_EC  $W/chain-ec.pem  $W/leafec.key  > $W/srv-ec.log 2>&1 &
SRV1=$!
python3 $W/serve.py $PORT_RSA $W/chain-rsa.pem $W/leafrsa.key > $W/srv-rsa.log 2>&1 &
SRV2=$!
waitport $PORT_EC 20; waitport $PORT_RSA 20

# boot <log> <host port> <with authority: yes|no>
boot() {
    LOG=$1
    rm -f $LOG
    fresh_store $W/store.img
    fresh_vars $W/vars.fd
    cp $BUILD/esp.img $W/esp.img
    {
        bootwait $LOG
        keys tab tab tab tab tab pause
        say "go system"
        say "go settings"
        [ "$3" = yes ] && say "write authority | $AUTH"
        say "write tls | strict"
        say "write update | auto https://10.0.2.100"
        say "back"
        say "back"
        sleep 1
        say "update check"
        waitlog $LOG 'update: no update package\|update: the reply\|update: package version' 150
        sleep 1
        echo quit
    } | qemu-system-x86_64 $QEMU_BASE \
      -drive if=pflash,format=raw,file=$W/vars.fd \
      -drive format=raw,file=$W/esp.img \
      -drive id=store,file=$W/store.img,format=raw,if=none \
      -device ide-hd,drive=store,bus=ide.1 \
      -device e1000,netdev=n0 -netdev user,id=n0,guestfwd=tcp:10.0.2.100:443-tcp:127.0.0.1:$2 \
      -serial file:$LOG >/dev/null 2>&1
}

boot $W/boot1.log $PORT_EC  yes
boot $W/boot2.log $PORT_RSA yes
boot $W/boot3.log $PORT_EC  no

kill $SRV1 $SRV2 2>/dev/null
wait 2>/dev/null

ok=1
for b in 1 2 3; do
    echo "--- boot $b ---"
    grep -a 'tls:  \(the server\|certificate\)\|update: \(no update\|the reply\)\|settings' $W/boot$b.log | cut -c1-140
done
echo "--- the checks ---"
grep -aq 'tls:  the server is verified; its chain is signed by an authority from the settings' $W/boot1.log \
    && echo "the ec chain under the written authority is verified" || { echo "FAILED: ec chain not verified"; ok=0; }
grep -aq 'update: the reply was not an update package' $W/boot1.log \
    && echo "and the page came through the sealed, verified channel" || { echo "FAILED: no page over the ec chain"; ok=0; }
grep -aq 'tls:  the server is verified; its chain is signed by an authority from the settings' $W/boot2.log \
    && echo "the rsa chain under the same authority is verified" || { echo "FAILED: rsa chain not verified"; ok=0; }
grep -aq 'update: the reply was not an update package' $W/boot2.log \
    && echo "and the page came through" || { echo "FAILED: no page over the rsa chain"; ok=0; }
grep -aq 'tls:  the server is not verified: no trusted authority signs the chain; refused' $W/boot3.log \
    && echo "without the authority the server is not verified and, being strict, refused" || { echo "FAILED: unverified server not refused"; ok=0; }
grep -aq 'update: no update package' $W/boot3.log && ! grep -aq 'update: the reply' $W/boot3.log \
    && echo "and no page came from it" || { echo "FAILED: a page came from the refused server"; ok=0; }
[ $ok = 1 ] && echo "the tls client verifies a server under a trusted authority and refuses one without" || echo "tls verification FAILED"
