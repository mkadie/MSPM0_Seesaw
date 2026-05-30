#!/bin/bash
# Flash an MSPM0G3507 over the EXP432 XDS110 + TC2030 using UniFlash DSLite.
#
# Usage:  ./flash.sh [build/firmware.elf]
#   - Pads the image to an 8-byte multiple (MSPM0 flash writes 64-bit words;
#     DSLite errors "Length of block ... should be divisible by 8" otherwise).
#   - Toggles nSRST before connecting so a blank/garbage-running chip can be
#     halted (recovers the "Connection to MSPM0 core failed" blank-flash trap).
#     This REQUIRES RST_N wired: EXP432 RST -> TC2030 pin 1.
#   - Erases, programs, verifies, then releases reset so the firmware runs.
set -u
ELF="${1:-$(dirname "$0")/build/firmware.elf}"
UF="${UNIFLASH_DIR:-$HOME/ti/uniflash_9.3.0}"
CCXML=$UF/deskdb/content/TICloudAgent/linux/scripting/examples/debugger/mspm0g3507/mspm0g3507.ccxml
XRDIR=$UF/deskdb/content/TICloudAgent/linux/ccs_base/common/uscif/xds110
export LD_LIBRARY_PATH="$XRDIR:$UF/deskdb/content/TICloudAgent/linux/ccs_base/common/uscif:${LD_LIBRARY_PATH:-}"
RST="$XRDIR/xds110reset"

[ -f "$ELF" ] || { echo "no ELF at $ELF"; exit 2; }

# Pad to next multiple of 8 bytes -> padded hex next to the ELF.
end=$(arm-none-eabi-size -A "$ELF" 2>/dev/null | awk '/Total/{print $2}')
# Compute byte length of the loadable image from objcopy'd bin instead (robust):
TMPBIN=$(mktemp --suffix=.bin)
arm-none-eabi-objcopy -O binary "$ELF" "$TMPBIN"
len=$(stat -c %s "$TMPBIN")
pad=$(( (len + 7) / 8 * 8 ))
HEX="${ELF%.elf}_pad.hex"
arm-none-eabi-objcopy -O binary --pad-to "$pad" --gap-fill 0xFF "$ELF" "$TMPBIN"
arm-none-eabi-objcopy -I binary -O ihex "$TMPBIN" "$HEX"
rm -f "$TMPBIN"
echo "image $len B -> padded $pad B -> $HEX"

for try in 1 2 3 4 5; do
  echo "=== flash attempt $try ==="
  "$RST" --action toggle >/dev/null 2>&1     # break blank-flash garbage loop
  sleep 1                                     # let reset settle before connect
  out=$(cd "$UF" && timeout 180 ./dslite.sh --mode flash -c "$CCXML" -e -f "$HEX" -v 2>&1)
  echo "$out" | grep -iE "verif|^Success|fail:|error:|fatal" | head -6
  if echo "$out" | grep -qiE "verification successful|^Success"; then
    "$RST" --action deassert >/dev/null 2>&1  # release reset -> run
    echo ">>> FLASH OK (attempt $try). Reset released; firmware running."
    exit 0
  fi
  sleep 1
done
echo ">>> FLASH FAILED after retries. Check RST_N wiring (TC2030 pin1) and power."
exit 1
