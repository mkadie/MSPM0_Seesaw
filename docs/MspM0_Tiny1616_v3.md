# MspM0_Tiny1616 v3 — Schematic Reference

Source: `MspM0_Tiny1616 v3.pdf` (single sheet, dated 4/27/2026, exported from Neutron).

This board is a small carrier for a TI **MSPM0G3507SPTRPT0048A** microcontroller in the 48-pin LQFP (PT) package. The form factor and the two "SEESAW_LEFT / SEESAW_RIGHT" headers strongly imply the layout is meant to drop into an Adafruit Seesaw-style 2x10 footprint, with separate breakouts for I2C, UART, ADC, and a TC2030 SWD programming pad.

---

## 1. Main IC — U2: MSPM0G3507SPTRPT0048A

- Family: TI MSPM0G350x (Cortex-M0+, 80 MHz, CAN-FD, 128 KB flash, 32 KB SRAM).
- Package: 48-pin LQFP, "PT" suffix (`...PT...0048`).
- VCORE: pins 6 and 48, both bypassed.
- VDD rail (3.3 V) bypassed by C3 (10 µF / 10 V), C4 (10 µF), and 0.1 µF caps near the supply pins.
- VSS: pin 7 → GND.
- RST_N: pin 4, pulled up to 3.3 V via R2, with C5 (0.1 µF) to GND. Also routed to J1 pin 1 (programming header VPP/MCLR).

### 1.1 Pin map (as drawn)

| Pin | Signal name on chip | Net on schematic | Notes |
|----:|---------------------|------------------|-------|
| 1   | PA0                 | (not labeled)    | GPIO |
| 2   | PA1                 | (not labeled)    | GPIO |
| 3   | PA28                | (not labeled)    | GPIO |
| 4   | RST_N               | RST_N            | RC reset network, to TC2030 J1.1 |
| 5   | PA31                | (not labeled)    | GPIO |
| 6   | VCORE               | VCORE            | 1 µF decoupling |
| 7   | VSS                 | GND              | Ground |
| 8   | PA2 / ROSC          | —                | |
| 9   | PA3 / LFXIN         | —                | LFXTAL input (unused) |
| 10  | PA4 / LFXOUT        | —                | |
| 11  | PA5 / HFXIN         | —                | HFXTAL input (unused) |
| 12  | PA6 / HFXOUT        | —                | |
| 13  | PA7                 | —                | |
| 14  | PB2                 | **SDA**          | I2C0 SDA — exposed on SEESAW_LEFT pin 9 |
| 15  | PB3                 | **SCL**          | I2C0 SCL — exposed on SEESAW_LEFT pin 10 |
| 16  | PA8                 | **TDX**          | UART TX — to SEESAW_LEFT pin 8 |
| 17  | PA9                 | **RDX**          | UART RX — to SEESAW_LEFT pin 7 |
| 18  | PA10                | **LED_PIN**      | Drives the on-board red LED (LED_FID1) through R3 |
| 19  | PA11                | (not labeled)    | ⚠ See §4 — this is the ROM-BSL UART RX pin |
| 20  | PB6                 | —                | |
| 21  | PB7                 | —                | |
| 22  | PB8                 | —                | |
| 23  | PB9                 | —                | |
| 24  | PB14                | —                | |
| 25  | PB15                | —                | |
| 26  | PB16                | —                | |
| 27  | PA12 / CAN_TX       | "12"             | Header pin → SEESAW_RIGHT 9 |
| 28  | PA13 / CAN_RX       | "13"             | Header pin → SEESAW_RIGHT 8 |
| 29  | PA14                | "14"             | Header pin → SEESAW_RIGHT 5 |
| 30  | PA15 / A1_0         | "15"             | Header pin → SEESAW_RIGHT 4 |
| 31  | PA16 / A1_1         | "16"             | Header pin → SEESAW_RIGHT 3 |
| 32  | PA17 / A1_2         | "5"              | Header pin → SEESAW_LEFT 6 |
| 33  | PA18 / A1_3         | "11"             | Header pin → SEESAW_RIGHT 10 |
| 34  | PA19 / SWDIO        | **SWDIO**        | TC2030 J1.4 |
| 35  | PA20 / SWCLK        | **SWCLK**        | TC2030 J1.5 |
| 36  | PB17 / A1_4         | —                | |
| 37  | PB18 / A1_5         | —                | |
| 38  | PB19 / A1_6         | —                | |
| 39  | PA21 / A1_7 / VREF- | **SWO**          | TC2030 J1.6 (LVP / SWO) |
| 40  | PA22 / A0_7         | "4"              | Header pin → SEESAW_LEFT 5 |
| 41  | PB20 / A0_6         | —                | |
| 42  | PB24 / A0_5         | —                | |
| 43  | PA23 / VREF+        | —                | |
| 44  | PA24 / A0_3         | "3"              | Header pin → SEESAW_LEFT 4 |
| 45  | PA25 / A0_2         | "2"              | Header pin → SEESAW_LEFT 3 |
| 46  | PA26 / A0_1 / CAN_TX| "1"              | Header pin → SEESAW_LEFT 2 |
| 47  | PA27 / A0_0 / CAN_RX| "0"              | Header pin → SEESAW_LEFT 1 |
| 48  | VCORE               | VCORE            | Same rail as pin 6 |

> "TDX/RDX" in the source schematic appears to be a stylization of TX/RX for the UART data lines.

---

## 2. Connectors

### 2.1 SEESAW_LEFT (10-pin header, marked **DNP** — Do Not Populate)

