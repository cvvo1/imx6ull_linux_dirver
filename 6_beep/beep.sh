#!/bin/bash
make clean
make
arm-linux-gnueabihf-gcc beepAPP.c -o beepAPP
sudo cp beep.ko beepAPP /home/cvvo/linux/nfs/rootfs/lib/modules/4.1.15/ -f
