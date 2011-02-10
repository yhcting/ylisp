#!/bin/bash

check () {
    # $1 : config command
    # $2 : step - configure, make or running
    if test 0 != $?; then
        echo == Error: $1 : $2
        exit 1
    fi
}


testunit () {
    # $1 : config command
    $1
    check $1 "config"
    make
    check $1 "make"
    make install
    check $1 "install"
    cd test
    ./test
    check $1 "running"
    cd ..
    make clean
}

cmdprefix="./configure --prefix=$(pwd)/install "
option="v f"
cmd=${cmdprefix}
testunit "${cmd}" 
cmd="${cmd} --with-debug="
testunit "${cmd}"

for opt in $option; do
    testunit "${cmd}${opt}"
done

echo ==========================
echo =       Well Done!       =
echo ==========================
