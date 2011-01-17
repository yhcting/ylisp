#!/bin/bash

check_exit_status() {
    if [ 0 != $? ]; then
        echo There is ERROR : $?...
        exit 0
    fi
}

clear

rm -rf include
rm -rf lib

mkdir include
mkdir lib

MODS="ylisp ylbase ylext yljfe ylr test"

for subm in $MODS; do
    echo ----- build $subm ------
    cd $subm;
    make clean
    make release;
    check_exit_status
    cd ..;
done
