"""Fruit Jam host-side test client for the MSPM0 seesaw firmware.

Talks over board.STEMMA_I2C() to the MSPM0G3507 seesaw firmware at 0x49
(I2C1 on the MSPM0; see mspm0_firmware_spec.md). Standalone: drop on CIRCUITPY
and run from the REPL with `import seesaw_test` (leaves your code.py untouched).

  import seesaw_test          # runs scan + identity + selftest, then 8s button demo
  seesaw_test.identify()      # just the identity/selftest checks
  seesaw_test.monitor()       # button event loop until Ctrl-C

Wiring note: the board has SDA/SCL crossed for I2C1 -> at the SEESAW_LEFT header
wire host-SDA to pin 10 (PB3) and host-SCL to pin 9 (PB2). See xds110_flashing_setup.md.
"""

import time
import board

ADDR = 0x49

# I2C on the STEMMA QT / hardware I2C bus (SDA=GPIO20, SCL=GPIO21).
#   Fruit Jam STEMMA SDA -> MSPM0 PB3 = chip SDA  (SEESAW_LEFT pin 10)
#   Fruit Jam STEMMA SCL -> MSPM0 PB2 = chip SCL  (SEESAW_LEFT pin 9)
# (D9/D10 are freed for the AAC rotary encoder.)

# --- Seesaw protocol constants (match firmware/mspm0/src/main.c) ---
MOD_STATUS = 0x00
FN_HW_ID, FN_VERSION, FN_OPTIONS, FN_SELFTEST, FN_SWRST = 0x01, 0x02, 0x03, 0x04, 0x7F
MOD_GPIO = 0x01
FN_DIRSET, FN_DIRCLR = 0x02, 0x03
FN_BULK, FN_BULK_SET, FN_BULK_CLR, FN_INTFLAG = 0x04, 0x05, 0x06, 0x0A
MOD_EVENT = 0x80
FN_EVENT_LEN, FN_EVENT_POPALL = 0x01, 0x02

HW_ID_EXPECT = 0x84

_i2c = None


def _bus():
    global _i2c
    if _i2c is None:
        _i2c = board.STEMMA_I2C()
    return _i2c


def ss_read(module, func, nbytes):
    """Seesaw read: write <module,func>, STOP, then read nbytes (big-endian)."""
    i2c = _bus()
    buf = bytearray(nbytes)
    while not i2c.try_lock():
        pass
    try:
        i2c.writeto(ADDR, bytes([module, func]))
        time.sleep(0.005)                 # let the target prepare the response
        i2c.readfrom_into(ADDR, buf)
    finally:
        i2c.unlock()
    return buf


def ss_write(module, func, data=b""):
    i2c = _bus()
    while not i2c.try_lock():
        pass
    try:
        i2c.writeto(ADDR, bytes([module, func]) + bytes(data))
    finally:
        i2c.unlock()


def _be(buf):
    v = 0
    for b in buf:
        v = (v << 8) | b
    return v


def scan():
    i2c = _bus()
    while not i2c.try_lock():
        pass
    try:
        found = i2c.scan()
    finally:
        i2c.unlock()
    print("I2C scan:", [hex(a) for a in found])
    if ADDR in found:
        print("  -> seesaw 0x49 present")
    else:
        print("  !! 0x49 NOT found — check I2C wiring (SDA/SCL crossed) + power")
    return found


def identify():
    """Read identity registers + run the selftest round-trip. Returns True if OK."""
    ok = True

    hw = ss_read(MOD_STATUS, FN_HW_ID, 1)[0]
    print("STATUS.HW_ID   = 0x%02X (expect 0x%02X)" % (hw, HW_ID_EXPECT))
    ok &= hw == HW_ID_EXPECT

    ver = _be(ss_read(MOD_STATUS, FN_VERSION, 4))
    print("STATUS.VERSION = 0x%08X (product=0x%04X date=0x%04X)"
          % (ver, ver >> 16, ver & 0xFFFF))

    opt = _be(ss_read(MOD_STATUS, FN_OPTIONS, 4))
    print("STATUS.OPTIONS = 0x%08X (STATUS=%d GPIO=%d EVENT=%d)"
          % (opt, opt & 1, (opt >> 1) & 1, (opt >> 2) & 1))

    # SELFTEST: read returns 0x55 and arms intflag=0x3F + event [0xFE,0xED]
    st = ss_read(MOD_STATUS, FN_SELFTEST, 1)[0]
    print("STATUS.SELFTEST= 0x%02X (expect 0x55)" % st)
    ok &= st == 0x55

    flags = _be(ss_read(MOD_GPIO, FN_INTFLAG, 4))
    print("GPIO.INTFLAG   = 0x%08X (expect 0x0000003F after selftest)" % flags)
    ok &= flags == 0x3F

    n = ss_read(MOD_EVENT, FN_EVENT_LEN, 1)[0]
    evts = list(ss_read(MOD_EVENT, FN_EVENT_POPALL, n)) if n else []
    print("EVENT depth=%d  POP_ALL=%s (expect [0xFE, 0xED])"
          % (n, [hex(e) for e in evts]))
    ok &= evts[:2] == [0xFE, 0xED]

    print("SELFTEST:", "PASS" if ok else "FAIL")
    return ok


