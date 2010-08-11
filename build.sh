#!/bin/bash

check_exit_status() {
    if [ 0 != $? ]; then
        echo There is ERROR : $?...
        exit 0
    fi
}

clear

mkdir include
mkdir lib

cd ylisp
check_exit_status
make clean
check_exit_status
make release
check_exit_status

cd ../ylbase
check_exit_status
make clean
check_exit_status
make release
check_exit_status

cd ../ylmath
check_exit_status
make clean
check_exit_status
make release
check_exit_status

cd ../ylstring
check_exit_status
make clean
check_exit_status
make release
check_exit_status

cd ../ylsystem
check_exit_status
make clean
check_exit_status
make release
check_exit_status

cd ../ylcon
check_exit_status
make clean
check_exit_status
make release
check_exit_status

cd ../yljfe
check_exit_status
make clean
check_exit_status
make release
check_exit_status

cd ..