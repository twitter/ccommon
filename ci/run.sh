#!/bin/bash

set -uo pipefail
IFS=$'\n\t'

die() { echo "fatal: $*" >&2; exit 1; }

export PATH=$HOME/.cargo/bin:$PATH

mkdir -p _build && ( cd _build && cmake -D BUILD_AND_INSTALL_CHECK=yes .. && make -j && make check )
RESULT=$?

egrep -r ":F:|:E:" . |grep -v 'Binary file' || true


if [[ $RESULT -ne 0 ]]; then
  echo "Build failure" >&2
  exit $RESULT
else
  echo "success!" >&2
  exit 0
fi
