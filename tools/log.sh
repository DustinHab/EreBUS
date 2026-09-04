#!/bin/sh
# log.sh -- the serial log without the firmware's escape sequences.
# - a script because a grep pattern does not survive PowerShell -> WSL quoting
#   tools/log.sh [pattern] [file]

PATTERN=${1:-.}
FILE=${2:-build/serial.log}

cat -v "$FILE" \
  | sed 's/\^\[\[[0-9;]*[A-Za-z]//g' \
  | sed 's/\^M$//' \
  | grep -E "$PATTERN"
