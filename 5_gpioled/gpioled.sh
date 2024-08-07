#!/bin/bash
make clean
make
arm-linux-gnueabihf-gcc gpioledAPP.c -o gpioledAPP
sudo cp gpioled.ko gpioledAPP /home/cvvo/linux/nfs/rootfs/lib/modules/4.1.15/ -f
