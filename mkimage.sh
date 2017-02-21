#!/bin/bash

. shell-common.sh

install_darwin() {
    # Attach image
    local attachres
    attachres="$(hdiutil attach "$OUTFILE")"
    [ $? -eq 0 ] || exit 1
    
    # Get line pointing to mountpoint
    local mountpoint_line
    mountpoint_line="$(echo "$attachres"|tail -n1)"

    # Assume mountpoint starts with the string '/Volumes/'
    # Strip everything up to /Volumes/ 
    local mountpoint
    mountpoint="/Volumes/${mountpoint_line#*/Volumes/}"

    # Now call grub-install
    grub-install --root="$mountpoint" "$OUTFILE" || {
        hdiutil detach "$mountpoint"
        exit 1
    }

    # Unmount disk image
    hdiutil detach "$mountpoint"
}

OUTFILE="$1"
LABEL="$2"
SIZE="$3"

[ -n "$SIZE" ] || {
    echo "Usage: ${PROGRAM_NAME} <outfile> <label> <size>"
    exit 1
}

mtools_init "$OUTFILE"

# Calculate size per commandline
SIZE_size=${#SIZE}
SIZE_suffix=${SIZE:$SIZE_size-1}
SIZE_base=${SIZE:0:$SIZE_size-1}
case "$SIZE_suffix" in
    'G'|'g')
        SIZE=$(( $SIZE_base * 1024 * 1024 * 1024 ))
        ;;
    'M'|'m')
        SIZE=$(( $SIZE_base * 1024 * 1024 ))
        ;;
    'K'|'k')
       SIZE=$(( $SIZE_base * 1024 ))
       ;;
    [0-9])
        ;;
    *)
        echo "Invalid size: $SIZE"
        exit 1
        ;;
esac

# Create new image using dd
echo "Creating image (" $(( $SIZE / 1024 )) "kb)"
echo -n > "$OUTFILE"
dd if=/dev/zero of="$OUTFILE" bs=1024 count=$(( $SIZE / 1024 )) || exit 1

# Create partition inside image
echo "Creating partition..."
mpartition -I L: || exit 1
mpartition -c -b 2048 -l $(( $SIZE - 2048 )) L: || exit 1
mpartition -a L: || exit 1

# format partition
echo "Formatting partition (label $LABEL)"
mformat -v "$LABEL" L:

# Install grub
case $(uname) in
    'Darwin')
        install_darwin
        ;;
    *)
        echo "Platform not supported: $(uname)"
        exit 1
        ;;
esac


