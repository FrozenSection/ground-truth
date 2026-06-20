# Ground Truth — Wiring

Everything stacks on / wires through the **eInk Feather Friend**, which sits on the
**Adafruit ESP32 Feather V2** and passes the Feather's GPIOs through to its own pads.
GPIOs are the single source of truth — see `include/config.h`.

## Already in place

**E-paper (4.2" GDEY042T81)** — via the Feather Friend's SPI + control pads:
| Function | GPIO | Note |
|---|---|---|
| SCK | 5 | hardware SPI (shared) |
| MOSI | 19 | hardware SPI (shared) |
| EPD CS | 15 | Friend ECS |
| EPD DC | 33 | Friend DC |
| EPD BUSY | 27 | **hand-wired** Friend BUSY pad → the D11 header (GPIO27) |
| SRAM CS | 32 | unused, held HIGH |
| SD CS | 14 | unused, held HIGH |

**Smart button** — panel-mount momentary: one leg → **GPIO 26 (A0)**, other leg → **GND**
(`INPUT_PULLUP`, pressed = LOW).

## Ethernet — Adafruit WIZ5500 breakout (#6348), Gate 1b

Plain SPI W5500 (its "co-processor" = the chip's built-in TCP/IP stack), level-shifted,
3–5 V tolerant. Shares the e-paper SPI bus and adds its own CS + MISO + RST + IRQ.

| W5500 pad | Wire colour | → Feather/Friend pin | GPIO |
|---|---|---|---|
| **VIN** | Red | **3V** | — |
| **GND** | Black | GND | — |
| **SCK** | Yellow | SCK | 5 |
| **MOSI** | Blue | MO / MOSI | 19 |
| **MISO** | Green | MI / MISO | 21 |
| **CS** | White | A5 | 4 |
| **IRQ** | Yellow ‡ | A1 | 25 |
| **RST** | Blue ‡ | SDA | 22 |
| 3.3V | — | *leave open* (regulator output) | — |

‡ Colour reused: **Yellow** = SCK + IRQ, **Blue** = MOSI + RST. Each pair has one wire on
the SPI header and one on the analog/SDA side, so they're never adjacent. Only **Red
(power)** and **Black (ground)** must be unique — a mis-traced *signal* colour just fails
to link (debuggable), it can't damage anything.

### Critical: VIN = 3 V, not 5 V
The breakout level-shifts its SPI lines to whatever **VIN** is. Powered at 5 V it would
drive **MISO into the ESP32 at 5 V and destroy the pin.** Use the Feather's **3V** pad
(matches logic level). The W5500 draws ~130 mA from the 3.3 V regulator (500 mA peak) —
fine on USB power, especially since WiFi can idle when Ethernet is the active link.

### Notes
- **Shared SPI bus:** SCK/MOSI/MISO are common with the e-paper; only the chip-selects
  differ (EPD = 15, W5500 = 4). The e-paper never reads MISO, so no contention. Firmware
  asserts one CS at a time.
- **Pins consumed:** A5 (4), A1 (25), SDA (22) — none used elsewhere (no I²C / analog on
  this build). Avoided the strapping pins (0/2/12/15; 5 & 15 are already in safe use).
- **RST / IRQ are optional.** Drop both → a clean 6-wire harness (one unique colour each),
  firmware uses `-1` for those. Recommended to keep both: IRQ enables interrupt mode
  (safer bring-up), RST lets firmware recover a wedged chip without a power cycle.
- **Firmware (Gate 1b):** arduino-esp32 `ETH` over SPI — `ETH_PHY_W5500`, CS = 4, IRQ = 25,
  RST = 22, on the shared SPI bus (SCK 5 / MISO 21 / MOSI 19). `online = WiFi || Ethernet`;
  both MACs exposed. Bench-validate SPI sharing with the e-paper before sealing.
