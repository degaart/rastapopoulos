#!/bin/bash

. shell-common.sh

IMGFILE="$1"
SRCFILE="$2"
DSTFILE="$3"

[ -n "$DSTFILE" ] || {
    echo "Usage: $PROGRAM_NAME <imgfile> <srcfile> <dstfile>"
    exit 1
}

echo "[COPY] $SRCFILE"
mtools_init "$IMGFILE"
mcopy -D o "$SRCFILE" "$DSTFILE"

