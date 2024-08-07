#!/bin/bash
make clean
make
arm-linux-gnueabihf-gcc -march=armv7-a -mfpu=neon -mfloat-abi=hard icm20608APP.c -o icm20608APP
sudo cp icm20608.ko icm20608APP /home/cvvo/linux/nfs/rootfs/lib/modules/4.1.15/ -f


