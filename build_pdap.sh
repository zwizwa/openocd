#!/bin/bash
cd $(dirname $0)
./bootstrap \
&& \
./configure \
    --prefix=$(readlink -f .) \
    --enable-verbose \
    --enable-pdap \
&& \
make -j8 \
&& \
make install


