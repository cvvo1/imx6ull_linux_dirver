#!/bin/bash
make clean
make
sudo cp ramdisk_norequest.ko  /home/cvvo/linux/nfs/rootfs/lib/modules/4.1.15/ -f
