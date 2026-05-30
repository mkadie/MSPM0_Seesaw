# Wiring Diagram — Fruit Jam ↔ MSPM0_Tiny1616 v3

Draft v0.2. Covers two parallel buses, both permanent on the production PCB:

1. **SWD programming/debug bus** — Fruit Jam GPIO drives SWDIO/SWCLK/RST_N on the MSPM0. Used by CircuitPython (PIO-based SWD) to flash and debug the MSPM0. The same SWD nets are also brought out to the J1 TC2030 pad on the MSPM0 board for bench-only probe access.
2. **I2C application bus** — Fruit Jam STEMMA QT (GPIO20/21) talks to the MSPM0 firmware over I2C through SEESAW_LEFT pins 9/10.

Pin numbers come from `fruit_jam.md` and `MspM0_Tiny1616_v3.md` in this folder.

---

## 1. Connections at a glance

| Purpose         | Fruit Jam side                         | MSPM0 net / connector                                | Notes |
|-----------------|----------------------------------------|------------------------------------------------------|-------|
| 3.3 V power     | GPIO header **3V3**                    | SEESAW_RIGHT **pin 1** (3.3 V)                       | Single-source the rail from the Fruit Jam |
| Ground          | GPIO header **GND**                    | SEESAW_RIGHT **pin 2**, also TC2030 J1.3            | Tie all grounds together |
| **SWCLK**       | **D6 / GPIO6** (header)                | SWCLK net → PA20 (pin 35), TC2030 **J1.5**           | 470 Ω series resistor on the Fruit Jam side (see §3.2) |
| **SWDIO**       | **D7 / GPIO7** (header)                | SWDIO net → PA19 (pin 34), TC2030 **J1.4**           | 470 Ω series resistor on the Fruit Jam side |
| **RST_N**       | **D8 / GPIO8** (header), open-drain    | RST_N net → MCU pin 4, TC2030 **J1.1**               | Open-drain only — pulled up on the MSPM0 by R2; never drive high |
| SWO (optional)  | A1 / GPIO41 (header) — input only      | SWO net → PA21 (pin 39), TC2030 **J1.6**             | Optional: enables ITM/RTT trace later. Leave unconnected for v0.2. |
| I2C SDA         | STEMMA QT **SDA** = GPIO20             | SEESAW_LEFT **pin 9** (PB2)                          | Application I2C, after firmware is on the MSPM0 |
| I2C SCL         | STEMMA QT **SCL** = GPIO21             | SEESAW_LEFT **pin 10** (PB3)                         | Application I2C |

Why these specific Fruit Jam pins?

- **D6 / D7 / D8 (GPIO6/7/8)** are plain digital GPIO on the 2x16 header, not shared with HSTX/DVI (GPIO12-19), not on the analog mux, and consecutive — convenient for PIO assignment (one base pin + count). GPIO8 is also UART1_TX in its alternate function; we just leave UART1 unused here.
- **A1 / GPIO41** is on the header and tolerates being used as a plain input for SWO if/when we wire it.
- The application I2C stays on **GPIO20/21** so the standard `board.STEMMA_I2C()` / `board.I2C()` works unmodified.

---

## 2. ASCII diagram

