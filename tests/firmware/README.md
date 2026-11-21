# Portable GPIO emulator checks

The BusContract sketch uses the normal Arduino GPIO backend. Its test
fixtures drive PBUSY, sample D0 through D7 at the rising /WR edge, and
check the bytes sent during initialization, text output, and a bitmap
write. A final write must time out while the fixture holds PBUSY high.

## simavr

Install simavr development headers, libelf development headers,
pkg-config, and a C compiler. Install Arduino AVR Boards 1.8.8 for Uno
and Mega, or SparkFun AVR Boards 1.1.13 for the Pro Micro. For SparkFun,
use its Board Manager index.

| Board | Runner argument | Arduino FQBN |
| --- | --- | --- |
| Uno R3 | `uno` | `arduino:avr:uno` |
| Mega 2560 | `mega` | `arduino:avr:mega:cpu=atmega2560` |
| SparkFun Pro Micro 5 V / 16 MHz | `promicro` | `SparkFun:avr:promicro:cpu=16MHzatmega32U4` |

Run from the library root with a writable output directory. Select the
matching `board` and `fqbn` from the table.

```sh
board=uno
fqbn=arduino:avr:uno
build=/path/to/emulator-build
mkdir -p "$build"
arduino-cli compile --fqbn "$fqbn" --library . --output-dir "$build" extras/BusContract
cc -std=c11 -Wall -Wextra -Werror tests/simavr/bus.c \
  $(pkg-config --cflags --libs simavr) -o "$build/check"
"$build/check" "$board" "$build/BusContract.ino.hex"
```

simavr also checks /RESET width, /WR width, data setup and hold, and
delayed assertion of PBUSY against the 16 MHz simulated clock. The
runner prints the cycle count and sampled byte at each rising /WR edge.

## Renode

Install Arduino UNO R4 Boards 1.5.3 and Renode 1.17.0. Build this
fixture with USB disabled because the Renode board lacks the USB
peripheral model.

```sh
build=/path/to/emulator-build
mkdir -p "$build"
arduino-cli compile --fqbn arduino:renesas_uno:minima --library . \
  --build-property compiler.cpp.extra_flags=-DNO_USB --output-dir "$build" \
  extras/BusContract
python3 tests/renode/run.py renode "$build/BusContract.ino.elf" "$build"
```

The Renode log includes each captured byte and the handshake result.
