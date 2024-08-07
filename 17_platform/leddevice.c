#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of_gpio.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/irq.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/platform_device.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

/* 寄存器物理地址 */
#define CCM_CCGR1_BASE            (0X020C406C)
#define SW_MUX_GPIO1_IO03_BASE    (0X020E0068)
#define SW_PAD_GPIO1_IO03_BASE    (0X020E02F4)
#define GPIO1_GDIR_BASE           (0X0209C004)
#define GPIO1_DR_BASE             (0X0209C000)

#define REGISTER_LENGTH				4   /* 长度为4字节 */

/* @description		: 释放flatform设备模块的时候此函数会执行	
 * @param - dev 	: 要释放的设备 
 * @return 			: 无
 */
static void led_release(struct device *dev)
{
	printk("led device released!\r\n");	
}

/*  
 * 设备资源信息，也就是LED0所使用的所有寄存器
 */
struct resource led_resources[]={
	[0]={
		.start=CCM_CCGR1_BASE,
		.end=(CCM_CCGR1_BASE+REGISTER_LENGTH-1),
		.flags=IORESOURCE_MEM,
	},
	[1]={
		.start=SW_PAD_GPIO1_IO03_BASE,
		.end=(SW_PAD_GPIO1_IO03_BASE+REGISTER_LENGTH-1),
		.flags=IORESOURCE_MEM,
	},
	[2]={
		.start=SW_PAD_GPIO1_IO03_BASE,
		.end=(SW_PAD_GPIO1_IO03_BASE+REGISTER_LENGTH-1),
		.flags=IORESOURCE_MEM,
	},
	[3]={
		.start=GPIO1_GDIR_BASE,
		.end=(GPIO1_GDIR_BASE+REGISTER_LENGTH-1),
		.flags=IORESOURCE_MEM,
	},
	[4]={
		.start=GPIO1_DR_BASE,
		.end=(GPIO1_DR_BASE+REGISTER_LENGTH-1),
		.flags=IORESOURCE_MEM,
	},
};

/* platform设备结构体  */
static struct platform_device leddevice={
	.name="My_imx6ul_led",
	.id= -1,
	.dev={
		.release=&led_release,
	},
	.num_resources = ARRAY_SIZE(led_resources),   /* ARRAY_SIZE——目的求出数组包含的最大个数 */
	.resource=led_resources,
};


/* 驱动入口 */
static int __init leddevice_init(void)
{
	return platform_device_register(&leddevice);
}

/* 驱动出口 */
static void __exit leddevice_exit(void)
{
	platform_device_unregister(&leddevice);
}

/* 注册驱动加载和卸载 */
module_init(leddevice_init);
module_exit(leddevice_exit);

/* LICENSE和作者信息 */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("CVVO");