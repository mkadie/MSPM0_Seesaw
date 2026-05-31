# MSPM0G3507 Seesaw Firmware Spec — `MspM0_Tiny1616 v3`

This is the spec for the firmware that will live on the MSPM0G3507 chip on our `MspM0_Tiny1616 v3` board. It ports the existing MSP430FR2311 Seesaw slave (see `prior_project_summary.md`) onto the new chip and the new pinout, and tightens the parts that gave us trouble before.

The firmware will be built in C using TI's MSPM0-SDK (DriverLib + ARM Cortex-M0+ GCC), output as a `.bin`, and flashed onto the chip over SWD by the Fruit Jam (see `fruit_jam.md` §4). No PC tool is in the loop at runtime.

---

## 0. Implementation corrections (resolved at bring-up, 2026-05-29)

Verified against SysConfig device data (`MSPM0G350X.json`) + the v3 schematic while
writing the firmware. The implemented code (`firmware/mspm0/src/`) follows these, not
the original §2/§3 assumptions:

1. **I2C is `I2C1`, not `I2C0`.** PB2/PB3 have no I2C0 mux at all. PB2 = **I2C1.SCL**
   (PINCM15, PF4), PB3 = **I2C1.SDA** (PINCM16, PF4). I2C1 base `0x400F2000`, IRQ vec 41.
2. **SDA/SCL are crossed on the board.** The schematic labels PB2's net "SDA" and PB3's
   "SCL", but the silicon forces SCL=PB2 / SDA=PB3. Since SEESAW_LEFT is DNP/hand-wired,
   fix it at the cable: **host-SDA → SEESAW_LEFT pin 10 (PB3)**, **host-SCL → pin 9 (PB2)**
   — opposite the silk. Firmware is correct for the silicon regardless.
3. **Debug UART is `UART1`** (PA8=TX/PINCM19/PF2, PA9=RX/PINCM20/PF2), not UART0.
4. Build is bare-metal against the SDK `hw_*.h` register structs (no DriverLib), flashed
   with the EXP432 XDS110 via `firmware/mspm0/flash.sh` (see `xds110_flashing_setup.md`),
   not the Fruit Jam PIO SWD. Image must be 8-byte aligned (`flash.sh` pads it).

On-chip init verified over SWD after flashing: I2C1 SOAR=0x49, SCTR=0x09,
UART1 IBRD=17/FBRD=23, GPIOA LED output enabled.

### Bring-up resolved (2026-05-30) — full Seesaw round-trip PASSES

5. **`SOAR.OAREN` is mandatory.** Writing the address to `SOAR` does nothing without
   `I2C_SOAR_OAREN_ENABLE` (bit 14) — the target won't ACK its address. Also select the
   I2C functional clock (`CLKSEL=BUSCLK`, `CLKDIV=/1`) and enable `SCTR.SCLKSTRETCH`.
   This was the last firmware bug; once fixed the host scan immediately found `0x49`.
6. **Host I2C = Fruit Jam STEMMA QT / hardware I2C (SDA=GPIO20, SCL=GPIO21)**, via
   `board.STEMMA_I2C()`. Wire: **STEMMA SDA → SEESAW_LEFT pin 10 (PB3 = chip SDA)**,
   **STEMMA SCL → pin 9 (PB2 = chip SCL)**, common GND. The seesaw coexists with the
   on-board accelerometer (0x18) on this bus (scan shows 0x18 + 0x49).
   - During bring-up the wires were briefly on D9/D10 with `bitbangio` (those pins can't
     form a hardware I2C pair); a GPIO-level connectivity test
     (`firmware/mspm0/src/pintest_in.c`/`pintest_out.c`, reading MSPM0 pins over SWD)
     mapped D9↔PB3, D10↔PB2 and proved wires+ground before I2C worked. Moved to STEMMA QT
     because **D9/D10 collide with the AAC app's rotary encoder**
     (`hardware_config.py`: encoder on D9/D10) — STEMMA QT frees them and gives hardware I2C.
