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
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define TIMER_CNT    1     /* 设备号个数 */
#define TIMER_NAME  "timer"   /* 设备名字 */

#define CLOSE_CMD 		(_IO(0XEF, 0x1))	/* 关闭定时器 */
#define OPEN_CMD		(_IO(0XEF, 0x2))	/* 打开定时器 */
#define SETPERIOD_CMD	(_IO(0XEF, 0x3))	/* 设置定时器周期命令 */
/* 驱动程序中的ioctl接口：long (*unlocked_ioctl) (struct file * fp, unsigned int request, unsigned long args);
 * 对于request，用户与驱动之间的ioctl协议构成 dit(2bit)+size(14bit)+type(8bit)+nr(8bit)
 * 1）dir，表示ioctl命令的访问模式，分为无数据(_IO)、读数据(_IOR)、写数据(_IOW)、读写数据(_IOWR)四种模式。
 * 2）type，表示设备类型，也可翻译成“幻数”或“魔数”，可以是任意一个char型字符，如’a’、‘b’、‘c’等，其主要作用是使ioctl命令具有唯一的设备标识。
 * 3）nr，即number，命令编号/序数，取值范围0~255，在定义了多个ioctl命令的时候，通常从0开始顺次往下编号。
 * 4）size，涉及到ioctl的参数arg，占据13bit或14bit，这个与体系有关，arm使用14bit。用来传递arg的数据类型的长度，比如如果arg是int型，我们就将这个参数填入int，系统会检查数据类型和长度的正确性
 * 分别对应了四个dir：
 * _IO(type, nr)：用来定义不带参数的ioctl命令。
 * _IOR(type,nr,size)：用来定义用户程序向驱动程序写参数的ioctl命令。
 * _IOW(type,nr,size)：用来定义用户程序从驱动程序读参数的ioctl命令。
 * _IOWR(type,nr,size)：用来定义带读写参数的驱动命令。
*/

#define LEDOFF 	0				/* 关灯，定义为字符串 */
#define LEDON 	1				/* 开灯 */

/* timer设备结构体 */
struct timer_dev{
	dev_t devid;   /* 设备号，由dev_t数据类型为（unsigned int） */
	struct cdev cdev;     /* cdev结构体表示字符设备 */
	struct class *class;  /* 类 */
	struct device *device;	/* 设备 */
	int major;   /* 主设备号 */
	int minor;   /* 次设备号 */
	struct device_node *nd;   /* 设备都是以节点的形式“挂”到设备树上的，因此要想获取这个设备的其他属性信息，必须先获取到这个设备的节点。
							 Linux内核使用device_node结构体来描述一个节点 */ 
	int gpio_led;    /* led所使用的GPIO编号 */

	spinlock_t lock;	

	int timerperiod;   /* 定时周期，单位为ms，系统频率为100 Hz */
	struct timer_list timer;  /* 定义一个定时器 */
};

struct timer_dev timerdev;


/*
 * @description : 初始化 LED 灯 IO， open 函数打开驱动的时候
 * 初始化 LED 灯所使用的 GPIO 引脚。
 * @param : 无
 * @return : 无
*/
static int ledio_init(void)
{
	int ret=0;
	/* 设置LED所使用的GPIO */
	/* 1、获取设备节点：gpioled */
	timerdev.nd=of_find_node_by_path("/gpioled"); /* 通过路径来查找指定的节点，path：带有全路径的节点名，可以使用节点的别名 */
	if(timerdev.nd==NULL){
		printk("gpioled node not find!\r\n");
		return -EINVAL;  /* 返回无效参数符号 */
	}else{
		printk("gpioled node find!\r\n");
	}

	/* 2、获取设备树中的gpio属性，得到LED所使用的LED编号*/
	timerdev.gpio_led=of_get_named_gpio(timerdev.nd,"led-gpios",0);  /* 类似<&gpio5 7 GPIO_ACTIVE_LOW>的属性信息转换为对应的GPIO编号 */
	/* 函数原型：int of_get_named_gpio(struct device_node *np,const char *propname,int index) */
	if(timerdev.gpio_led<0){
		printk("can't get gpio_led!\r\n");
		return -EINVAL;  /* 返回无效参数符号 */
	}else{
		printk("gpip_led num=%d\r\n",timerdev.gpio_led);
	}
	
	/* 3、申请IO，申请后能被其他设备检测，避免重复使用 */
	ret=gpio_request(timerdev.gpio_led,"gpio-led");
	/* 函数原型： int gpio_request(unsigned gpio, const char *label)*/
	if(ret<0){
		printk("gpio_led request fail!\r\n");
		return -EINVAL;  /* 返回无效参数符号 */
	}

	/* 4、设置GPIO1_IO03为输出，并且输出高电平，默认关闭LED灯(低电平点亮) */ 
	ret=gpio_direction_output(timerdev.gpio_led,1);
	/* 函数原型：int gpio_direction_output(unsigned gpio, int value) */
	if(ret<0){
		printk("can't set gpio!\r\n");
	}
	return 0;
}

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int timer_open(struct inode *inode, struct file *filp)
{
	int ret=0;
	timerdev.timerperiod=1000;    /* 默认时间为1s */
	filp->private_data=&timerdev;   /* 设置私有数据 */
	ret=ledio_init();   /* 初始化ledio */
	if(ret<0){
		return ret;
	}
	return 0;
}

