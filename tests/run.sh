#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build=$(mktemp -d "${VFD_TEST_TMPDIR:-${TMPDIR:-/tmp}}/gu256x64d7000-tests.XXXXXX")
trap 'rm -r -- "$build"' EXIT HUP INT TERM

"${CXX:-c++}" -std=gnu++11 -Wall -Wextra -Werror -DGU256X64D7000_HOST_TEST \
  -I"$root/tests" -I"$root/src" "$root/tests/commands.cpp" -o "$build/commands"
"$build/commands"

if printf '%s\n' '#include <GU256X64D7000.h>' \
  'gu256x64d7000::Display<gu256x64d7000::ArduinoWiring<2,3,4,5,6,7,8,9,10,11,2>> vfd;' |
  "${CXX:-c++}" -std=gnu++11 -I"$root/tests" -I"$root/src" -x c++ -fsyntax-only - \
    >"$build/duplicate.log" 2>&1; then
  echo "Display accepted duplicate pins" >&2
  exit 1
fi
grep -q 'VFD pins must be distinct' "$build/duplicate.log" || {
  cat "$build/duplicate.log" >&2
  exit 1
}
