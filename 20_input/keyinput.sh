#!/bin/bash
make clean
make
arm-linux-gnueabihf-gcc keyinputAPP.c -o keyinputAPP
sudo cp keyinput.ko keyinputAPP /home/cvvo/linux/nfs/rootfs/lib/modules/4.1.15/ -f