| Header pin | Net      | Goes to MCU pin | Notes |
|-----------:|----------|-----------------|-------|
| 1          | "0"      | PA27 (47)       | A0_0 / CAN_RX |
| 2          | "1"      | PA26 (46)       | A0_1 / CAN_TX |
| 3          | "2"      | PA25 (45)       | A0_2 |
| 4          | "3"      | PA24 (44)       | A0_3 |
| 5          | "4"      | PA22 (40)       | A0_7 |
| 6          | "5"      | PA17 (32)       | A1_2 |
| 7          | RDX (RX) | PA9 (17)        | UART RX |
| 8          | TDX (TX) | PA8 (16)        | UART TX |
| 9          | SDA      | PB2 (14)        | I2C SDA — pulled up to 3.3 V via R4 |
| 10         | SCL      | PB3 (15)        | I2C SCL — pulled up to 3.3 V via R1 |

### 2.2 SEESAW_RIGHT (10-pin header, marked **DNP**)

| Header pin | Net      | Goes to MCU pin | Notes |
|-----------:|----------|-----------------|-------|
| 1          | 3.3V     | —               | Power input/output (board 3.3 V rail) |
| 2          | GND      | —               | Ground |
| 3          | "16"     | PA16 (31)       | A1_1 |
| 4          | "15"     | PA15 (30)       | A1_0 |
| 5          | "14"     | PA14 (29)       | GPIO |
| 6          | (open)   | —               | |
| 7          | (open)   | —               | |
| 8          | "13"     | PA13 (28)       | CAN_RX |
| 9          | "12"     | PA12 (27)       | CAN_TX |
| 10         | "11"     | PA18 (33)       | A1_3 |

### 2.3 J1 — TC2030 (Tag-Connect 6-pin, Microchip-style ICSP/SWD pinout)

| TC2030 pin | Function     | Net     | MCU pin |
|-----------:|--------------|---------|---------|
| 1          | VPP/MCLR     | RST_N   | 4       |
| 2          | VDD          | 3.3V    | —       |
| 3          | VSS          | GND     | —       |
| 4          | ICSP_DAT/PGD | SWDIO   | PA19 (34) |
| 5          | ICSP_CLK/PGC | SWCLK   | PA20 (35) |
| 6          | LVP          | SWO     | PA21 (39) |

> Note: the silk uses Microchip ICSP nomenclature, but the MCU is TI ARM. Pins 4/5 carry SWDIO/SWCLK and pin 6 carries SWO — this is a standard CMSIS-DAP / J-Link SWD connection through a Tag-Connect TC2030 cable.

---

## 3. Passives & indicators

| Ref | Function |
|-----|----------|
| C1  | 0.1 µF — VCORE bulk near pin 48 |
| C2  | 0.1 µF — VDD decoupling |
| C3  | 1 µF / 10 V — VCORE bulk near pin 6 |
| C4  | 10 µF — bulk on 3.3 V rail (input side) |
| C5  | 0.1 µF — RST_N filter cap |
| R1  | I2C SCL pull-up to 3.3 V |
| R4  | I2C SDA pull-up to 3.3 V |
| R2  | RST_N pull-up to 3.3 V |
| R3  | Series resistor for LED_FID1 (red LED) on LED_PIN/PA10 |
| LED_FID1 | Red status LED, anode through R3 to 3.3 V, cathode to PA10 (active-low) |

---

## 4. Programming and BSL notes

The MSPM0G3507 ROM BSL (bootloader) supports UART, I2C, SPI, and (on G3507) CAN. Two of those are already exposed on SEESAW_LEFT:

- **I2C BSL** — default I2C target address `0x48`. Lines: PB2/SDA, PB3/SCL → SEESAW_LEFT pins 9/10.
- **UART BSL** — ROM default lives on **PA10 / PA11**. On this board PA10 is dual-purposed as LED_PIN, and PA11 has no external header pad. So UART BSL on the ROM-default pins will drive the LED while data is moving and will be hard to wire up cleanly. The PA8/PA9 UART exposed on SEESAW_LEFT 7/8 is the *application* UART, **not** the ROM-default BSL UART.
- **SWD** — always available via the TC2030 J1 pad. This is the safe path for first-time programming and recovery.

To invoke the BSL the chip needs an NRST cycle while the BSL-invoke condition is met (default condition is set in NONMAIN; if NONMAIN has not been customized, the device falls into BSL when the application is blank/invalid).

NONMAIN's `BSLPINCFG0` register can be reflashed (once, via SWD) to relocate the BSL UART to PA8/PA9 if we want UART BSL through the SEESAW_LEFT header in the future.

---

## 5. Quick reference for inter-board wiring

The minimum wires needed to talk to this board over I2C from a host (e.g. Fruit Jam) are:

- SEESAW_RIGHT 1 → host 3.3 V
- SEESAW_RIGHT 2 → host GND
- SEESAW_LEFT 9 (SDA) → host SDA
- SEESAW_LEFT 10 (SCL) → host SCL

Pull-ups (R1, R4) are already on the MSPM0 board, so the host should not also enable strong pull-ups on the same lines.

For SWD programming there are two supported paths, both terminating on the same SWDIO/SWCLK/RST_N nets:

- **Permanent / production:** SWDIO/SWCLK/RST_N tap directly off the chip and run to the Fruit Jam GPIO header (D7/D6/D8) through 470 Ω series resistors. The Fruit Jam programs the chip from CircuitPython using a PIO-based SWD engine. See `wiring_diagram.md`.
- **Bench / emergency:** plug a Tag-Connect TC2030-IDC cable into J1 → external CMSIS-DAP / J-Link / XDS110 probe. While doing this, hold the Fruit Jam in BOOTSEL (or remove its power) so its GPIO are tristated and don't fight the probe.

The two paths share the same wire — the 470 Ω series resistors are what make this safe.
