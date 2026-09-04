#!/bin/sh
# pipe-tap.sh -- watches the host wire while B sends: does slirp pass the datagram? (test-rig diagnosis)
cd "$(dirname "$0")/.."
BUILD=build

tcpdump -i any -n -l udp port 7801 > $BUILD/tap.log 2>/dev/null &
TAP=$!

HOSTIP=$(hostname -I | awk '{print $1}')
IPKEYS=$(echo "$HOSTIP" | sed -e 's/\./ dot /g' -e 's/\([0-9]\)/\1 /g')

rm -f $BUILD/teststore.img
tools/look.sh $BUILD/screen-pipe-b.png \
  tab tab tab p e e r ret \
  ret p e e r spc shift-backslash spc $IPKEYS spc 7 8 0 1 \
  left \
  up up up up up up up up up up up up up up up up right \
  home m100,20 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 \
  m100,0 m100,0 m100,0 m95,0 click \
  m0,1 m0,1 m0,1 m0,1 m0,1 m0,1 >/dev/null 2>&1

sleep 2
kill $TAP 2>/dev/null
echo "--- sender said ---"
grep -a 'pipedbg: offering' $BUILD/serial.log | head -2
echo "--- host saw ---"
cat $BUILD/tap.log