```
                                 FRUIT JAM (RP2350B)                                          MSPM0_Tiny1616 v3 (MSPM0G3507)
                  ┌─────────────────────────────────────────┐                       ┌─────────────────────────────────────────┐
                  │                                         │                       │                                         │
                  │  STEMMA QT (JST-SH 4-pin)               │                       │  SEESAW_LEFT  (10-pin, 0.1")             │
                  │   ┌────────────────────────────────┐    │                       │   ┌────────────────────────────────┐    │
                  │   │ 1  GND                         ├────┼── black ──────────────┼── │ → use SEESAW_RIGHT pin 2 (GND)  │    │
                  │   │ 2  3V3                         ├────┼── red   ──────────────┼── │ → use SEESAW_RIGHT pin 1 (3V3)  │    │
                  │   │ 3  SDA  (= GPIO20)             ├────┼── blue  ──────────────┼── │ 9   SDA  → PB2  (pin 14)        │    │
                  │   │ 4  SCL  (= GPIO21)             ├────┼── yellow ─────────────┼── │ 10  SCL  → PB3  (pin 15)        │    │
                  │   └────────────────────────────────┘    │                       │   └────────────────────────────────┘    │
                  │                                         │                       │                                         │
                  │  GPIO header (2x16)                     │                       │  J1 — TC2030 (Tag-Connect 6-pin, SWD)    │
                  │   ┌────────────────────────────────┐    │  ── 470 Ω ─┐          │   ┌────────────────────────────────┐    │
                  │   │ 3V3                            ├────┼── red ─────┼─── 3V3 ──┼── │ 2  VDD                          │    │
                  │   │ GND                            ├────┼── black ───┼── GND ───┼── │ 3  VSS                          │    │
                  │   │ D6  / GPIO6  (SWCLK out)       ├────┼─[470Ω]─────┴── SWCLK ─┼── │ 5  SWCLK ── PA20 (pin 35)       │    │
                  │   │ D7  / GPIO7  (SWDIO i/o)       ├────┼─[470Ω]──────── SWDIO ─┼── │ 4  SWDIO ── PA19 (pin 34)       │    │
                  │   │ D8  / GPIO8  (RST_N OD out)    ├────┼─[470Ω]──────── RST_N ─┼── │ 1  VPP/MCLR ── pin 4            │    │
                  │   │ A1  / GPIO41 (SWO in, optional)├────┼──────────────── SWO ──┼── │ 6  LVP    ── PA21 (pin 39)      │    │
                  │   └────────────────────────────────┘    │                       │   └────────────────────────────────┘    │
                  │                                         │                       │                                         │
                  └─────────────────────────────────────────┘                       └─────────────────────────────────────────┘
```

---

## 3. Design notes

### 3.1 Why the SWD lines are permanent on the production PCB

On the production board both the Fruit Jam GPIO (D6/D7/D8) **and** the J1 TC2030 pad land on the same SWDIO/SWCLK/RST_N nets. That's intentional — the TC2030 footprint is just a pad-array, no physical receptacle, so most of the time nothing is plugged into it and there's no contention. When a TC2030 cable + CMSIS-DAP probe is *briefly* plugged in for bench debug, two drivers can be on the bus at once.

### 3.2 Series resistors handle the contention

Put **470 Ω** (1/16 W is fine) in series on each of SWCLK / SWDIO / RST_N at the Fruit Jam side. Reasoning:

