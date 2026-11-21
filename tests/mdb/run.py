#!/usr/bin/env python3
"""Measure and compare ATmega4809 fast-path Arduino ELFs in MPLAB X MDB."""

import argparse
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
CLOCK = 16_000_000
BOARDS = {
    "nano-every": ("arduino:megaavr:nona4809:mode=off", 0x09, 6, 4, 0),
    "uno-wifi-rev2": ("arduino:megaavr:uno2018", 0x05, 2, 5, 1),
}


def run(command, cwd, timeout=60):
    return subprocess.run(
        command, cwd=cwd, timeout=timeout, check=True, text=True,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
    ).stdout


def read_instructions(avr_bin, elf, work, start, end, wr_latch, wr_bit):
    listing = run(
        [
            avr_bin / "avr-objdump", "-dz", f"--start-address={start}",
            f"--stop-address={end}", elf,
        ],
        work,
    )
    (work / "burst.disassembly").write_text(listing)
    instructions = [
        (int(address, 16), opcode, operands.strip())
        for address, opcode, operands in re.findall(
            r"^\s*([0-9a-f]+):\t[0-9a-f ]+\t(\w+)[ \t]*([^;\n]*)",
            listing, re.MULTILINE,
        )
    ]
    mnemonics = [(opcode, operands) for _, opcode, operands in instructions]
    strobe = f"0x{wr_latch:02x}, {wr_bit}"
    falling = mnemonics.index(("cbi", strobe))
    rising = mnemonics.index(("sbi", strobe))
    assert [op for op, _ in mnemonics[falling:rising + 3]] == [
        "cbi", "nop", "nop", "sbi", "nop", "nop",
    ], "unexpected strobe instructions"
    for register in ("0x0d", "0x05"):
        assert any(
            opcode == "out" and operands.startswith(register + ",")
            for opcode, operands in mnemonics
        ), f"missing direct output to {register}"
    assert "digitalWrite" not in listing and "digitalRead" not in listing
    return mnemonics, instructions[falling + 1][0], instructions[rising + 1][0]


def measure(args, library, name, board):
    fqbn, wr_latch, wr_bit, busy_bit, reset_bit = BOARDS[board]
    work = args.output_dir / board / name
    work.mkdir(parents=True, exist_ok=True)
    build = work / "build"
    sketch = ROOT / "extras" / "mdb" / "FastPath"
    # The Uno WiFi Rev2 upload image expects a bootloader at 0x0.
    # SIM starts at reset without that bootloader.
    link_options = (
        ["--build-property", "build.text_section_start=.text=0x0"]
        if board == "uno-wifi-rev2" else []
    )
    try:
        output = run(
            [
                args.arduino_cli, "compile", "--fqbn",
                fqbn, *link_options, "--library", library,
                "--build-path", build, sketch,
            ],
            work,
            timeout=180,
        )
    except subprocess.CalledProcessError as error:
        (work / "build.log").write_text(error.stdout)
        raise
    (work / "build.log").write_text(output)
    elf = build / "FastPath.ino.elf"
    symbols = {
        symbol: int(address, 16)
        for address, symbol in re.findall(
            r"^([0-9a-f]+) \w (vfd_\w+)$",
            run([args.avr_bin / "avr-nm", elf], work), re.MULTILINE,
        )
    }

    start, end = symbols["vfd_burst_begin"], symbols["vfd_burst_end"]
    instructions, after_fall, after_rise = read_instructions(
        args.avr_bin, elf, work, start, end, wr_latch, wr_bit
    )
    # AVR ELF symbols use byte addresses. MDB AVR breakpoints use word addresses.
    breakpoints = [
        symbols["vfd_busy_probe"], start, end, symbols["vfd_test_end"],
        after_fall, after_rise,
    ]
    command_file = work / "fast-path.mdb"
    command_file.write_text("\n".join([
        "device ATmega4809",
        "set oscillator.frequencyunit Mega",
        "set oscillator.frequency 16",
        "hwtool SIM",
        f'program "{elf}"',
        *(f"break *0x{address // 2:x}" for address in breakpoints),
        "run", "wait 5000",
        # SIM treats PF4 and VPORTF.IN separately. Inject at the register
        # the AVR instruction actually reads.
        f"write /r 0x16 0x{1 << busy_bit:02x}",
        "continue", "wait 5000", "print vfd_result",
        "write /r 0x16 0x00",
        "stopwatch nror", "stopwatch clear",
        "continue", "wait 5000", "stopwatch", f"x /r1xb 0x{wr_latch:x}", "delete 4",
        "continue", "wait 5000", "stopwatch", f"x /r1xb 0x{wr_latch:x}", "delete 5",
        "continue", "wait 5000", "stopwatch",
        "x /r1xb 0xd", "x /r1xb 0x5", "x /r1xb 0x1",
        "continue", "wait 5000", "print vfd_result", "quit", "",
    ]))
    log = work / "fast-path.log"
    with log.open("w") as output:
        subprocess.run(
            [args.mdb, command_file], cwd=work, timeout=60, check=True,
            stdout=output, stderr=subprocess.STDOUT,
        )
    trace = log.read_text()
    stops = [int(address, 16) for address in re.findall(r"address:0x([0-9a-f]+)", trace)]
    expected_stops = [breakpoints[i] // 2 for i in (0, 1, 4, 5, 2, 3)]
    assert stops == expected_stops, f"unexpected breakpoint stops, see {log}"
    assert re.findall(r"vfd_result=\s*(\d+)", trace) == ["1", "2"], log
    base_latch = 0x02 if wr_latch == 0x05 else 0
    high_latch = base_latch | (1 << wr_bit)
    strobe_values = re.findall(rf"x /r1xb 0x{wr_latch:x}\n(\w+) *\n", trace)
    assert strobe_values[:2] == [f"{base_latch:02X}", f"{high_latch:02X}"], log
    data_high_latch = high_latch if wr_latch == 0x05 else 0x02
    for address, value in ((0xD, 0x25), (0x5, data_high_latch),
                           (0x1, 1 << reset_bit)):
        assert re.search(rf"x /r1xb 0x{address:x}\n{value:02X} *\n", trace), log
    fall, rise, burst = (
        int(count) for count in re.findall(r"^Stopwatch cycle count = (\d+)", trace, re.MULTILINE)
    )
    pulse = rise - fall
    assert pulse * 1_000_000_000 >= 100 * CLOCK, "strobe below 100 ns"
    print(
        f"{board} {name}: /WR low {pulse} cycles ({pulse * 1e9 / CLOCK:g} ns), "
        f"64-byte ready burst {burst} cycles ({burst * 1e6 / CLOCK:g} us)"
    )
    return pulse, burst, instructions


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mdb", required=True, type=Path)
    parser.add_argument("--arduino-cli", default="arduino-cli")
    parser.add_argument("--avr-bin", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--compare-library", type=Path)
    parser.add_argument("--board", choices=BOARDS, action="append")
    args = parser.parse_args()
    for option in ("mdb", "avr_bin", "output_dir", "compare_library"):
        value = getattr(args, option)
        if value is not None:
            setattr(args, option, value.resolve())
    for board in args.board or BOARDS:
        reference = measure(args, ROOT, "reference", board)
        if args.compare_library:
            candidate = measure(args, args.compare_library, "candidate", board)
            if reference != candidate:
                parser.exit(1, f"{board} burst timings or instructions changed.\n")


if __name__ == "__main__":
    main()
