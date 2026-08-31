#!/bin/sh
# log.sh -- show the serial log without the firmware's escape sequences.
#
# Exists because quoting a grep pattern through PowerShell into WSL into
# bash loses a layer of quotes somewhere every other time. A script in
# the tree takes its argument plainly and is repeatable.
#
#   tools/log.sh [pattern] [file]

PATTERN=${1:-.}
FILE=${2:-build/serial.log}

cat -v "$FILE" \
  | sed 's/\^\[\[[0-9;]*[A-Za-z]//g' \
  | sed 's/\^M$//' \
  | grep -E "$PATTERN"
