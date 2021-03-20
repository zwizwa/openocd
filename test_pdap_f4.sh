#!/bin/bash
OPENOCD=$(readlink -f $(dirname $0)/src/openocd)
cd tcl
# export PDAP_LOGFILE=/tmp/pdap.txt
export PDAP_TTY=/dev/tty-0483-5740-55ff73065077564813521387
exec gdb \
     --eval-command="set confirm off" \
     --eval-command="file $OPENOCD" \
     --eval-command="run -d2 --command 'adapter driver pdap' --command 'source target/stm32f4x.cfg'"


# $OPENOCD \
#     -d2 \
#     --command "adapter driver pdap" \
#     --command "adapter speed 123" \
#     --command 'source target/stm32f4x.cfg' \



    

