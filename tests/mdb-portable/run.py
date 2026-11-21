#!/usr/bin/env python3
"""Validate the Arduino GPIO bus in the MPLAB X MDB classic AVR simulator."""

import argparse
from pathlib import Path
import re
import subprocess


ROOT = Path(__file__).resolve().parents[2]
BOARDS = {
    # Order: FQBN, simulator device, data D0-D7, PBUSY, /WR, /RESET.
    "uno-r3": (
        "arduino:avr:uno", "ATmega328P",
        "PD2 PD3 PD4 PD5 PD6 PD7 PB0 PB1", "PB2", "PB3", "PB4",
    ),
    "mega-2560": (
        "arduino:avr:mega:cpu=atmega2560", "ATmega2560",
        "PE4 PE5 PG5 PE3 PH3 PH4 PH5 PH6", "PB4", "PB5", "PB6",
    ),
    "pro-micro-5v": (
        "SparkFun:avr:promicro:cpu=16MHzatmega32U4", "ATmega32U4",
        "PD1 PD0 PD4 PC6 PD7 PE6 PB4 PB5", "PB6", "PF7", "PF6",
    ),
}
EXPECTED = bytes.fromhex(
    # Controller reset and clear, text at (260, 2), bitmap at (120, 3).
    "1b 40 0c "
    "1f 24 04 01 02 00 48 69 "
    "1f 24 78 00 03 00 1f 28 66 11 02 00 02 00 01 80 01 ff 00"
)


def verify(args, name):
    fqbn, device, data, busy, write, reset = BOARDS[name]
    data = data.split()
    work = args.output_dir / name
    work.mkdir(parents=True, exist_ok=True)
    build = work / "build"
    sketch = ROOT / "extras" / "MdbPortableBus"
    with (work / "build.log").open("w") as log:
        subprocess.run(
            [
                args.arduino_cli, "compile", "--fqbn", fqbn, "--library", ROOT,
                "--build-path", build, sketch,
            ],
            cwd=work, timeout=180, check=True, stdout=log,
            stderr=subprocess.STDOUT,
        )
    elf = build / "MdbPortableBus.ino.elf"

    # AVR ELF symbols are byte addresses and MDB breakpoints use word addresses.
    table = subprocess.check_output([args.avr_nm, elf], cwd=work, text=True)
    marks = {
        label: int(address, 16) // 2
        for address, label in re.findall(
            r"^([0-9a-f]+) \w mdb_(\w+)$", table, re.MULTILINE,
        )
    }
    commands = [
        f"device {device}",
        "set oscillator.frequencyunit Mega",
        "set oscillator.frequency 16",
        # MDB warns on each Arduino flash pin-table read into R31 on
        # some cores. The sampled GPIO levels still check those reads.
        "set warningmessagebreakoptions.W0033_CORE_UNPREDICTABLE_OPCODE Ignore",
        "hwtool SIM",
        f'program "{elf}"',
        *(f"break *0x{address:x}" for address in marks.values()),
    ]
    expected_stops = []
    expected_pins = []

    def stop(label, action="continue", wait_ms=2500):
        commands.extend([action, f"wait {wait_ms}"])
        expected_stops.append(marks[label])

    def pin(name, high):
        commands.append(f"print pin {name}")
        expected_pins.append((name, "in" if name == busy else "out", bool(high)))

    stop("ready", "run")
    pin(reset, True)
    pin(write, True)
    commands.append(f"write pin {busy} low")

    stop("reset_low")
    pin(reset, False)
    stop("reset_high")
    pin(reset, True)

    for value in EXPECTED:
        stop("write_low")
        pin(write, False)
        pin(busy, False)
        stop("write_high")
        pin(write, True)
        for bit, name in enumerate(data):
            pin(name, value & (1 << bit))
        commands.append(f"write pin {busy} high")
        stop("busy_read")
        pin(busy, True)
        commands.append(f"write pin {busy} low")

    stop("timeout")
    commands.append(f"write pin {busy} high")
    stop("passed", wait_ms=15000)
    pin(busy, True)
    pin(write, True)
    commands.append("quit")

    script = work / "bus.mdb"
    script.write_text("\n".join(commands) + "\n")
    output = work / "bus.log"
    with output.open("w") as log:
        subprocess.run(
            [args.mdb, script], cwd=work, timeout=180, check=True, stdout=log,
            stderr=subprocess.STDOUT,
        )
    trace = output.read_text()
    actual_stops = [
        int(address, 16) for address in re.findall(r"address:0x([0-9a-f]+)", trace)
    ]
    if actual_stops != expected_stops:
        raise RuntimeError(f"Unexpected breakpoint sequence for {fqbn}; see {output}")
    # MDB prints analog-capable outputs in volts on some devices.
    actual_pins = [
        (name, direction, level == "HIGH" or bool(volts and float(volts) >= 2.5))
        for name, direction, level, volts in re.findall(
            r"^(P[A-Z][0-7])[ \t]+[AD](in|out)[ \t]+(?:(HIGH|LOW)|([0-9.]+)V)\b",
            trace, re.MULTILINE,
        )
    ]
    if actual_pins != expected_pins:
        for index, expected in enumerate(expected_pins):
            actual = actual_pins[index] if index < len(actual_pins) else None
            if actual != expected:
                raise RuntimeError(
                    f"Pin sample {index}: expected {expected}, got {actual}; see {output}"
                )
        raise RuntimeError(f"Unexpected extra pin samples; see {output}")
    print(f"{fqbn}: checked {len(EXPECTED)} write strobes and PBUSY timeout")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mdb", required=True, type=Path)
    parser.add_argument("--arduino-cli", default="arduino-cli")
    parser.add_argument("--avr-nm", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--board", choices=BOARDS, action="append")
    args = parser.parse_args()
    args.mdb = args.mdb.resolve()
    args.avr_nm = args.avr_nm.resolve()
    args.output_dir = args.output_dir.resolve()
    for board in args.board or BOARDS:
        verify(args, board)


if __name__ == "__main__":
    main()
