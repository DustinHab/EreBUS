#!/bin/sh
# wifitest.sh -- the wireless station against the virtual access point.
#
# The machine's only wire leads to tools/wifi-ap.py, which is the air:
# an access point named "erebus test" with a passphrase. Through the
# terminal: 'networks' lists it; a join with the wrong passphrase is
# turned away and says so; 'join erebus test' asks for the passphrase,
# which is typed unseen; the handshake finishes, the frames are
# sealed, an address arrives by lease through the seal, and the
# access point's pings are answered. The access point's own log is
# the other half of the proof.

cd "$(dirname "$0")/.."
BUILD=build
rm -f $BUILD/teststore.img $BUILD/wifi-serial.log $BUILD/wifi-ap.log $BUILD/wifitest.ppm
dd if=/dev/zero of=$BUILD/teststore.img bs=1M count=32 status=none
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/test-vars.fd

python3 tools/wifi-ap.py --port 8020 --ssid "erebus test" --password "correct horse" > $BUILD/wifi-ap.log 2>&1 &
AP=$!
sleep 1

keys() {
    for k in "$@"; do
        case "$k" in
            pause) sleep 1.6 ;;
            *) echo "sendkey $k"; sleep 0.35 ;;
        esac
    done
}

{
    sleep 26
    keys tab tab tab tab tab pause
    keys n e t w o r k s ret pause
    keys j o i n spc e r e b u s spc t e s t spc w i t h spc w r o n g spc w o r d s ret
    sleep 6
    keys j o i n spc e r e b u s spc t e s t ret pause
    keys c o r r e c t spc h o r s e ret
    sleep 14
    keys w i f i ret pause
    keys n e t w o r k s ret pause
    sleep 2
    echo "screendump $BUILD/wifitest.ppm"
    sleep 2
    echo quit
} | qemu-system-x86_64 -machine q35 -m 512M -cpu max \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -vga none -device VGA,edid=on,xres=1280,yres=800 \
  -drive id=store,file=$BUILD/teststore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 -netdev socket,id=n0,connect=127.0.0.1:8020 \
  -display none -monitor stdio \
  -serial file:$BUILD/wifi-serial.log >/dev/null 2>&1

kill $AP 2>/dev/null
wait $AP 2>/dev/null
python3 tools/ppm2png.py $BUILD/wifitest.ppm $BUILD/wifitest.png 2>/dev/null

# The second boot: the network was written into the settings when the
# join worked, so this time nobody types anything -- the station hears
# the network, remembers it, and joins on its own.
python3 tools/wifi-ap.py --port 8020 --ssid "erebus test" --password "correct horse" > $BUILD/wifi-ap2.log 2>&1 &
AP=$!
sleep 1
{
    sleep 40
    echo quit
} | qemu-system-x86_64 -machine q35 -m 512M -cpu max \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -vga none -device VGA,edid=on,xres=1280,yres=800 \
  -drive id=store,file=$BUILD/teststore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 -netdev socket,id=n0,connect=127.0.0.1:8020 \
  -display none -monitor stdio \
  -serial file:$BUILD/wifi-serial2.log >/dev/null 2>&1
kill $AP 2>/dev/null
wait $AP 2>/dev/null
echo "--- the second boot, nobody typing ---"
grep -a 'wifi:\|net:  10\.' $BUILD/wifi-serial2.log | cut -c1-110

echo "--- the station ---"
grep -a 'wifi:\|net:  10\.' $BUILD/wifi-serial.log | cut -c1-110
echo "--- the access point ---"
grep -a -v 'arp:' $BUILD/wifi-ap.log | cut -c1-110 | tail -12
echo "--- the checks ---"
ok=1
grep -aq 'wifi: self test passed' $BUILD/wifi-serial.log && echo "the station's arithmetic checks out" || { echo "FAILED: self test"; ok=0; }
grep -aq 'wrong passphrase' $BUILD/wifi-ap.log && grep -aq 'did not open it' $BUILD/wifi-serial.log && echo "the wrong passphrase was turned away, and said so" || { echo "FAILED: the wrong passphrase"; ok=0; }
grep -aq "joined 'erebus test'" $BUILD/wifi-serial.log && echo "the right one joined" || { echo "FAILED: no join"; ok=0; }
grep -aq 'handshake is done' $BUILD/wifi-ap.log && echo "the access point agrees the keys are held" || { echo "FAILED: handshake"; ok=0; }
grep -aq 'net:  10.9.8.20 by lease' $BUILD/wifi-serial.log && echo "an address came by lease through the seal" || { echo "FAILED: no lease"; ok=0; }
grep -aq 'answered ping' $BUILD/wifi-ap.log && echo "the station answered pings through the seal" || { echo "FAILED: no ping answered"; ok=0; }
grep -aq 'remembered' $BUILD/wifi-serial2.log && grep -aq "joined 'erebus test'" $BUILD/wifi-serial2.log && echo "the second boot joined on its own, from memory" || { echo "FAILED: the second boot did not join by itself"; ok=0; }
[ $ok = 1 ] && echo "the wireless station works" || echo "the wireless station FAILED"
