#!/bin/bash

dest_file=obj/Makefile.depends
tmpfile="$(mktemp /tmp/rasta.XXXX)"

cleanup() {
    rm -f "$tmpfile"
}
trap "cleanup" EXIT


touch "$dest_file"

for i in $*
do
    if [ -f "$i" ]; then
        echo "[DEP] $i"
        /bin/echo -n "obj/" >> "$tmpfile" || exit 1
        $CPP -E $CPPFLAGS -MM $i >> "$tmpfile" || exit 1
    fi
done
cp "$tmpfile" "$dest_file"