7. **Verified against the official `adafruit_seesaw` CircuitPython library**: `Seesaw(i2c,
   addr=0x49)` constructs OK (HW_ID 0x84 is the recognized ATtiny806 id), and STATUS/GPIO
   reads work. Our `EVENT` module (0x80) is a custom extension on top.

Final result from `fruit_jam/seesaw_test.py`: scan=0x49, HW_ID=0x84, VERSION=0x00020100,
OPTIONS=0x07, SELFTEST=0x55, INTFLAG=0x3F, EVENT POP_ALL=[0xFE,0xED]. **SELFTEST: PASS.**

### GPIO + bidirectional FJ↔seesaw loopback (validated)

- `HOST_WRITABLE_MASK` widened to `0x1F83F` (logical bits 0..5 **and** 11..16) so the host
  can configure the button pins as outputs too — standard Seesaw behaviour (DIRSET/DIRCLR).
- `seesaw_test.gpio_test()` exercises every spare pin (drive H/L, read back) — all PASS.
- `seesaw_test.loopback_test()` runs the two-way carrier-board test and auto-discovers the
  Fruit-Jam-pin ↔ seesaw-logical-bit mapping. **Both directions PASS** for all 5 nets:

  | Fruit Jam pin | seesaw bit | MSPM0 pin | role |
  |---|---|---|---|
  | D10 | 14 | PA14 | spare |
  | A4  | 1  | PA26 | button 1 |
  | A5  | 2  | PA25 | button 2 |
  | D6  | 3  | PA24 | button 3 |
  | D7  | 4  | PA22 | button 4 |

  Test 1 = FJ drives / seesaw reads over I2C; Test 2 = seesaw drives over I2C / FJ reads.
  (Note: D10 lands on a spare pin, not a button.)

### Bring-up resolved (2026-05-30) — 8 buttons, BTN1/A4 re-enabled, power conflict found

8. **Eight buttons now (0..7).** Buttons 6/7 were added on **PA9/PA8** (PINCM20/19), which
   meant **dropping the debug UART** (it lived on those pins). `BTN_MASK` and the IRQ
   polarity setup cover bits 0..7; `HOST_WRITABLE_MASK` widened to `0x0001F8FF` (logical
   bits 0..7 and 11..16).
9. **Button 1 (PA26 = A4) is now a real button.** It had been *excluded* (input, no pull,
   no IRQ) because on this board A4 doubled as **FULL_POWER**, held low by an external
   pull-down — a pull-up would have fought it. FULL_POWER is now controlled elsewhere and
   A4 is just an input, so BTN1 was restored to the normal button treatment (internal
   pull-up + falling-edge IRQ, in `BTN_MASK`). Confirmed: pressing the A4 switch emits a
   `SEESAW button 1` event, and the Fruit-Jam-side A4 monitor sees the same net toggle.
   *Caveat:* the old external A4 pull-down must be removed, or it fights the pull-ups and
   pins A4 low (BTN1 reads stuck-pressed).
10. **Seesaw vs. LCD share the switched 3V3 rail — power, not wiring.** The biggest
    bring-up red herring: with the ILI9341 LCD plugged in, `scan` showed only `0x18` and
    never `0x49`. Root cause is **power**: the LCD and the seesaw both hang off the
    FULL_POWER / 3V3_SWITCHED rail (fed via the hardwired pull-down), and the LCD's draw
    **browns out the seesaw** so it never enumerates. Unplug the LCD (or power the seesaw
    independently) → red LED on, `scan = ['0x18','0x49']`, `HW_ID = 0x84` rock-solid on
    every pass. **The firmware was fine the whole time.** Next board must give the seesaw
    its own always-on 3V3 (see §8).
11. **Host-side test mode** (`fruit_jam/button_test.py`, gated by `test_mode = true` in the
    AAC `config.txt`): shows seesaw EVENT presses, the Fruit Jam onboard buttons, and a
    live **A4 line monitor** on the display + REPL — the working bring-up/diagnostic tool.

---

## 1. Goals

