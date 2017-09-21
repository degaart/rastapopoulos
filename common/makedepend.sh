#!/bin/bash

dest_file=obj/Depends.mk
CC="${CC-cc}"

if ! [ -d "$(dirname "$dest_file")" ]; then
    mkdir -p "$(dirname "$dest_file")" || exit 1
fi

echo -n > "$dest_file"

for file in $*
do
    if [ -f "$file" ]; then
        filebase="$(basename "$file")"
        echo "[DEP] $filebase"
        $CC -E $CPPFLAGS $CFLAGS -MM $file | sed -E "s/.*: /obj\/${filebase/\//\\/}.o: /" >> "$dest_file" || exit 1
    fi
done

