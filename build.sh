#!/bin/bash
cd $(dirname $0)
./bootstrap \
&& \
./configure \
    --prefix=. \
    --enable-verbose \
    --enable-ftdi \
    --enable-stlink \
    --enable-cmsis-dap \
    --enable-pdap \
&& \
make -j8 \
&& \
make install


