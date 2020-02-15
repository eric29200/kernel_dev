#!/bin/sh
module="simple"
device="simple"

# remove stale nodes
sudo rm -f /dev/${device}

# invoke rmmod with all arguments we got
sudo rmmod $module $* || exit 1
