#!/bin/bash

set -euo pipefail
IFS=$'\n\t'

die() { echo "fatal: $*" >&2; exit 1; }

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 check-install-path"
  exit 1
fi

CHECK_PREFIX="$1"
shift

TEMP="$(mktemp -d -t TEMP.XXXXXXX)" || die "failed to make tmpdir"
cleanup() { [[ -n "${TEMP:-}" ]] && rm -rf "${TEMP}"; }
trap cleanup EXIT

realpath() { python3 -c "import os,sys; print(os.path.realpath(sys.argv[1]))" "$1"; }

TOPLEVEL="$(cd "$(dirname "$(realpath "$0" >/dev/null || exit 1)")" && git rev-parse --show-toplevel)" || die 'failed to find TOPLEVEL'


CHECK_VERSION=0.14.0
CHECK_TARBALL="check-${CHECK_VERSION}.tar.gz"
CHECK_DIR="check-${CHECK_VERSION}"
CHECK_LOG="build-check.log"

echo "building and installing check" >&2

(
  echo "cd..." >&2 &&
  cd "$TEMP" &&
    echo "wget..." >&2 &&
    wget "https://github.com/libcheck/check/releases/download/${CHECK_VERSION}/${CHECK_TARBALL}" &&
    echo "tar..." >&2 &&
    tar xfz "${CHECK_TARBALL}" &&
    echo "cd..." >&2 &&
    cd "${CHECK_DIR}" &&
    echo "configure.." >&2 &&
    ./configure --prefix="${CHECK_PREFIX}" &&
    echo "make.." >&2 &&
    make &&
    echo "install.." >&2 &&
    make install
)
#) >$TEMP/${CHECK_LOG} 2>&1

RESULT=$?
if [[ $RESULT -ne 0 ]]; then
  cat >&2 <<EOS
check build failed! log below:

EOS

  cat $TEMP/${CHECK_LOG}
else
  echo "Success!" >&2
fi

exit $RESULT
