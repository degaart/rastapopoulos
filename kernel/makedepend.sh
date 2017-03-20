#!/bin/bash

dest_file=obj/Depends.mk

echo -n > "$dest_file"

for file in $*
do
    if [ -f "$file" ]; then
        echo "[DEP] $file"
        $CC -E $CPPFLAGS $CFLAGS -MM $file | sed -E "s/.*: /obj\/${file/\//\\/}.o: /" >> "$dest_file" || exit 1
    fi
done