1. Be a drop-in **Seesaw I2C peripheral** that the Fruit Jam can talk to with the same module/function command structure used in `prior_project_summary.md` §1. STATUS and GPIO modules at minimum.
2. Implement the original AAC button-expander spec's behavior that we couldn't fit on the FR2311: an **ordered, 16-deep buffer of button events** in addition to the bitfield latch.
3. Use only the I/O that the `MspM0_Tiny1616 v3` board actually breaks out (the SEESAW_LEFT and SEESAW_RIGHT headers, and the on-board LED on PA10).
4. Be recoverable from a host abort mid-transaction without a hardware reset.
5. Ship in <16 KB so we have headroom in the MSPM0G3507's 128 KB flash.

---

## 2. Identity and addresses

| Item                | Value             | Notes |
|---------------------|-------------------|-------|
| I2C target address  | **`0x49`**        | Same as Adafruit Seesaw default. **Not** `0x18` (Fruit Jam accelerometer). |
| `STATUS.HW_ID`      | **`0x84`**        | New value to identify "MSPM0G3507-as-seesaw"; distinct from `0xF0` used on FR2311. |
| `STATUS.VERSION`    | `0x00020100`      | `PRODUCT_ID = 0x0002`, `DATE_CODE = 0x0100` — bumps from the FR2311's `0x00010228`. |
| `STATUS.OPTIONS`    | `0x00000007`      | bit0 STATUS, bit1 GPIO, bit2 EVENT_BUFFER (new). |
| `STATUS.SELFTEST`   | returns `0x55`    | Side effect: arms `gpio_intflag = SELFTEST_PATTERN` and pushes a self-test marker into the event buffer. |
| `STATUS.SWRST`      | with `0xFF` data  | `SCB.AIRCR = 0x05FA0004` to trigger system reset. |

The address must be confirmed by an I2C scan from the Fruit Jam at first power-up (lessons learned §1 in `prior_project_summary.md`).

---

## 3. Pin assignment on `MspM0_Tiny1616 v3`

All pin numbers below are MSPM0G3507 pins; the SEESAW header mapping is from `MspM0_Tiny1616_v3.md`.

| Function          | MSPM0 pin         | Net          | Available on header        | Notes |
|-------------------|-------------------|--------------|----------------------------|-------|
| I2C0 SDA          | PB2 (pin 14)      | `SDA`        | SEESAW_LEFT pin 9          | PB2 is I2C0_SDA in the IOMUX |
| I2C0 SCL          | PB3 (pin 15)      | `SCL`        | SEESAW_LEFT pin 10         | PB3 is I2C0_SCL in the IOMUX |
| UART debug TX     | PA8 (pin 16)      | `TDX`        | SEESAW_LEFT pin 8          | UART0_TX or UART1_TX (alt fn TBD) |
| UART debug RX     | PA9 (pin 17)      | `RDX`        | SEESAW_LEFT pin 7          | matching UART RX |
| LED1              | PA10 (pin 18)     | `LED_PIN`    | (on-board LED only)        | active-low: drive 0 to light, 1 to extinguish |
| Button "0"        | PA27 (pin 47)     | `"0"`        | SEESAW_LEFT pin 1          | input + pull-up + falling-edge IRQ |
| Button "1"        | PA26 (pin 46)     | `"1"`        | SEESAW_LEFT pin 2          | (also CAN_TX alt) |
| Button "2"        | PA25 (pin 45)     | `"2"`        | SEESAW_LEFT pin 3          | |
| Button "3"        | PA24 (pin 44)     | `"3"`        | SEESAW_LEFT pin 4          | |
| Button "4"        | PA22 (pin 40)     | `"4"`        | SEESAW_LEFT pin 5          | |
| Button "5"        | PA17 (pin 32)     | `"5"`        | SEESAW_LEFT pin 6          | |
| Spare GPIO "11"   | PA18 (pin 33)     | `"11"`       | SEESAW_RIGHT pin 10        | host-controllable |
| Spare GPIO "12"   | PA12 (pin 27)     | `"12"`       | SEESAW_RIGHT pin 9         | |
| Spare GPIO "13"   | PA13 (pin 28)     | `"13"`       | SEESAW_RIGHT pin 8         | |
| Spare GPIO "14"   | PA14 (pin 29)     | `"14"`       | SEESAW_RIGHT pin 5         | |
| Spare GPIO "15"   | PA15 (pin 30)     | `"15"`       | SEESAW_RIGHT pin 4         | |
| Spare GPIO "16"   | PA16 (pin 31)     | `"16"`       | SEESAW_RIGHT pin 3         | |

