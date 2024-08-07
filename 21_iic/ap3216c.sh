#!/bin/bash
make clean
make
arm-linux-gnueabihf-gcc ap3216cAPP.c -o ap3216cAPP
sudo cp ap3216c.ko ap3216cAPP /home/cvvo/linux/nfs/rootfs/lib/modules/4.1.15/ -f
