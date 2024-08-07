#!/bin/bash
make clean
make
arm-linux-gnueabihf-gcc newchrledAPP.c -o newchrledAPP
sudo cp newchrled.ko newchrledAPP /home/cvvo/linux/nfs/rootfs/lib/modules/4.1.15/ -f
