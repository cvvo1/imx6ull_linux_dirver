#!/bin/bash
make clean
make
arm-linux-gnueabihf-gcc chrdevbaseApp.c -o chrdevbaseApp
sudo cp chrdevbase.ko chrdevbaseAPP /home/cvvo/linux/nfs/rootfs/lib/modules/4.1.15/ -f
