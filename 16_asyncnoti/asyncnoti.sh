#!/bin/bash
make clean
make
arm-linux-gnueabihf-gcc asyncnotiAPP.c -o asyncnotiAPP
sudo cp asyncnoti.ko asyncnotiAPP /home/cvvo/linux/nfs/rootfs/lib/modules/4.1.15/ -f
