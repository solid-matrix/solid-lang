#!/usr/bin/env bash
# End-to-end smoke: solid build (seed module) -> link -> exit 13.
# Hosted: clang driver with lld executor (crt + libc).
# Freestanding (--libc=none): direct ld.lld, raw exit syscall.
set -u

if [ "$#" -ne 1 ]; then
  echo "usage: smoke.sh <path-to-solid>" >&2
  exit 1
fi
SOLID="$1"

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

cd "$work"

# Hosted: clang + lld.
"$SOLID" build -o hello || exit 1
file hello
ldd hello || true
./hello
code=$?
if [ "$code" -ne 13 ]; then
  echo "cli_smoke: hosted: expected exit 13, got $code" >&2
  exit 1
fi
echo "cli_smoke: hello exited 13"

# Freestanding: direct ld.lld, no crt/libc (triple env `none`).
"$SOLID" build --target=x86_64-unknown-linux-none -o bare || exit 1
file bare
./bare
code=$?
if [ "$code" -ne 13 ]; then
  echo "cli_smoke: freestanding: expected exit 13, got $code" >&2
  exit 1
fi
echo "cli_smoke: bare exited 13"
