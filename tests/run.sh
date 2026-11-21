#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build=$(mktemp -d "${TMPDIR:-/tmp}/gu256x64d7000-tests.XXXXXX")
trap 'rm -r -- "$build"' EXIT HUP INT TERM

"${CXX:-c++}" -std=gnu++11 -Wall -Wextra -Werror -D__AVR_ATmega4809__ \
  -I"$root/tests" -I"$root/src" "$root/tests/commands.cpp" -o "$build/commands"
"$build/commands"
