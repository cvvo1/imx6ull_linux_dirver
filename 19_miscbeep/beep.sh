#!/bin/bash
make clean
make
arm-linux-gnueabihf-gcc beepAPP.c -o beepAPP
sudo cp miscbeep.ko beepAPP /home/cvvo/linux/nfs/rootfs/lib/modules/4.1.15/ -f
