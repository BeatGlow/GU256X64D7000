# Portable GPIO tests in MPLAB X MDB

Run the test on the Arduino Uno R3, Arduino Mega 2560, and 5 V / 16 MHz
SparkFun Pro Micro. Install MPLAB X with the classic AVR simulator,
Arduino CLI with the Arduino AVR Boards core, and the SparkFun AVR Boards
core. Set the SparkFun package index when installing it.

```sh
arduino-cli core install arduino:avr@1.8.8
arduino-cli core install SparkFun:avr@1.1.13 \
    --additional-urls https://raw.githubusercontent.com/sparkfun/Arduino_Boards/main/IDE_Board_Manager/package_sparkfun_index.json
python3 tests/mdb-portable/run.py \
    --mdb /path/to/mdb.sh \
    --avr-nm /path/to/avr-gcc/bin/avr-nm \
    --output-dir /path/to/test-output
```

The runner builds the Arduino sketch once per board and loads its ELF
into MDB. The Pro Micro test skips native USB initialization because MDB
6.35 stalls at the USB PLL lock on the ATmega32U4. The Arduino core still
initializes its timers and GPIO. The runner samples the simulated pin directions and levels for reset,
both write strobe edges, and the eight data lines. The expected bytes
cover initialization, text, and a two-column by two-band bitmap. MDB
holds PBUSY high until the library reads it after each write. At the end
it holds PBUSY high through the sketch's 120 ms timeout check and verifies
that no further write occurs.

Use `--board uno-r3`, `--board mega-2560`, or `--board pro-micro-5v`
to select a target. The output directory retains the compiler output,
MDB command file, and simulator trace for each board. The fixture uses
debugger breakpoints at the GPIO transitions, so these tests check
signal sequence and payload. The timing of the uninstrumented
ATmega4809 backend is covered by the separate fast-path benchmark.
