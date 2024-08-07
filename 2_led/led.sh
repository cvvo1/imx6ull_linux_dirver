#!/bin/bash
make clean
make
arm-linux-gnueabihf-gcc ledAPP.c -o ledAPP
sudo cp led.ko ledAPP /home/cvvo/linux/nfs/rootfs/lib/modules/4.1.15/ -f