- **Worst case both sides drive the line in opposite directions:** current is limited to (3.3 V − 0 V) / 470 Ω ≈ **7 mA**, well under the RP2350 and ARM I/O limits, and below the RP2350 default drive strength even at the strongest setting. No part is damaged; the externally-connected probe simply wins on the wire because its driver is much stronger and our resistor weakens our side.
- **At normal SWD speeds** (we'll target 1–4 MHz) 470 Ω with the few-pF capacitance of the trace + receiver still gives risetimes well under the SWD bit period.
- Optional: bring the resistors down to **150 Ω** if we want faster SWD, or up to **1 kΩ** if we want the external probe to dominate even more aggressively. 470 Ω is the safe middle.
- Best practice when actually using the TC2030 probe: hold the Fruit Jam in BOOTSEL (its GPIO are tristated), or cut power to the Fruit Jam, or set the SWD GPIO to inputs in software before plugging the probe in. The series resistors are a backstop for the case where someone forgets.

### 3.3 RST_N is open-drain only

The MSPM0 already has a 10 k pull-up (R2) and 0.1 µF cap (C5) on RST_N. Two rules:

- The Fruit Jam GPIO that drives RST_N must be configured **open-drain** in CircuitPython (`digitalio.DigitalInOut(...).switch_to_output(value=True, drive_mode=digitalio.DriveMode.OPEN_DRAIN)`). Drive low to assert reset; release (set high in OD mode = high-Z) to deassert.
- Don't push-pull-drive RST_N high — it will fight C5 every cycle and may glitch the device into a bad reset state.

The series 470 Ω still helps here: if the external probe ever asserts RST_N while we accidentally drive it high, it limits the current.

### 3.4 SWO is optional in v0.2

We won't use SWO for first-light flashing. It's only needed if we later turn on ITM trace or RTT logging. Routing the trace anyway is free and saves a board spin if we change our minds; just leave the Fruit Jam GPIO41 unconnected in software for now.

### 3.5 No on-board regulator on the MSPM0 board

The schematic shows the MSPM0 board accepting 3.3 V directly (no LDO/buck visible). Single-source the 3V3 rail from the Fruit Jam's GPIO header `3V3` pin, never from `5V`. Bench measurement: `SEESAW_RIGHT pin 1` should read 3.30 V ± a few percent under load.

---

## 4. Build order

1. **Mechanical wiring (bench rev 0):** point-to-point with breadboard jumpers — Fruit Jam header to MSPM0 SEESAW headers + the 6 SWD-pad pins. Solder 470 Ω resistors inline on SWCLK/SWDIO/RST_N at the Fruit Jam end.
2. **Power-on smoke test:** measure 3.3 V at SEESAW_RIGHT pin 1. Verify GND continuity.
3. **CircuitPython on Fruit Jam:** flash latest `adafruit-circuitpython-adafruit_fruit_jam-*.uf2`.
4. **Idle-line check:** from CircuitPython, set GPIO6/7 as inputs, GPIO8 as open-drain output high. With a scope or even a multimeter, SWCLK should be floating low (no internal pull either way), SWDIO should float, RST_N should be sitting at 3.3 V (held by the MSPM0's R2 pull-up).
5. **First SWD transaction:** use the upcoming `mspm0_swd.py` (see `fruit_jam.md` §4) to clock out the SWD line-reset sequence and read the MSPM0's IDCODE. Expected value is the ARM Cortex-M0+ DAP IDCODE — TI lists this in the MSPM0G TRM.
6. **Bench probe sanity check:** plug a CMSIS-DAP probe into the J1 TC2030 pad while the Fruit Jam is in BOOTSEL. OpenOCD/pyOCD should see the same IDCODE. (Confirms wiring + contention strategy is fine.)
7. **First flash:** stream firmware bytes through the SWD layer, write through the MSPM0 flash controller, verify by readback. Reset the MSPM0 and start the application I2C scan (`fruit_jam.md` §3.3) — it should now show whatever target address the firmware claims.

---

## 4a. Application I2C address

The MSPM0 firmware (`mspm0_firmware_spec.md`) responds at I2C address **`0x49`**. **Do not use `0x18`** — that's the Fruit Jam's onboard accelerometer and was a major red herring on the prior MSP430 build (`prior_project_summary.md` §5). First Fruit Jam smoke test after every reflash: `i2c.scan()` must show `0x49` along with whatever onboard devices the Fruit Jam has.

---

## 5. Things to design later

- A small stacking PCB or ribbon-cable harness that does all the SEESAW-to-Fruit-Jam plumbing in one connector, including the SWD lines and series resistors.
- A pull-down on D8 / RST_N at the Fruit Jam end (10 kΩ to GND) so that during Fruit Jam reset/BOOTSEL, RST_N on the MSPM0 isn't disturbed at all (the open-drain driver tristates anyway, but a pull-down gives a defined idle state).
- If we later want to drive the MSPM0's analog inputs from the Fruit Jam side, we'd add jumper wires from SEESAW_LEFT pins 1–6 to specific Fruit Jam GPIO. Not in scope for v0.2.

---

## 6. To-be-confirmed before we solder anything permanent

- That `D6 / D7 / D8` come out of the Fruit Jam GPIO 2x16 header in the positions we expect — the canonical pinout image from the Adafruit learn guide should be used to mark up the cable.
- That 470 Ω is the value we want for the SWD series resistors — final value depends on our chosen SWD clock frequency and the production trace lengths. 470 Ω is safe up to roughly 4 MHz on short traces.
- Final RST_N drive-mode behavior in CircuitPython under reset/boot — confirm the Fruit Jam doesn't glitch RST_N during its own startup.
