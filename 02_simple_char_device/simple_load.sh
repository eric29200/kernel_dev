#!/bin/sh
module="simple"
device="simple"
mode="664"
group="eric"

# load module
sudo insmod ./$module.ko $* || exit 1

# retrieve major number
major=$(awk "\$2==\"$module\" {print \$1}" /proc/devices)

sudo rm -f /dev/${device}
sudo mknod /dev/${device} c $major 0
sudo chgrp $group /dev/${device}
sudo chmod $mode  /dev/${device}
