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

cd ylisp
echo --- build ylisp
make clean
check_exit_status
make release
check_exit_status

cd ../ylbase
echo --- build ylbase
make clean
check_exit_status
make release
check_exit_status

cd ../ylext
echo --- build ylext
make clean
check_exit_status
make release
check_exit_status

cd ../yljfe
echo --- build yljfe
make clean
check_exit_status
make
check_exit_status

cd ../ylr
echo --- build ylr
make clean
check_exit_status
make
check_exit_status

cd ../test
echo --- build test
make clean
check_exit_status
make
check_exit_status

cd ..