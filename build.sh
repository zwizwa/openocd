#!/bin/bash
cd $(dirname $0)
./bootstrap \
&& \
./configure \
    --prefix=/nix/exo/ \
    --enable-verbose \
    --enable-ftdi \
    --enable-stlink \
    --enable-cmsis-dap \
&& \
make \
&& \
make install


