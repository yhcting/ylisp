#!/bin/bash

#########################################
# Build script for local developement
#########################################

installdir=$(pwd)/install/
libdir=${installdir}lib/ylisp

rm -rf ${installdir}
mkdir -p ${installdir}

while getopts "b" opt; do
    case $opt in
        b)
            rm -rf autom4te.cache
            aclocal
            autoheader
            autoconf
            automake
            ./configure --prefix="${installdir}" --with-debug=v
            make clean
            ;;
        *)
            exit 1
            ;;
    esac
done

make
make install

