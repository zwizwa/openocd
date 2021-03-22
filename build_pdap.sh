#!/bin/bash
cd $(dirname $0)
./bootstrap \
&& \
./configure \
    --enable-verbose \
    --enable-pdap \
&& \
make -j8 \
&& \
make install