/*
 * @description : ioctl 函数，
 * @param – filp : 要打开的设备文件(文件描述符)
 * @param - cmd : 应用程序发送过来的命令
 * @param - arg : 参数，设置定时周期ms
 * @return : 0 成功;其他 失败
*/
static long timer_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct timer_dev *dev=filp->private_data;
	unsigned long flags;  /* 自旋锁保护中断状态 */
	int timerperiod;

	switch(cmd){
		case CLOSE_CMD:  /* 关闭定时器 */
			gpio_set_value(dev->gpio_led,1);
			del_timer_sync(&dev->timer);
			break;
		case OPEN_CMD:  /* 打开定时器 */
			spin_lock_irqsave(&dev->lock,flags);  /* 上锁 */
			timerperiod=dev->timerperiod;			/* 临界区 */
			spin_unlock_irqrestore(&dev->lock,flags);  /* 解锁 */
			mod_timer(&dev->timer, jiffies+msecs_to_jiffies(timerperiod)); /* 设置超时时间即设置节拍数 */
			/* jiffies(32位系统)——linux内核使用全局变量来记录系统从启动以来的系统节拍数，其中系统频率为100 Hz(1秒的节拍数);
				msecs_to_jiffies——将毫秒jiffies类型 */
 			break;
		case SETPERIOD_CMD:
			spin_lock_irqsave(&dev->lock,flags);  /* 上锁 */
			dev->timerperiod=arg;
			spin_unlock_irqrestore(&dev->lock,flags);  /* 解锁 */
			mod_timer(&dev->timer, jiffies+msecs_to_jiffies(dev->timerperiod));
			break;
		default:
			break;
	}
	return 0;
}


/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int timer_release(struct inode *inode, struct file *filp)
{
	struct timer_dev *dev=filp->private_data;
	/* 释放IO,申请和释放配合使用 */
	gpio_free(dev->gpio_led);
	return 0;
}


static struct file_operations timer_fops={
	.owner=THIS_MODULE,
	.open=timer_open,
	.unlocked_ioctl=timer_ioctl,
	.release=timer_release,
};

/* static一般用于修饰局部变量，全局变量，函数 */

/* 定时器回调函数，达到超时时间时运行 */
void timer_function(unsigned long arg)
{
	struct timer_dev *dev=(struct timer_dev *)arg;
	int timerperiod;
	unsigned long flags;
	static int sta = 1;

	sta=!sta;
	gpio_set_value(dev->gpio_led,sta);

	/* 重启定时器+自旋锁 */
	spin_lock_irqsave(&dev->lock,flags);  /* 上锁 */
	timerperiod=dev->timerperiod;
	spin_unlock_irqrestore(&dev->lock,flags);  /* 解锁 */
	mod_timer(&dev->timer,jiffies+msecs_to_jiffies(timerperiod));
	
}



