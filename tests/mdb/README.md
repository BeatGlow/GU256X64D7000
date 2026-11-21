# ATmega4809 fast-path benchmark

Install Arduino CLI, Arduino megaAVR Boards 1.8.8, and Microchip MPLAB X
with support for ATmega4809 in the simulator. Use the AVR tools packaged
with the Arduino core. The script builds the sketch for Nano Every and Uno WiFi Rev2 and runs each
in MDB at 16 MHz. Use `--board` to select a single board. The Uno WiFi
Rev2 simulator build links at the reset vector because MDB starts without
the bootloader.

```sh
python3 tests/mdb/run.py --mdb /path/to/mdb.sh \
  --avr-bin /path/to/avr-gcc/bin --output-dir /path/to/benchmark-output
```

Add `--compare-library /path/to/another/checkout` to require the same
strobe cycles, burst cycles, and disassembled burst instructions from both
builds. The output directory contains each build, MDB log, and
disassembly.

The sketch checks PBUSY and sends 64 bytes with interrupts masked while
the simulated VFD is ready. The MDB stopwatch measures the first /WR low
pulse and the entire burst, including latch capture and the final ready
check. The script verifies the /WR and /RESET latches and the split-port
encoding of the final byte.

MDB 6.35 exposes PORTF pin stimulus independently of VPORTF.IN on the
ATmega4809. The runner writes the VPORTF.IN register directly and
checks the VPORT output latches. Use a hardware logic analyzer for
electrical pin timing and PBUSY propagation.
