#!/usr/bin/env python3
"""Run the Uno R4 VFD bus fixture against an Arduino ELF built with NO_USB."""

import argparse
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("renode")
parser.add_argument("elf", type=Path)
parser.add_argument("output_dir", type=Path)
args = parser.parse_args()

fixture = Path(__file__).resolve().parent
output = args.output_dir.resolve()
output.mkdir(parents=True, exist_ok=True)
script = output / "uno-r4.resc"
script.write_text(
    f"include @{fixture / 'VfdBus.cs'}\n"
    "mach create\n"
    f"machine LoadPlatformDescription @{fixture / 'uno-r4.repl'}\n"
    f"sysbus LoadELF @{args.elf.resolve()}\n"
    'emulation RunFor "0.500"\n'
    "q\n"
)
log = output / "uno-r4.log"
with log.open("w") as trace:
    subprocess.run(
        [args.renode, "--console", "--disable-xwt", "--plain", script],
        cwd=output, stdout=trace, stderr=subprocess.STDOUT, timeout=60, check=True,
    )

trace = log.read_text()
if "VFD BUS PASS" not in trace or "VFD BUS FAIL" in trace:
    parser.exit(1, f"Uno R4 bus fixture failed. See {log}\n")
print(f"Uno R4 bus fixture passed. See {log}")
