#!/bin/bash
make clean
make
arm-linux-gnueabihf-gcc atomicAPP.c -o atomicAPP
sudo cp spinlock.ko atomicAPP /home/cvvo/linux/nfs/rootfs/lib/modules/4.1.15/ -f
