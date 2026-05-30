#!/bin/bash
# Quick XDS110 -> TC2030 -> MSPM0G3507 SWD connect test.
# Success = it prints 16 bytes of flash. Failure = "Error connecting to the target".
set -u
UF="${UNIFLASH_DIR:-$HOME/ti/uniflash_9.3.0}"
CCXML=$UF/deskdb/content/TICloudAgent/linux/scripting/examples/debugger/mspm0g3507/mspm0g3507.ccxml
OUT=/tmp/m0read.bin
rm -f "$OUT"
echo "Reading 16 bytes @ 0x0 over SWD ..."
"$UF/dslite.sh" --mode memory -c "$CCXML" -r 0x00000000,16 -o "$OUT" -e 2>&1 \
  | grep -iE "Connecting|Error connecting|DAP Connection|Operation was aborted|complete|Success"
if [ -s "$OUT" ]; then
  echo "*** SWD CONNECT OK — flash bytes @0x0: ***"
  xxd "$OUT"
else
  echo "*** SWD CONNECT FAILED (see Error -615 = bad SWD framing/wiring). ***"
fi