> Buttons "0".."5" are nominally physical buttons in the AAC application; "11".."16" are spare GPIO that the host can drive in either direction. There is **no LED2 on this board** (the schematic only has LED_FID1 = LED1). LED2-style behavior from the original spec collapses into LED1 timing.

### 3.1 Protected pins (firmware must refuse host writes)

| Pin                 | Why                                  |
|---------------------|--------------------------------------|
| PB2 (SDA)           | reserved for I2C0                    |
| PB3 (SCL)           | reserved for I2C0                    |
| PA8 (TDX)           | reserved for debug UART              |
| PA9 (RDX)           | reserved for debug UART              |
| PA10 (LED1)         | reserved for firmware LED control    |
| PA19 (SWDIO)        | SWD — debug only                     |
| PA20 (SWCLK)        | SWD — debug only                     |
| PA21 (SWO)          | SWD trace — debug only               |

The Seesaw `GPIO_DIRSET / DIRCLR / BULK_SET / BULK_CLR / PULLEN*` paths must mask out these pins before applying changes.

---

## 4. Behavior

### 4.1 Boot

1. SYSCTL: bring up `MCLK` from the internal oscillator at **32 MHz** (default after reset is fine; explicitly select to be safe).
2. Configure all GPIOA/GPIOB pins to known states. Buttons → input with pull-up. Spare GPIO → input with no pull. LED1 → output high (extinguished).
3. Configure I2C0 in target mode at address 0x49, 100/400 kHz tolerated (let the host pick the clock).
4. Configure UART (PA8/PA9) at 115200 baud for debug prints. (Optional but useful during bring-up.)
5. Enable interrupts (`__enable_irq()`).
6. Print a banner over UART: `"=== MSPM0 seesaw addr=0x49 buttons=0..5 ===\r\n"`.
7. Drop into a `WFI` idle loop in main.

### 4.2 Button handling

- All six button GPIOs share a single GPIOA group interrupt.
- ISR reads `GPIOA.RIS` (raw interrupt status) masked to the button set, clears via `GPIOA.ICLR`.
- For each transitioned bit:
  - Set the bit in `gpio_intflag` (32-bit latch, same shape as Seesaw expects).
  - Push the **pin number** (0..5 in human terms) into the FIFO event buffer if there's room. Drop the event if the FIFO is full and increment a "dropped" counter that the host can read via a future `STATUS.OVERRUN` register.
- LED1 on (drive PA10 low) on any latched button.

### 4.3 Event buffer (new vs. FR2311)

- 16-deep ring buffer of `uint8_t`, holding pin numbers (`0..5`).
- A new Seesaw module **`EVENT` (id `0x80`)** with two functions:
  - `EVENT_LEN` (`0x01`) — read returns 1 byte, the current depth.
  - `EVENT_POP_ALL` (`0x02`) — read returns up to 16 bytes (the entire buffer in order). Side effect: empties the buffer and turns off LED1.
- For backward compatibility, `GPIO.INTFLAG` continues to return the latched bitfield and atomically clear it. After clear, LED1 is also turned off (matches the FR2311 "STOP turns LED1 off" behavior, but tied to the read instead of the STOP condition).

### 4.4 Recovering from host abort

The FR2311 firmware would hang clock-stretching after a `Ctrl-C`. To avoid that on the MSPM0:

- The I2C target ISR always pre-fills the next TX byte from the response buffer (or 0x00 if past length) — it never blocks waiting for a buffer write.
- A STOP condition unconditionally resets the per-transaction state machine: clears `rx_idx`, resets `tx_idx`, and discards any in-flight response.
- A timeout in the ISR (configurable, default 50 ms) on the START-without-STOP case forces a state reset. (The MSPM0 I2C peripheral has built-in timeout support — use it.)