def _pixels():
    try:
        import neopixel
        return neopixel.NeoPixel(board.NEOPIXEL, 5, brightness=0.2, auto_write=True)
    except Exception as exc:               # noqa: BLE001 - optional feedback
        print("(NeoPixel feedback unavailable:", exc, ")")
        return None


def monitor(seconds=None):
    """Poll EVENT.POP_ALL for button presses; light a NeoPixel per button.

    Buttons report ids 0..5. Runs `seconds` (None = until Ctrl-C).
    """
    px = _pixels()
    colors = [(40, 0, 0), (40, 40, 0), (0, 40, 0),
              (0, 40, 40), (0, 0, 40), (40, 0, 40)]
    print("monitoring buttons (press SEESAW buttons 0..5)...")
    end = None if seconds is None else time.monotonic() + seconds
    try:
        while end is None or time.monotonic() < end:
            n = ss_read(MOD_EVENT, FN_EVENT_LEN, 1)[0]
            if n:
                evts = list(ss_read(MOD_EVENT, FN_EVENT_POPALL, n))
                print("buttons:", evts)
                if px is not None:
                    for e in evts:
                        if 0 <= e < 6:
                            px[e % 5] = colors[e]
            time.sleep(0.05)
    except KeyboardInterrupt:
        print("stopped")
    if px is not None:
        px.fill(0)


