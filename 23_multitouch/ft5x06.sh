#!/bin/bash
make clean
make
sudo cp ft5x06.ko  /home/cvvo/linux/nfs/rootfs/lib/modules/4.1.15/ -f
