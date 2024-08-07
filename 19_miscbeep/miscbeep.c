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
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define MISCBEEP_NAME  "miscbeep"   /* 设备名字 */
#define MISCBEEP_MINOR    141       /* 子设备号 */

#define BEEPOFF 	0				/* 关峰鸣器，定义为字符串 */
#define BEEPON 	1				/* 开峰鸣器 */

/* miscbeep设备结构体 */
struct miscbeep_dev{
	dev_t devid;   /* 设备号，由dev_t数据类型为（unsigned int） */
	struct cdev cdev;     /* cdev结构体表示字符设备 */
	struct class *class;  /* 类 */
	struct device *device;	/* 设备 */
	struct device_node *nd;   /* 设备都是以节点的形式“挂”到设备树上的，因此要想获取这个设备的其他属性信息，必须先获取到这个设备的节点。
							 Linux内核使用device_node结构体来描述一个节点 */ 
	int gpio_beep;    /* miscbeep所使用的GPIO编号 */
};

struct miscbeep_dev miscbeep;


/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int miscbeep_open(struct inode *inode, struct file *filp)
{
	filp->private_data=&miscbeep;   /* 设置私有数据 */
	return 0;
}


/*
 * @description		: 向设备写数据 
 * @param - filp 	: 设备文件，表示打开的文件描述符
 * @param - buf 	: 要写给设备写入的数据
 * @param - cnt 	: 要写入的数据长度
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 			: 写入的字节数，如果为负值，表示写入失败
 */
static ssize_t miscbeep_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	int retvalue;
	u8 databuff[1];
	u8 miscbeepstate;
	struct miscbeep_dev *dev=filp->private_data;

	retvalue=copy_from_user(databuff,buf,cnt);
	if(retvalue<0){
		printk("kernel write faimiscbeep!\r\n");
		return -EFAULT;  /* Bad address,错误地址  */
	}

	miscbeepstate=databuff[0];

	if(miscbeepstate==BEEPON){
		gpio_set_value(dev->gpio_beep,0);	   /* 输出高电平，关闭蜂鸣器 */
	}else if(miscbeepstate==BEEPOFF){
		gpio_set_value(dev->gpio_beep,1);    /* 高电平miscbeep开灯 */
	}

	return 0;
}

static struct file_operations miscbeep_fops={
	.owner=THIS_MODULE,
	.open=miscbeep_open,
	.write=miscbeep_write,
};

/* MISC设备结构体，MISC 设备的主设备号为 10 */
static struct miscdevice miscdev_beep={
	.minor=MISCBEEP_MINOR,   /* 子设备号 */
	.name=MISCBEEP_NAME,    /* 设备名字 */
	.fops=&miscbeep_fops,  /* 设备操作集 */
};

 /*
  * @description     : flatform驱动的probe函数，当驱动与
  *                    设备匹配以后此函数就会执行
  * @param - dev     : platform设备
  * @return          : 0，成功;其他负值,失败
  */
static int miscbeep_probe(struct platform_device *dev)
{
	int ret=0;

	printk("led driver and device has matched!\r\n");
	
	/* 设置miscbeep所使用的GPIO */
	/* 1、获取设备节点：miscbeep */
	miscbeep.nd=of_find_node_by_path("/beep"); /* 通过路径来查找指定的节点，path：带有全路径的节点名，可以使用节点的别名 */
	if(miscbeep.nd==NULL){
		printk("miscbeep node not find!\r\n");
		return -EINVAL;  /* 返回无效参数符号 */
	}else{
		printk("miscbeep node find!\r\n");
	}

	/* 2、获取设备树中的gpio属性，得到miscbeep所使用的miscbeep编号*/
	miscbeep.gpio_beep=of_get_named_gpio(miscbeep.nd,"beep-gpios",0);  /* 类似<&gpio5 7 GPIO_ACTIVE_LOW>的属性信息转换为对应的GPIO编号 */
	/* 函数原型：int of_get_named_gpio(struct device_node *np,const char *propname,int index) */
	if(miscbeep.gpio_beep<0){
		printk("can't get gpio_beep!\r\n");
		return -EINVAL;  /* 返回无效参数符号 */
	}else{
		printk("gpio_beep num=%d\r\n",miscbeep.gpio_beep);
	}
	
	/* 3、申请IO，申请后能被其他设备检测，避免重复使用 */
	ret=gpio_request(miscbeep.gpio_beep,"gpio-beep");
	/* 函数原型： int gpio_request(unsigned gpio, const char *label)*/
	if(ret<0){
		printk("gpio_beep request fail!\r\n");
		return -EINVAL;  /* 返回无效参数符号 */
	}

	/* 4、设置GPIO5_IO01为输出，并且输出低电平，默认关闭miscbeep(低电平峰鸣) */ 
	ret=gpio_direction_output(miscbeep.gpio_beep,1);
	/* 函数原型：int gpio_direction_output(unsigned gpio, int value) */
	if(ret<0){
		printk("can't set gpio!\r\n");
	}


	/* 一般情况下会注册对应的字符设备，但是这里我们使用MISC设备所以我们不需要自己注册字符设备驱动，只需要注册misc设备驱动即可*/
	ret=misc_register(&miscdev_beep);
	if(ret<0){
		printk("misc device register failed!\r\n");
		return -EFAULT;
	}
	return 0;
}

/*
 * @description     : platform驱动的remove函数，移除platform驱动的时候此函数会执行
 * @param - dev     : platform设备
 * @return          : 0，成功;其他负值,失败
 */
static int miscbeep_remove(struct platform_device *dev)
{
	gpio_set_value(miscbeep.gpio_beep, 1);  /* 注销设备的时候关闭LED灯 */
	
	/* 释放IO,申请和释放配合使用 */
	gpio_free(miscbeep.gpio_beep);

	misc_deregister(&miscdev_beep);    /* 注销掉 MISC 设备 */
	printk("miscbeep exit\r\n");
	return 0;
}

static const struct of_device_id beep_of_match[]={
	{.compatible="mytest_beep"},
	{/* Sentinel */}
};

static struct platform_driver miscbeep_driver={
	.driver={
		.name="My_imx6ul_beep",   /* 驱动名字，用于和设备匹配(无设备数方法) */
		.of_match_table=beep_of_match,
	},
	.probe=miscbeep_probe,
	.remove=miscbeep_remove,
};

/* 驱动入口 */
static int __init miscbeep_init(void)
{
	return platform_driver_register(&miscbeep_driver);
}

/* 驱动出口 */
static void __exit miscbeep_exit(void)
{
	platform_driver_unregister(&miscbeep_driver);
}

/* 注册驱动加载和卸载 */
module_init(miscbeep_init);
module_exit(miscbeep_exit);

/* LICENSE和作者信息 */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("CVVO");