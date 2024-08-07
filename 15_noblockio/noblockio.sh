#!/bin/bash
make clean
make
arm-linux-gnueabihf-gcc noblockioAPP.c -o noblockioAPP
sudo cp noblockio.ko noblockioAPP /home/cvvo/linux/nfs/rootfs/lib/modules/4.1.15/ -f
