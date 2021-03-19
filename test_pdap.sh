#!/bin/bash
OPENOCD=$(readlink -f $(dirname $0)/src/openocd)
cd tcl
exec gdb \
     --eval-comman="file $OPENOCD" \
     --eval-command="run -d2 --command 'adapter driver pdap' --command 'adapter speed 123' --command 'source target/stm32f4x.cfg'"


# $OPENOCD \
#     -d2 \
#     --command "adapter driver pdap" \
#     --command "adapter speed 123" \
#     --command 'source target/stm32f4x.cfg' \



    

