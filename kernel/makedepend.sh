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

        ext="${i##*.}"
        if [ "$ext" == "c" ]; then
            $CC -E $CPPFLAGS $CFLAGS -MM $i >> "$tmpfile" || exit 1
        elif [ "$ext" == "cpp" ]; then
            $CXX -E $CPPFLAGS $CXXFLAGS -MM $i >> "$tmpfile" || exit 1
        else
            echo "Unknown file type $i"
            exit 1
        fi
    fi
done
cp "$tmpfile" "$dest_file"
