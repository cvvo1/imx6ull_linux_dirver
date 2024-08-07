#!/bin/bash
make clean
make
arm-linux-gnueabihf-gcc keyAPP.c -o keyAPP
sudo cp key.ko keyAPP /home/cvvo/linux/nfs/rootfs/lib/modules/4.1.15/ -f