### 4.5 Self-test

`STATUS.SELFTEST` does three things, atomic from the host's perspective:

1. Sets `gpio_intflag = 0x0000003F` (bits 0..5, the six buttons).
2. Pushes the marker sequence `0xFE 0xED` into the event buffer.
3. Returns `0x55` to the host as the immediate response.

The host validates by then reading `STATUS.HW_ID` (still `0x84`), `GPIO.INTFLAG` (must be `0x0000003F`), and `EVENT.POP_ALL` (must contain `0xFE 0xED` ahead of any real button events).

---

## 5. Peripheral mapping (MSP430 → MSPM0)

This is the porting cheat-sheet — for everything in the FR2311 firmware, what's the MSPM0 equivalent?

| MSP430FR2311 thing            | MSPM0G3507 equivalent                                         |
|-------------------------------|---------------------------------------------------------------|
| `WDTCTL = WDTPW \| WDTHOLD`   | `SYSCTL.WDTCTL` clear-enable bit + DriverLib `DL_WWDT_disable` |
| Clock: `CSCTL*` + DCO + FLL    | `SYSCTL.MCLKCFG` — pick MFCLK or HFCLK; default is fine       |
| `__bis_SR_register(LPM0_bits)` | `__WFI()` (Cortex-M0+ wait-for-interrupt)                     |
| eUSCI_B0 in I2C target mode    | I2C0 in target mode (`DL_I2C_*`); IOMUX PB2/PB3 to I2C0       |
| eUSCI_A0 UART backchannel      | UART0 or UART1 on PA8/PA9                                     |
| `P1IE / P1IES / P1IFG`         | GPIOA group IRQ; `DL_GPIO_setLowerPinsPolarity`, `DL_GPIO_clearInterruptStatus`, `DL_GPIO_getEnabledInterruptStatus` |
| `P1OUT |= LED1`                | `DL_GPIO_clearPins(GPIOA, GPIO_LED1_PIN)` (active-low)        |
| `P1IN`                          | `DL_GPIO_readPins(GPIOA, mask)`                                |
| `__attribute__((interrupt(EUSCI_B0_VECTOR)))` | `void I2C0_IRQHandler(void)` declared in the vector table |
| `__attribute__((interrupt(PORT1_VECTOR)))`    | `void GPIOA_IRQHandler(void)` (or GROUP1)                  |
| Reset via `WDTCTL = 0`         | `NVIC_SystemReset()` from the CMSIS header                    |

> The MSPM0-SDK function names above (`DL_*`) are TI's DriverLib wrappers. We can also bare-metal poke the registers if we want a smaller binary; either is fine.

---

## 6. Build and flash workflow

```
docs/                                         (this folder)
firmware/mspm0/
  src/main.c            ← the C we'll write next
  src/seesaw.c
  src/seesaw.h
  src/buttons.c
  src/buttons.h
  Makefile              ← arm-none-eabi-gcc + linker script + tools
  build/firmware.bin    ← what we ship to the Fruit Jam to flash
```

1. **Compile** with the GNU ARM Embedded toolchain (`arm-none-eabi-gcc -mcpu=cortex-m0plus -mthumb -Os ...`).
2. **Link** against the MSPM0G3507 linker script from the MSPM0-SDK (or a hand-written one — flash at `0x0`, RAM at `0x20200000`).
3. **Output** as `firmware.bin` (raw binary). The TI toolchain produces `.out` (ELF) by default; convert with `arm-none-eabi-objcopy -O binary firmware.elf firmware.bin`.
4. **Copy** `firmware.bin` to the Fruit Jam's `CIRCUITPY` drive.
5. **Run** the Fruit Jam loader (`mspm0_loader.py`) which calls into the SWD stack and writes the binary into MSPM0 flash.
6. **Reset** the MSPM0 via SWD (`SCB.AIRCR = 0x05FA0004`) and watch the UART banner come up.
7. **Smoke test** with an I2C scan from CircuitPython — must show `0x49`. Then read `STATUS.HW_ID`, expect `0x84`.