def _be4(v):
    return bytes([(v >> 24) & 0xFF, (v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF])

# Logical GPIO bit map (matches firmware): buttons 0..5 (inputs),
# spare/host-writable 11..16 (SEESAW_RIGHT). bit -> (name, MSPM0 pin).
GPIO_PINS = [
    (0, "btn0", "PA27"), (1, "btn1", "PA26"), (2, "btn2", "PA25"),
    (3, "btn3", "PA24"), (4, "btn4", "PA22"), (5, "btn5", "PA17"),
    (11, "sp11", "PA18"), (12, "sp12", "PA12"), (13, "sp13", "PA13"),
    (14, "sp14", "PA14"), (15, "sp15", "PA15"), (16, "sp16", "PA16"),
]
SPARE_BITS = (11, 12, 13, 14, 15, 16)


def gpio_test():
    """Exercise every seesaw GPIO over I2C: read all inputs, then for each
    host-writable spare pin drive it high and low and read it back through the
    seesaw. Reports a per-pin PASS/FAIL and flags spurious button activity."""
    print("=== seesaw GPIO test (over I2C) ===")
    bulk = _be(ss_read(MOD_GPIO, FN_BULK, 4))
    print("GPIO BULK = 0x%08X" % bulk)
    for bit, name, pin in GPIO_PINS:
        print("  %-5s %-5s bit%-2d = %d" % (name, pin, bit, (bulk >> bit) & 1))

    print("-- drive test: spare pins as output, drive 1 then 0, read back --")
    for bit in SPARE_BITS:
        m = 1 << bit
        ss_write(MOD_GPIO, FN_DIRSET, _be4(m))      # -> output
        ss_write(MOD_GPIO, FN_BULK_SET, _be4(m))    # drive high
        time.sleep(0.005)
        hi = (_be(ss_read(MOD_GPIO, FN_BULK, 4)) >> bit) & 1
        ss_write(MOD_GPIO, FN_BULK_CLR, _be4(m))    # drive low
        time.sleep(0.005)
        lo = (_be(ss_read(MOD_GPIO, FN_BULK, 4)) >> bit) & 1
        ss_write(MOD_GPIO, FN_DIRCLR, _be4(m))      # -> input (restore)
        ok = hi == 1 and lo == 0
        print("  sp%-2d  drive1->%d  drive0->%d  %s" %
              (bit, hi, lo, "PASS" if ok else "FAIL"))

    fl = _be(ss_read(MOD_GPIO, FN_INTFLAG, 4))
    print("INTFLAG (latched buttons) = 0x%08X" % fl)
    n = ss_read(MOD_EVENT, FN_EVENT_LEN, 1)[0]
    evs = list(ss_read(MOD_EVENT, FN_EVENT_POPALL, n)) if n else []
    print("EVENT depth=%d  %s" % (n, [hex(e) for e in evs]))
    if n:
        print("  !! spurious button events on btn%s — that input is floating/noisy"
              % sorted(set(evs)))
    print("=== gpio_test done ===")


# Fruit Jam pins net-connected to seesaw GPIO (per the carrier board).
LOOPBACK_FJ_PINS = ["D10", "A4", "A5", "D6", "D7"]
ALL_GPIO_MASK = 0x0001F83F          # seesaw logical bits 0..5 and 11..16
_loopback_map = {}                  # FJ pin name -> seesaw logical bit


def _read_bulk():
    return _be(ss_read(MOD_GPIO, FN_BULK, 4))


def loopback_fj_to_seesaw():
    """TEST 1: Fruit Jam drives each pin; seesaw reads it over I2C. Also
    discovers which seesaw logical bit each FJ pin is wired to."""
    import digitalio
    global _loopback_map
    _loopback_map = {}
    print("=== TEST 1: Fruit Jam OUT -> seesaw IN ===")
    ios = {nm: digitalio.DigitalInOut(getattr(board, nm)) for nm in LOOPBACK_FJ_PINS}
    for io in ios.values():
        io.switch_to_input()
    try:
        for nm in LOOPBACK_FJ_PINS:
            io = ios[nm]
            track = ALL_GPIO_MASK
            for _ in range(8):                       # many trials reject floating-pin noise
                io.switch_to_output(value=False)     # drive net LOW
                time.sleep(0.006)
                lo = _read_bulk()
                io.value = True                      # drive net HIGH
                time.sleep(0.006)
                hi = _read_bulk()
                io.switch_to_input()
                track &= (~lo & hi) & ALL_GPIO_MASK  # bits that follow this pin
            bits = [b for b in range(17) if (track >> b) & 1]
            if len(bits) == 1:
                _loopback_map[nm] = bits[0]
                print("  FJ %-3s -> seesaw bit %-2d : PASS" % (nm, bits[0]))
            else:
                print("  FJ %-3s -> %s : FAIL (want exactly 1 tracking bit)" % (nm, bits))
    finally:
        for io in ios.values():
            io.deinit()
    # clear button events generated by the driving
    n = ss_read(MOD_EVENT, FN_EVENT_LEN, 1)[0]
    if n:
        ss_read(MOD_EVENT, FN_EVENT_POPALL, n)
    ss_read(MOD_GPIO, FN_INTFLAG, 4)
    print("  discovered map:", _loopback_map)
    return _loopback_map


def loopback_seesaw_to_fj():
    """TEST 2: seesaw drives each mapped bit (commanded over I2C); Fruit Jam
    reads its pin. Needs firmware with host-writable buttons (bits 0..5)."""
    import digitalio
    if not _loopback_map:
        print("run loopback_fj_to_seesaw() first to discover the mapping"); return
    print("=== TEST 2: seesaw OUT -> Fruit Jam IN ===")
    for nm, bit in _loopback_map.items():
        io = digitalio.DigitalInOut(getattr(board, nm))
        io.switch_to_input()
        m = 1 << bit
        try:
            ss_write(MOD_GPIO, FN_DIRSET, _be4(m))    # seesaw bit -> output
            ss_write(MOD_GPIO, FN_BULK_SET, _be4(m))  # drive HIGH
            time.sleep(0.008)
            rhi = io.value
            ss_write(MOD_GPIO, FN_BULK_CLR, _be4(m))  # drive LOW
            time.sleep(0.008)
            rlo = io.value
            ss_write(MOD_GPIO, FN_DIRCLR, _be4(m))    # restore input
            ok = rhi and not rlo
            print("  seesaw bit %-2d -> FJ %-3s : H->%d L->%d  %s" %
                  (bit, nm, rhi, rlo, "PASS" if ok else "FAIL"))
        finally:
            io.deinit()


def loopback_test():
    loopback_fj_to_seesaw()
    loopback_seesaw_to_fj()


def run():
    print("=== MSPM0 seesaw host test ===")
    if ADDR not in scan():
        return
    identify()
    monitor(seconds=8)
    print("=== done (call seesaw_test.monitor() for a longer button loop) ===")


run()
