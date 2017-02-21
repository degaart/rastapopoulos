#!/bin/bash

graphic=0
reboot=0
debug=0

while [ -n "$1" ]
do
    case "$1" in
        graphic)
            graphic=1
            ;;
        reboot)
            reboot=1
            ;;
        debug)
            debug=1
            ;;
    esac
    shift
done

QFLAGS=

if [ "$graphic" -eq 0 ]; then
    QFLAGS="$QFLAGS -nographic"
fi

if [ "$reboot" -eq 0 ]; then
    QFLAGS="$QFLAGS -no-reboot"
fi

if [ "$debug" -ne 0 ]; then
    QFLAGS="$QFLAGS -s -S"
fi

# Execute qemu
qemu-system-i386 \
    -drive file=disk.img,format=raw \
    -boot c \
    -m 128 \
    -debugcon file:/tmp/rastapopoulos.log \
    $QFLAGS