/* 驱动入口 */
static int __init timer_init(void)
{	
	/* 初始化自旋锁 */
	spin_lock_init(&timerdev.lock);

	/* 注册字符设备驱动 */
	/* 1、创建设备号 */
	if(timerdev.major){
		timerdev.devid=MKDEV(timerdev.devid,0);  /* 由高12位的主设备号和低20位的次设备号组成完全设备号 */
		register_chrdev_region(timerdev.devid,TIMER_CNT,TIMER_NAME);
		/* 函数原型为int register_chrdev_region(dev_t from, unsigned count, const char *name) */
		/*要申请的起始设备号，也就是给定的设备号；申请的数量，一般都是一个；设备名字 */
	}else{
		alloc_chrdev_region(&timerdev.devid,0,TIMER_CNT,TIMER_NAME);
		/*函数原型为int alloc_chrdev_region(dev_t *dev,unsigned baseminor,unsigned count,const char *name)*/
		timerdev.major=MAJOR(timerdev.devid);
		timerdev.minor=MINOR(timerdev.devid);
	}
	printk("timer major=%d,minor=%d\r\n",timerdev.major,timerdev.minor);

	/* 2、初始化cdev */
	timerdev.cdev.owner=THIS_MODULE;
	cdev_init(&timerdev.cdev,&timer_fops);
	/* 函数原型为void cdev_init(struct cdev *cdev, const struct file_operations *fops) */

	/* 3、添加一个cdev*/
	cdev_add(&timerdev.cdev,timerdev.devid,TIMER_CNT);
	/* 函数原型int cdev_add(struct cdev *p, dev_t dev, unsigned count) */

	/* 4、创建类 */
	timerdev.class=class_create(THIS_MODULE,TIMER_NAME);
	/* 函数原型为struct class *class_create (struct module *owner, const char *name) */
	/* IS_ERR()将传入的值与(unsigned long)-MAX_ERRNO比较，MAX_ERRNO为4095，其含义就是最大错误号，-4095补码为0xfffff001，
		即大于等于0xfffff001的指针为非法指针，内核一页4k(4095)记录内核空间的错误指针 */
	if(IS_ERR(timerdev.class)){   /*  判断是否为指针错误，IS_ERR有效指针、空指针返回false，错误指针返回true  */
		return PTR_ERR(timerdev.class);  /* PTR_ERR()将传入的void *类型指针强转为long类型，从而返回出错误类型 */
	}

	/* 5、创建设备 */
	timerdev.device=device_create(timerdev.class,NULL,timerdev.devid,NULL,TIMER_NAME);
	/* 函数原型为struct device *device_create(struct class *cls, struct device *parent,dev_t devt, void *drvdata,const char *fmt, ...); 
	参数class就是设备要创建哪个类下面；参数parent是父设备，一般为NULL;参数devt是设备号；参数drvdata是设备可能会使用的一些数据，一般为NULL；
	参数fmt是设备名字，如果设置fmt=xxx的话，就会生成/dev/xxx这个设备文件 */

	if(IS_ERR(timerdev.device)){   /*  判断是否为指针错误，IS_ERR有效指针、空指针返回false，错误指针返回true  */
		return PTR_ERR(timerdev.device);  /* PTR_ERR()将传入的void *类型指针强转为long类型，从而返回出错误类型 */
	}


	/* 6、初始化定时器 ,设置定时器处理函数,还未设置周期，所有不会激活定时器 */
	init_timer(&timerdev.timer);
	timerdev.timer.function=timer_function;
	timerdev.timer.data=(unsigned long)&timerdev;

	return 0;
}

/* 驱动出口 */
static void __exit timer_exit(void)
{
	del_timer_sync(&timerdev.timer);		/* 删除timer */

	/* 删除cdev字符设备，采用cdev来描述字符设备 */
	cdev_del(&timerdev.cdev);

	/* 注销设备号 */
	unregister_chrdev_region(timerdev.devid,TIMER_CNT);

	/* 摧毁设备，注意先后顺序 */
	device_destroy(timerdev.class,timerdev.devid);  /* void device_destroy(struct class *cls, dev_t devt); */

	/* 销毁类 */
	class_destroy(timerdev.class);


	printk("timer_exit\r\n");

}

/* 注册驱动加载和卸载 */
module_init(timer_init);
module_exit(timer_exit);

/* LICENSE和作者信息 */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("CVVO");