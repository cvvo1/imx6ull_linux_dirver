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

#define LEDDEV_CNT    1     /* 设备号个数 */
#define LEDDEV_NAME  "dtsplatled"   /* 设备名字 */

#define LEDOFF 	0				/* 关灯，定义为字符串 */
#define LEDON 	1				/* 开灯 */

/* leddev设备结构体 */
struct leddev_dev{
	dev_t devid;   /* 设备号，由dev_t数据类型为（unsigned int） */
	struct cdev cdev;     /* cdev结构体表示字符设备 */
	struct class *class;  /* 类 */
	struct device *device;	/* 设备 */
	int major;   /* 主设备号 */
	int minor;   /* 次设备号 */
	struct device_node *nd;   /* 设备都是以节点的形式“挂”到设备树上的，因此要想获取这个设备的其他属性信息，必须先获取到这个设备的节点。
							  Linux内核使用device_node结构体来描述一个节点 */ 
	int gpio_led;    /* led所使用的GPIO编号 */

};

struct leddev_dev leddev;


/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int led_open(struct inode *inode, struct file *filp)
{
	filp->private_data=&leddev;   /* 设置私有数据 */
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
static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	int retvalue;
	u8 databuff[1];
	u8 ledstate;

	struct leddev_dev *dev=filp->private_data;

	retvalue=copy_from_user(databuff,buf,cnt);
	if(retvalue){
		printk("kernel write failed!\r\n");
		return -EFAULT;  /* Bad address,错误地址  */
	}

	ledstate=databuff[0];

	if(ledstate==LEDON){
		gpio_set_value(dev->gpio_led,0);	   /* 低电平设置led亮灯 */
	}else if(ledstate==LEDOFF){
		gpio_set_value(dev->gpio_led,1);    /* 高电平led关灯 */
	}

	return 0;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int led_release(struct inode *inode, struct file *filp)
{
	return 0;
}


static struct file_operations leddev_fops={
	.owner=THIS_MODULE,
	.open=led_open,
	.write=led_write,
	.release=led_release,
};

/*
 * @description		: flatform驱动的probe函数，当驱动与
 * 					  设备匹配以后此函数就会执行
 * @param - dev 	: platform设备
 * @return 			: 0，成功;其他负值,失败
 */
static int led_probe(struct platform_device *dev)
{
	int ret=0;

	printk("led driver and device has matched!\r\n");


	/* 设置LED所使用的GPIO */
	/* 1、获取设备节点：leddev */
	leddev.nd=of_find_node_by_path("/gpioled"); /* 通过路径来查找指定的节点，path：带有全路径的节点名，可以使用节点的别名 */
	if(leddev.nd==NULL){
		printk("leddev node not find!\r\n");
		return -EINVAL;  /* 返回无效参数符号 */
	}else{
		printk("leddev node find!\r\n");
	}

	/* 2、获取设备树中的gpio属性，得到LED所使用的LED编号*/
	leddev.gpio_led=of_get_named_gpio(leddev.nd,"led-gpios",0);  /* 类似<&gpio5 7 GPIO_ACTIVE_LOW>的属性信息转换为对应的GPIO编号 */
	/* 函数原型：int of_get_named_gpio(struct device_node *np,const char *propname,int index) */
	if(leddev.gpio_led<0){
		printk("can't get gpio_led!\r\n");
		return -EINVAL;  /* 返回无效参数符号 */
	}else{
		printk("gpip_led num=%d\r\n",leddev.gpio_led);
	}
	
	/* 3、申请IO，申请后能被其他设备检测，避免重复使用 */
	ret=gpio_request(leddev.gpio_led,"gpio-led");
	/* 函数原型： int gpio_request(unsigned gpio, const char *label)*/
	if(ret<0){
		printk("gpio_led request fail!\r\n");
		return -EINVAL;  /* 返回无效参数符号 */
	}

	/* 4、设置GPIO1_IO03为输出，并且输出高电平，默认关闭LED灯(低电平点亮) */ 
	ret=gpio_direction_output(leddev.gpio_led,1);
	/* 函数原型：int gpio_direction_output(unsigned gpio, int value) */
	if(ret<0){
		printk("can't set gpio!\r\n");
	}

	/* 注册字符设备驱动 */
	/* 1、创建设备号 */
	if(leddev.major){
		leddev.devid=MKDEV(leddev.devid,0);  /* 由高12位的主设备号和低20位的次设备号组成完全设备号 */
		register_chrdev_region(leddev.devid,LEDDEV_CNT,LEDDEV_NAME);
		/* 函数原型为int register_chrdev_region(dev_t from, unsigned count, const char *name) */
		/*要申请的起始设备号，也就是给定的设备号；申请的数量，一般都是一个；设备名字 */
	}else{
		alloc_chrdev_region(&leddev.devid,0,LEDDEV_CNT,LEDDEV_NAME);
		/*函数原型为int alloc_chrdev_region(dev_t *dev,unsigned baseminor,unsigned count,const char *name)*/
		leddev.major=MAJOR(leddev.devid);
		leddev.minor=MINOR(leddev.devid);
	}
	printk("leddev major=%d,minor=%d\r\n",leddev.major,leddev.minor);

	/* 2、初始化cdev */
	leddev.cdev.owner=THIS_MODULE;
	cdev_init(&leddev.cdev,&leddev_fops);
	/* 函数原型为void cdev_init(struct cdev *cdev, const struct file_operations *fops) */

	/* 3、添加一个cdev*/
	cdev_add(&leddev.cdev,leddev.devid,LEDDEV_CNT);
	/* 函数原型int cdev_add(struct cdev *p, dev_t dev, unsigned count) */

	/* 4、创建类 */
	leddev.class=class_create(THIS_MODULE,LEDDEV_NAME);
	/* 函数原型为struct class *class_create (struct module *owner, const char *name) */
	/* IS_ERR()将传入的值与(unsigned long)-MAX_ERRNO比较，MAX_ERRNO为4095，其含义就是最大错误号，-4095补码为0xfffff001，
		即大于等于0xfffff001的指针为非法指针，内核一页4k(4095)记录内核空间的错误指针 */
	if(IS_ERR(leddev.class)){   /*  判断是否为指针错误，IS_ERR有效指针、空指针返回false，错误指针返回true  */
		return PTR_ERR(leddev.class);  /* PTR_ERR()将传入的void *类型指针强转为long类型，从而返回出错误类型 */
	}

	/* 5、创建设备 */
	leddev.device=device_create(leddev.class,NULL,leddev.devid,NULL,LEDDEV_NAME);
	/* 函数原型为struct device *device_create(struct class *cls, struct device *parent,dev_t devt, void *drvdata,const char *fmt, ...); 
	参数class就是设备要创建哪个类下面；参数parent是父设备，一般为NULL;参数devt是设备号；参数drvdata是设备可能会使用的一些数据，一般为NULL；
	参数fmt是设备名字，如果设置fmt=xxx的话，就会生成/dev/xxx这个设备文件 */

	if(IS_ERR(leddev.device)){   /*  判断是否为指针错误，IS_ERR有效指针、空指针返回false，错误指针返回true  */
		return PTR_ERR(leddev.device);  /* PTR_ERR()将传入的void *类型指针强转为long类型，从而返回出错误类型 */
	}

	return 0;
}

/*
 * @description		: platform驱动的remove函数，移除platform驱动的时候此函数会执行
 * @param - dev 	: platform设备
 * @return 			: 0，成功;其他负值,失败
 */
static int led_remove(struct platform_device *dev)
{
	/* 卸载驱动的时候关闭LED */ 
	gpio_set_value(leddev.gpio_led,1);

	/* 删除cdev字符设备，采用cdev来描述字符设备 */
	cdev_del(&leddev.cdev);

	/* 注销设备号 */
	unregister_chrdev_region(leddev.devid,LEDDEV_CNT);

	/* 摧毁设备，注意先后顺序 */
	device_destroy(leddev.class,leddev.devid);  /* void device_destroy(struct class *cls, dev_t devt); */

	/* 销毁类 */
	class_destroy(leddev.class);

	/* 释放IO,申请和释放配合使用 */
	gpio_free(leddev.gpio_led);

	printk("led_exit\r\n");
	return 0;
}

static struct of_device_id led_of_match[]={   /* 需要包括最后一个空元素 */
	{.compatible="mytest_gpio"},
	{/* Sentinel */}
};

/* platform驱动结构体 */
static struct platform_driver leddriver={
	.driver={
		.name="My_imx6ul_led",   /* 驱动名字，用于和设备匹配 */
		.of_match_table=led_of_match,
	},
	.probe=led_probe,
	.remove=led_remove,
};

/*
 * @description	: 驱动模块加载函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init leddriver_init(void)
{
	return platform_driver_register(&leddriver);
}

/*
 * @description	: 驱动模块卸载函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit leddriver_exit(void)
{
	platform_driver_unregister(&leddriver);
}

/* 注册驱动加载和卸载 */
module_init(leddriver_init);
module_exit(leddriver_exit);

/* LICENSE和作者信息 */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("CVVO");