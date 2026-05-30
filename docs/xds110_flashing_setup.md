# XDS110 (MSP-EXP432P401R) → TC2030 → MSPM0G3507 flashing setup

Status as of 2026-05-29: **WORKING — flashed + verified the blink image end-to-end.**
Goal: flash the MSPM0G3507 seesaw board over SWD using the EXP432 LaunchPad's onboard
XDS110 debug probe through a TC2030 cable into J1.

## RESOLVED — how it finally worked

One command: `firmware/mspm0/flash.sh [build/firmware.elf]`. Two gotchas it handles:

1. **Probe wiring** was the original `-615`: SWDIO/SWCLK had to land on the EXP432 J101
   **emulator-side** `TMS_SWDIO` / `TCK_SWCLK` pins, with the Fruit Jam off the shared bus.
   The frozen `0x03000001` readback was the tell that the probe signals never reached the chip.
2. **Image length must be a multiple of 8 bytes** — MSPM0 flash writes 64-bit words; DSLite
   errors `Length of block ... should be divisible by 8`. Pad with objcopy `--pad-to ... --gap-fill 0xFF`.
3. **Blank-flash trap** — after the erase, main flash is blank, the core runs garbage and
   trips watchdog/clock → `Connection to MSPM0 core failed`. Recovery: pulse nSRST right
   before connecting (`xds110reset --action toggle`) so the debugger halts it at reset.
   This REQUIRES **RST_N wired (EXP432 RST → TC2030 pin 1)**.

Verified readback @0x0: `00 80 20 20  59 00 00 00 ...` = SP 0x20208000, reset 0x59 — matches the build.


## Software stack — WORKING and ready

Everything on the host side is installed and verified:

- **UniFlash 9.3.0** installed at `~/ti/uniflash_9.3.0` (CCS lacked MSPM0 device support;
  UniFlash bundles it). Installer threw a `libgconf-2.so.4` warning — that only affects the
  GUI; the `dslite.sh` CLI works fine.
- **DSLite** (TI CLI flasher): `~/ti/uniflash_9.3.0/dslite.sh`
  - DebugServer/binary: `.../deskdb/content/TICloudAgent/linux/ccs_base/DebugServer/bin/DSLite`
- **MSPM0G3507 device support** present in UniFlash targetdb
  (`.../common/targetdb/devices/MSPM0G3507.xml`).
- **ccxml** (XDS110 + MSPM0G3507): `firmware/mspm0/mspm0g3507.ccxml` (copy of the UniFlash
  scripting example; relative hrefs resolve against UniFlash's targetdb).
- **udev rules** installed: `/etc/udev/rules.d/71-ti-permissions.rules` +
  `70-mm-no-ti-emulators.rules` (from UniFlash). XDS110 USB node (`0451:bef3`) is now `0666`.

### Verified working commands

```bash
UF=~/ti/uniflash_9.3.0
CCXML=$UF/deskdb/content/TICloudAgent/linux/scripting/examples/debugger/mspm0g3507/mspm0g3507.ccxml

# Connect-only read test (16 bytes @ 0x0):
$UF/dslite.sh --mode memory -c "$CCXML" -r 0x00000000,16 -o /tmp/m0read.bin -e

# Flash (erase + program + verify) a hex or elf:
$UF/dslite.sh --mode flash  -c "$CCXML" -e -f firmware/mspm0/build/firmware.hex -v
```

DSLite gets all the way through: loads device support, maps registers, inits
CS_DAP_0 / CORTEX_M0P / SEC_AP, and issues XDS110 commands (XDS_VERSION, SWD_CONNECT...).

## The blocker — SWD physical link (Error -615), NOT software

The connect fails at the DAP read:

```
XDS110 SWD_CONNECT      -> ok
XDS110 CMAPI_CONNECT    -> 0xfffffd99
DpRegRead(0x0) (DPIDR)  -> 0x03000001 = Error -615
  "The target failed to see a correctly formatted SWD header."
```

- Same result at 5.5 MHz and at 500 kHz SWD clock → **not a clock/signal-integrity margin issue.**
  (Lowered the clock to 500 kHz to test, then reverted to the 5.5 MHz default. Backup of the
  connection xml: `TIXDS110_Connection.xml.orig.bak`.)
- The DPIDR read returns a consistent malformed `0x03000001` → bus is not framing correctly:
  classic miswire / no-target / bus-contention signature.

### Why we know the chip itself is fine

The earlier Fruit-Jam bit-banged SWD path (`fruit_jam/mspm0_swd.py`) **successfully read this
chip's DPIDR, CTRL/STAT and flash** (see `FINDINGS.md` / `STATUS_FOR_USER.md`). The MSPM0
SWD-DP responds when wired correctly. The only thing new this session is the
**EXP432 XDS110 → TC2030 → J1** path, so the fault is in that wiring/power.

## Hardware checklist (user side) — most likely first

1. **EXP432 isolation jumpers removed.** On the LaunchPad, the XDS110 SWD signals also run to
   the onboard MSP432 through the isolation block (J101/J103). Those signal jumpers
   (TMS/SWDIO, TCK/SWCLK, RST) must be **removed** so the onboard MSP432 doesn't load/fight
   the bus. Keep GND.
2. **SWDIO ↔ SWCLK not swapped.** Map EXP432 isolation-block → TC2030 / J1:
   - GND  → TC2030 pin 3 (GND / PA-VSS)
   - TCK  → TC2030 pin 5 (SWCLK = MSPM0 PA20)
   - TMS  → TC2030 pin 4 (SWDIO = MSPM0 PA19)
   - RST  → TC2030 pin 1 (RST_N)
   - 3V3  → TC2030 pin 2 (VDD) — for Vref AND common reference
3. **Target powered + common ground.** XDS110 connection defaults to *"Target supplied power"*
   — the probe does NOT power the board; it only senses Vref. The MSPM0 board must have its own
   3.3 V present (e.g. from the Fruit Jam), and the XDS110's 3V3/Vref pin must be tied to that
   same rail. Confirm GND is common between EXP432, MSPM0 board, and whatever powers it.
4. **nRESET wired (TC2030 pin 1).** Lets DSLite do connect-under-reset.

Once SWD reads a valid DPIDR, `dslite.sh --mode flash ... -f build/firmware.hex` should program
the blink image; then write/flash the seesaw firmware per `mspm0_firmware_spec.md`.