---

## 7. Open implementation questions

1. **MSPM0G3507 ARM DAP IDCODE** — pin the exact value from the MSPM0G TRM so the SWD stack has a known-good first read.
2. **MSPM0 I2C address-match latency** — confirm whether the I2C0 target peripheral can ACK quickly enough at 400 kHz from a `WFI` idle. If not, drop to 100 kHz or stay out of `WFI` while a transaction might be inbound.
3. **NONMAIN** — do we need to touch NONMAIN at all for this app? Probably not (default settings give us SWD access and a stock BSL that we don't need). Confirm before shipping.
4. **Linker script** — write our own minimal one or pull from MSPM0-SDK. Decide based on toolchain we settle on.
5. **CAN-FD** — the FR2311 had no CAN. The MSPM0G3507 does, and the schematic exposes CAN_TX (PA12/PA26) / CAN_RX (PA13/PA27) on the SEESAW headers. Out of scope for v1 but worth flagging — the spare GPIO labels "12"/"13" on SEESAW_RIGHT 8/9 can be repurposed as a CAN bus later without a board spin.

---

## 8. Next version — runtime-configurable, ATtiny1616-Seesaw-style

**Design goal for the next firmware/PCB revision: stop hard-coding the pin roles.**
This bring-up showed how much friction comes from baking choices into the firmware —
which pins are buttons vs. spare, whether a pin has a pull-up, that A4 was FULL_POWER and
therefore *not* a button (had to be re-flashed when that changed), etc. Every such change
meant editing C and re-flashing over SWD.

The next version should be **configurable at runtime over I2C, the way Adafruit's
ATtiny1616 Seesaw is** — the host (CircuitPython on the Fruit Jam) decides each pin's role
and changes it live, with no re-flash. Concretely:

1. **Every GPIO host-configurable via the standard Seesaw GPIO module.** Direction
   (`DIRSET`/`DIRCLR`), pulls (`PULLENSET`/`PULLENCLR` + `BULK_SET`/`BULK_CLR` to pick
   up/down), and interrupt enable (`INTENSET`/`INTENCLR`) for *any* pin — not a fixed
   "buttons 0..5/0..7" set. A pin becomes "a button" simply because the host enabled its
   edge interrupt; it becomes an output because the host set its direction. Mirrors the
   ATtiny1616 Seesaw GPIO API so the stock `adafruit_seesaw` library Just Works.
2. **No firmware-side privileged pins beyond the truly fixed ones** (I2C SDA/SCL, SWD,
   the on-board LED). Drop the hard-coded `BTN_MASK` / `HOST_WRITABLE_MASK` button list;
   replace with a host-writable per-pin config so cases like "A4 is FULL_POWER, not a
   button" or "A4 is now a button" are a CircuitPython config change, never a re-flash.
3. **Persistable profile (optional).** Let the host push a pin-role profile that the
   firmware can optionally save (NONMAIN/flash) and reload on boot, so a given assistive
   build comes up in its configured state without the host re-applying it every power-on.
4. **Why this matters — special-needs cases.** Each AAC / assistive user can need a
   different switch layout: different counts of buttons, different active-low vs.
   active-high switches, latching vs. momentary, some pins as outputs driving indicator
   LEDs, debounce/long-press timing, etc. Making all of that **CircuitPython-settable at
   runtime** means one firmware image adapts to every user's hardware and needs — the
   support person configures it from the Fruit Jam side instead of rebuilding firmware.
5. **Carry over the good parts of v1:** keep the custom `EVENT` FIFO module (0x80) for
   ordered press history, the host-abort-safe I2C target ISR, and `HW_ID = 0x84`. Add
   host-settable debounce and optional per-pin event-vs-latch behaviour.
6. **Hardware prerequisite (from §0.10):** give the seesaw its own **always-on 3V3** rail,
   independent of the LCD's switched rail, so it's always enumerable regardless of display
   state. Runtime config is useless if the chip browns out.
