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
    --enable-zwizwa \
&& \
make -j8 \
&& \
make install


