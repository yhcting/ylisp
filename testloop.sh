#!/bin/bash
cd test

for i in $(seq 1 99999)
do
    ./test
    if [ 0 != $? ]; then
        echo TEST FAILS : $i / $?
        exit 0
    fi
done