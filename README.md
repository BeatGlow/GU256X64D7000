# GU256X64D7000 Arduino library

Drive the Noritake GU256X64D-7000 over its 8-bit parallel interface from
an Arduino board with native 5 V GPIO on all 11 VFD signals.

## Install

Copy this `GU256X64D7000` folder into the Arduino sketchbook's
`libraries` directory. To redistribute it, zip the folder with
`library.properties`, `src`, `examples`, and the license inside it.
Arduino IDE can install that ZIP using **Sketch > Include Library > Add
.ZIP Library**.

Arduino IDE lists these sketches under the library's examples.

- [HelloVFD](examples/HelloVFD/HelloVFD.ino) clears the display and writes
  built-in text. It includes alternate wiring for the Uno WiFi Rev2.
- [BitmapVFD](examples/BitmapVFD/BitmapVFD.ino) draws a centered 16×16 bitmap.

Both examples select direct port wiring on the Nano Every and Uno WiFi
Rev2. The portable examples use Uno R3, Mega 2560, Uno R4 Minima, or
SparkFun Pro Micro 5 V / 16 MHz pins.

For PlatformIO, choose `framework = arduino` and add the repository to
your project's `lib_deps`.

```ini
lib_deps =
    https://github.com/BeatGlow/GU256X64D7000.git
```

## Common operations

Call `begin()` to configure the pins, then `initialize()` to pulse
`/RESET`, restore controller defaults, and clear the display.

| Operation | Method |
| --- | --- |
| Restore the controller's defaults without pulsing /RESET | `restoreDefaults()` |
| Clear the current window and move the cursor home | `clear()` |
| Move the cursor home or set its position | `home()`, `setCursor(x, band)` |
| Write printable ASCII in the built-in font | `writeText(text)`, `drawTextAt(x, band, text)` |
| Draw a complete monochrome rectangle | `drawBitmap(x, band, columns, bands, data)` |
| Set brightness from 1 (dimmest) to 8 (brightest) | `setBrightness(level)` |
| Switch display power | `setPower(enabled)` |
| Show or hide the cursor | `setCursorVisible(visible)` |
| Reverse subsequent writes | `setReverse(enabled)` |
| Scale the built-in font from 1 to 4 horizontally and 1 to 2 vertically | `setFontScale(horizontal, vertical)` |

These methods return `false` if an argument is invalid or the VFD
stays busy. Text takes a null-terminated ASCII string. The VFD handles
cursor advance and line wrapping. All coordinates use native VFD
units. X counts columns and Y counts bands of eight pixels. The current
VFD window determines which part of display memory is visible.

## Bitmap format

A bitmap contains one bit per pixel. Pack each vertical group of eight
pixels into a byte. Bit 7 is the top pixel and bit 0 is the bottom.
A 1 is a set pixel and a 0 is a clear pixel in the default write mode.

```text
Bit number:   7   6   5   4   3   2   1   0
            +---+---+---+---+---+---+---+---+
Pixel row:  | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |  within one band
            +---+---+---+---+---+---+---+---+
              ^                           ^
              top                         bottom
```

Write all bands for the leftmost column from top to bottom, then move
one column right. Given zero-based pixel coordinates `px` and `py`
within the bitmap, its byte offset is `px * bands + py / 8` and its bit
mask is `0x80 >> (py % 8)`. A bitmap is exactly `columns * bands` bytes
and `bands * 8` pixels high. For heights that are not multiples of
eight, round up to a whole band and supply the pixels for the extra rows.

The example below is two columns wide and 16 pixels high. The four
bytes travel in numbered order. Each cell shows one byte as eight
pixels, read from top to bottom.

```text
                         left column          right column
                     +-------------------+-------------------+
  first band (0..7)  | [0] 80  #.......  | [2] FF  ########  |
                     +-------------------+-------------------+
 second band (8..15) | [1] 01  .......#  | [3] 00  ........  |
                     +-------------------+-------------------+

  Wire order:  80 01 FF 00                  # set, . clear
```

