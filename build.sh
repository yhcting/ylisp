#!/bin/bash

check_exit_status() {
    if [ 0 != $? ]; then
        echo There is ERROR : $?...
        exit 0
    fi
}

while getopts "i:b:d:f:" opt; do
    case $opt in
        b)
            export SYSBIN=$OPTARG
            echo sysbin = $SYSBIN
            ;;
        d)
            export DEFS="$DEFS -D${OPTARG}"
            echo defs = $DEFS
            ;;
        i)
            export INCLUDES="$INCLUDES -I${OPTARG}"
            echo inc = $INCLUDES
            ;;
        f)
            export CFLAGS="$CFLAGS $OPTARG"
            echo cflags = $OPTARG
            ;;
    esac
done

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
