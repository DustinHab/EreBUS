#!/bin/sh
# wifitest.sh -- the WPA2 station against tools/wifi-ap.py (802.11 frames inside Ethernet).
# - 'networks' lists the AP; a wrong passphrase is refused; 'join' asks and takes the passphrase unseen
# - handshake, encrypted frames, DHCP lease through them, pings answered; second boot auto-joins

cd "$(dirname "$0")/.."
. tools/testlib.sh
LOG=$BUILD/wifi-serial.log
LOG2=$BUILD/wifi-serial2.log
PORT=${WIFIPORT:-8020}
rm -f $BUILD/teststore.img $LOG $LOG2 $BUILD/wifi-ap.log $BUILD/wifi-ap2.log $BUILD/wifitest.ppm
fresh_store $BUILD/teststore.img
fresh_vars $BUILD/test-vars.fd

python3 tools/wifi-ap.py --port $PORT --ssid "erebus test" --password "correct horse" > $BUILD/wifi-ap.log 2>&1 &
AP=$!
sleep 1

{
    bootwait $LOG
    keys tab tab tab tab tab pause
    say "networks"
    say "join erebus test with wrong words"
    waitlog $LOG 'did not open it\|handshake timeout' 30
    say "join erebus test"
    say "correct horse"
    waitlog $LOG "joined 'erebus test'" 40
    waitlog $LOG 'by lease' 30
    say "wifi"
    say "networks"
    sleep 1
    echo "screendump $BUILD/wifitest.ppm"
    sleep 1
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -drive id=store,file=$BUILD/teststore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 -netdev socket,id=n0,connect=127.0.0.1:$PORT \
  -serial file:$LOG >/dev/null 2>&1

kill $AP 2>/dev/null
wait $AP 2>/dev/null
python3 tools/ppm2png.py $BUILD/wifitest.ppm $BUILD/wifitest.png 2>/dev/null

# The second boot: the network was written into the settings when the
# join worked, so this time nobody types anything -- the station hears
# the network, remembers it, and joins on its own.
python3 tools/wifi-ap.py --port $PORT --ssid "erebus test" --password "correct horse" > $BUILD/wifi-ap2.log 2>&1 &
AP=$!
sleep 1
{
    bootwait $LOG2
    waitlog $LOG2 "joined 'erebus test'" 60
    waitlog $LOG2 'by lease' 30
    sleep 1
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -drive id=store,file=$BUILD/teststore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 -netdev socket,id=n0,connect=127.0.0.1:$PORT \
  -serial file:$LOG2 >/dev/null 2>&1
kill $AP 2>/dev/null
wait $AP 2>/dev/null
echo "--- the second boot, nobody typing ---"
grep -a 'wifi:\|net:  10\.' $LOG2 | cut -c1-110

echo "--- the station ---"
grep -a 'wifi:\|net:  10\.' $LOG | cut -c1-110
echo "--- the access point ---"
grep -a -v 'arp:' $BUILD/wifi-ap.log | cut -c1-110 | tail -12
echo "--- the checks ---"
ok=1
grep -aq 'wifi: self test passed' $LOG && echo "the station's crypto self test passed" || { echo "FAILED: self test"; ok=0; }
grep -aq 'wrong passphrase' $BUILD/wifi-ap.log && grep -aq 'did not open it' $LOG && echo "the wrong passphrase was refused, and reported" || { echo "FAILED: the wrong passphrase"; ok=0; }
grep -aq "joined 'erebus test'" $LOG && echo "the right one joined" || { echo "FAILED: no join"; ok=0; }
grep -aq 'handshake is done' $BUILD/wifi-ap.log && echo "the access point confirms the handshake" || { echo "FAILED: handshake"; ok=0; }
grep -aq 'net:  10.9.8.20 by lease' $LOG && echo "a dhcp lease came through the encrypted link" || { echo "FAILED: no lease"; ok=0; }
grep -aq 'answered ping' $BUILD/wifi-ap.log && echo "the station answered pings" || { echo "FAILED: no ping answered"; ok=0; }
grep -aq 'remembered' $LOG2 && grep -aq "joined 'erebus test'" $LOG2 && echo "the second boot joined on its own, from the settings" || { echo "FAILED: the second boot did not join by itself"; ok=0; }
[ $ok = 1 ] && echo "the wireless station works" || echo "the wireless station FAILED"