```cpp
const uint8_t pixels[] = {0x80, 0x01, 0xff, 0x00};
vfd.drawBitmap(120, 3, 2, 2, pixels);
```

The call places the upper-left pixel at column 120, band 3 (pixel
row 24). The rectangle must fit within the controller's 512 columns
and eight bands of display memory. The current window determines
which part of that memory appears on the 256 × 64 panel.

The array overload infers the byte count. For a pointer, pass the
count after the pointer. To stream a bitmap, call `setCursor()` and
`beginBitmap(columns, bands)`, then use `writeBytes()` or `write()` to
send exactly the same number of bytes in the same order. `command()`
accepts raw controller commands.

## Set the wiring

Choose 11 available Arduino GPIOs that operate at 5 V. Pass the pins in
D0–D7, PBUSY, /WR, /RESET order.

```cpp
#include <GU256X64D7000.h>

using BoardWiring = gu256x64d7000::ArduinoWiring<2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12>;
gu256x64d7000::Display<BoardWiring> vfd;
```

On the SparkFun Pro Micro 5 V / 16 MHz, use `A0` and `A1` in place
of pins 11 and 12. Connect board and display grounds, power the VFD
from its own regulated 5 V supply, tie /RD high, and pull /WR and /RESET
high so they stay inactive during MCU reset.

### ATmega4809 direct-port wiring

Use AVR port letters and bit numbers from the board pinout. Avoid pins
reserved for the board's peripherals or clock. `Display<>` selects the
Nano Every wiring on this MCU.

| Signal | ATmega4809 port bits | Nano Every pins |
| --- | --- | --- |
| D0 through D5 | PD0 through PD5 | A3, A2, A1, A0, A6, A7 |
| D6 and D7 | PB0 and PB1 | D9 and D10 |
| PBUSY | PF4 | D6 |
| /WR | PC6 | D4 |
| /RESET | PA0 | D2 |

Pass a different wiring type to the constructor when the board uses
other port bits. The type lets the compiler resolve every register and
mask before the program runs. This example keeps the Nano Every data
bus and uses different control pins.

```cpp
#include <GU256X64D7000.h>

using namespace gu256x64d7000;
using BoardWiring = Wiring<
    SplitData<Port::D, 0, 6, Port::B, 0>,
    Pin<Port::F, 5>,  // PBUSY
    Pin<Port::B, 2>,  // /WR
    Pin<Port::A, 1>   // /RESET
>;
Display<BoardWiring> vfd{BoardWiring{}};
```

Choose the data bus type to match the wiring.

- `DataPort<Port::D>` writes a whole port at once. Connect VFD D0 through
  D7 to MCU bits 0 through 7 of a single dedicated port.
- `SplitData<Port::D, 0, 6, Port::B, 0>` uses two consecutive groups.
  The arguments are the low port, its first bit, the number of low
  data bits, the high port, and its first bit. Wire each group in order.
  Groups starting at bit 0 avoid extra shifts.
- `DataPins<Pin<...>, ...>` accepts eight individual pins in VFD data
  order. It still writes the AVR virtual ports directly. Scattered
  wiring takes more instructions to assemble the output bytes.

With direct-port wiring, `writeBytes` captures the unrelated output bits
once before sending.
Keep other code, including interrupt handlers, from changing output
latches on those ports during a burst. `write` captures them per byte.
Both wait for PBUSY before driving data and strobing /WR. The caller
handles a timeout by resetting and reinitializing the panel before retrying
a partially transmitted command.

## Development

Install [pre-commit](https://pre-commit.com/) and run `pre-commit install`.
Run `pre-commit run --all-files` to format the code and test the command
API with a host C++ compiler. In CI, the hook and Library Manager checks
run first. Both examples then compile across the six boards in parallel.
The emulator and PlatformIO matrices run after the Arduino builds pass.
