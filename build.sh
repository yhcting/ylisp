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
check_exit_status
make clean
check_exit_status
make release
check_exit_status

cd ../ylbase
echo --- build ylbase
check_exit_status
make clean
check_exit_status
make release
check_exit_status

cd ../ylmath
echo --- build ylmath
check_exit_status
make clean
check_exit_status
make release
check_exit_status

cd ../ylstring
echo --- build ylstring
check_exit_status
make clean
check_exit_status
make release
check_exit_status

cd ../ylsystem
echo --- build ylsystem
check_exit_status
make clean
check_exit_status
make release
check_exit_status

cd ../ylext
echo --- build ylext
check_exit_status
make clean
check_exit_status
make release
check_exit_status

cd ../yljfe
echo --- build yljfe
check_exit_status
make clean
check_exit_status
make release
check_exit_status

cd ../test
make clean
make

cd ..