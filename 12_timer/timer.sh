#!/bin/bash
make clean
make
arm-linux-gnueabihf-gcc timerAPP.c -o timerAPP
sudo cp timer.ko timerAPP /home/cvvo/linux/nfs/rootfs/lib/modules/4.1.15/ -f
