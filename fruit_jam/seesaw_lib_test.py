# Test our MSPM0 firmware against the OFFICIAL adafruit_seesaw library (STEMMA QT bus).
import board
from adafruit_seesaw.seesaw import Seesaw

i2c = board.STEMMA_I2C()
print("=== official adafruit_seesaw vs MSPM0 firmware (STEMMA QT) ===")
try:
    ss = Seesaw(i2c, addr=0x49)
    print("Seesaw() OK  HW_ID=0x%02X  version=0x%08X" %
          (ss.read8(0x00, 0x01), ss.get_version()))
except Exception as e:
    print("LIB ERROR:", repr(e))
