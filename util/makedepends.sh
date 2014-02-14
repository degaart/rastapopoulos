#!/bin/bash

if [ "$1" = "" ]; then
	echo "Usage:" $(basename "$0") "<filename>"
	exit 1
fi


process_file() {
	local depends=$(grep '#include "' < "$1" |awk '{ print $2 }'|sed 's/"//g')
	
	for dep in $depends
	do
		depends="$depends $(process_file $dep)"
	done

	echo $depends
}

cd $(dirname "$1")
process_file "$1"
