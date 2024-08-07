#!/bin/bash
make clean
make
arm-linux-gnueabihf-gcc imx6ulirqAPP.c -o imx6ulirqAPP
sudo cp imx6ulirq.ko imx6ulirqAPP /home/cvvo/linux/nfs/rootfs/lib/modules/4.1.15/ -f
