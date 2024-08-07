
KERNELDIR:=/home/cvvo/linux/IMX6ULL/linux_kernel/linux-imx-rel_imx_4.1.15_2.1.0_ga_change   # 表示开发板所使用的Linux内核源码目录

CURRENT_PATH:=$(shell pwd)   # 表示当前路径，直接通过pwd命令获取

obj-m := led.o   # 将led.c这个文件编译为led.ko模块

build: kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules  
# 后面的modules表示编译模块， -C表示将当前的工作目录切换到指定目录中，也就是KERNERLDIR目录，M表示模块源码目录
# “make modules”命令中加入M=dir，程序会自动到指定的dir目录中读取模块的源码并将其编译为.ko文件

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean

