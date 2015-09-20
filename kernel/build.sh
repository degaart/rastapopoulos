#!/bin/bash

echo "$@" > /tmp/build.log

. /Volumes/Kratos/Projects/RastapopoulOS/src/variables.sh
cd $PREFIX/src/0.11/kernel
make



