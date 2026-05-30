# MSPM0_Seesaw

An [Adafruit Seesaw](https://www.adafruit.com/product/5690)-compatible I²C
peripheral implemented on a **TI MSPM0G3507** (Arm Cortex-M0+). It speaks the
Seesaw wire protocol (STATUS / GPIO modules) at the standard address `0x49`,
so it works with the stock `adafruit_seesaw` CircuitPython/Arduino driver, and
adds a small custom **EVENT** module (an ordered, buffered button-event FIFO).

It's exercised here from an **Adafruit Fruit Jam (RP2350B)** host over I²C, and
flashed over SWD using the **XDS110** debug probe on an MSP-EXP432P401R LaunchPad.

> **A note (and apology) on the docs:** a lot of the documentation in this repo
> is computer-assisted / AI-generated. I'm busy across a number of projects, and
> honestly I'm a lot better at *making* things than at *telling people about*
> them — so I leaned on tooling to get this written down at all. If something
> reads oddly or turns out to be wrong, trust the code; issues and PRs welcome.

## Why this exists

This firmware is **based on Adafruit's seesaw ATtiny1616 firmware**
([Adafruit_seesawPeripheral](https://github.com/adafruit/Adafruit_seesawPeripheral),
BSD-licensed). The goal is to **maximize compatibility with existing open-source
hardware and software** (the seesaw ecosystem) while letting us **embed exactly
the pieces we need/want** for our product:

- **Wake the host (RP2350) from *any* input while remembering state.** The
  peripheral latches input changes, so the main processor can sleep and be woken
  on activity without losing what happened.
- **A "latched" input.** If the main process misses a transient event because it
  was busy (e.g. playing sound or video), the seesaw has captured it — the host
  reads it back when it gets around to it, rather than dropping it.
- **Buffering** (a 16-deep ordered event FIFO), **room for debounce**, and
  **headroom for customization** specific to our hardware.

In fairness, **Adafruit's seesaw provides these capabilities too** — this port
exists mainly so we can run it on the **MSPM0G3507, chosen for its low power
consumption**, and embed it directly into our own board.

> The board in this repo (`MspM0_Tiny1616 v3`) is a **simple work/test layout**,
> not a finished product design — it's enough to bring the firmware up and prove
> the protocol end to end.

## What works (validated)

- I²C target at `0x49`; coexists on the bus with other devices.
- `STATUS`: HW_ID `0x84`, VERSION `0x00020100`, OPTIONS `0x07`, SELFTEST, SWRST.
- `GPIO`: DIRSET/DIRCLR, BULK / BULK_SET / BULK_CLR, INTFLAG (latch + clear),
  PULLEN — every exposed pin host-configurable as input or output.
- `EVENT` (custom, module `0x80`): LEN, POP_ALL — a 16-deep ordered FIFO of
  button events. Button edge-IRQs auto-suppress when a pin is driven as output.
- Verified against the official **`adafruit_seesaw`** library (`Seesaw(i2c, 0x49)`
  constructs, HW_ID accepted, STATUS/GPIO read).
- Full **bidirectional GPIO loopback** (Fruit Jam ⇄ seesaw) passes both ways.

See [`docs/mspm0_firmware_spec.md`](docs/mspm0_firmware_spec.md) §0 for the full
bring-up log, pin mapping, and gotchas.

## Repository layout

```
firmware/mspm0/        MSPM0G3507 firmware (bare-metal C, no SDK runtime)
  src/main.c           the seesaw peripheral
  src/device.h         board/peripheral register map + pin map
  src/hw_*.h           TI MSPM0 SDK register headers (BSD, see LICENSE)
  src/i2c_diag.c       bring-up: LED-on-I2C-activity diagnostic
  src/pintest_*.c      bring-up: GPIO connectivity test firmwares
  Makefile, linker.ld  arm-none-eabi-gcc build
  flash.sh             flash via XDS110 + UniFlash DSLite (8-byte pad + reset recovery)
  swd_connect_test.sh  quick SWD connectivity check
fruit_jam/             Fruit Jam (CircuitPython) host
  seesaw_test.py       client: scan, identity, selftest, gpio_test, loopback_test, monitor
  seesaw_lib_test.py   sanity check against the official adafruit_seesaw library
docs/                  spec, wiring, flashing guide, board reference + images
```

## Build

Needs the GNU Arm embedded toolchain (`arm-none-eabi-gcc`).

```bash
cd firmware/mspm0
make                # -> build/firmware.{elf,bin,hex}  (~1.9 KB)
```

## Flash (XDS110 over TC2030)

Uses TI **UniFlash**'s bundled DSLite. Set `UNIFLASH_DIR` if it isn't at
`~/ti/uniflash_9.3.0`. The script pads the image to an 8-byte boundary (MSPM0
flash writes 64-bit words) and pulses reset to recover a blank chip.

```bash
cd firmware/mspm0
./flash.sh                      # flashes build/firmware.elf
UNIFLASH_DIR=/opt/ti/uniflash ./flash.sh
```

Wiring + the SWD probe setup and pitfalls are in
[`docs/xds110_flashing_setup.md`](docs/xds110_flashing_setup.md).

## Host usage (CircuitPython)

Copy `fruit_jam/seesaw_test.py` to the Fruit Jam's `CIRCUITPY` drive. I²C is the
STEMMA QT bus (`board.STEMMA_I2C()`), with **SDA→PB3, SCL→PB2** on the seesaw
board (see [`docs/wiring_diagram.md`](docs/wiring_diagram.md)).

```python
import seesaw_test
seesaw_test.run()            # scan + identity + SELFTEST
seesaw_test.gpio_test()      # drive/read every GPIO over I2C
seesaw_test.loopback_test()  # bidirectional Fruit-Jam <-> seesaw GPIO test
seesaw_test.monitor()        # live button-event loop
```

Or with the official driver:

```python
import board
from adafruit_seesaw.seesaw import Seesaw
ss = Seesaw(board.STEMMA_I2C(), addr=0x49)
```

## Credits & license

Based on Adafruit's seesaw firmware and protocol (Dean Miller / Adafruit
Industries). Licensed under the **BSD 3-Clause** license — the same license as
Adafruit's seesaw — see [`LICENSE`](LICENSE). TI MSPM0 SDK register headers are
included under TI's BSD license with their notices retained.
