#!/bin/bash
make clean
make
arm-linux-gnueabihf-gcc dtsledAPP.c -o dtsledAPP
sudo cp dtsled.ko dtsledAPP /home/cvvo/linux/nfs/rootfs/lib/modules/4.1.15/ -f
