#!/bin/bash
cd $(dirname $0)
./src/openocd \
    -d3 \
    --command "adapter driver zwizwa" \
    --command "adapter speed 10000" \
    --command 'source ./tcl/target/stm32f4x.cfg' \



    

