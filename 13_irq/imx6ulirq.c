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
#include <linux/of_irq.h>
#include <linux/irq.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define IMX6ULIRQ_CNT    1     /* 设备号个数 */
#define IMX6ULIRQ_NAME  "imx6ulirq"   /* 设备名字 */

/* 定义按键值 */
#define KEY0VALUE 	0xF0				/* 按键，被按下的按键值 */
#define KEY0ONE     0xE0                /* 按键被一次按下 */
#define INVAKEY 	0x00				/* 无效按键值，按键没被按下 */
#define KEY_NUM     1                   /* 按键数量 	*/

/* 中断IO描述结构体 */
struct irq_keydesc{
	int gpio_key;   /* gpio编号 */
	int irqnum;   /* 中断号     */
	unsigned char value;  /* 按键对应的键值 */
	char name[10];   /* 中断名字 */
	irqreturn_t (*handler) (int, void *);    /* 中断服务函数 */
};

/* timer设备结构体 */
struct imx6ulirq_dev{
	dev_t devid;   /* 设备号，由dev_t数据类型为（unsigned int） */
	struct cdev cdev;     /* cdev结构体表示字符设备 */
	struct class *class;  /* 类 */
	struct device *device;	/* 设备 */
	int major;   /* 主设备号 */
	int minor;   /* 次设备号 */
	struct device_node *nd;   /* 设备都是以节点的形式“挂”到设备树上的，因此要想获取这个设备的其他属性信息，必须先获取到这个设备的节点。
							 Linux内核使用device_node结构体来描述一个节点 */ 

	int timerperiod;   /* 定时周期，单位为ms，系统频率为100 Hz */
	struct timer_list timer;  /* 定义一个定时器 */

	struct irq_keydesc irqkeydesc[KEY_NUM]; /* 按键描述结构体数组,开发板上只有一个按键，因此irqkeydesc数组只有一个元素*/  

	atomic_t keyvalue;		/* 有效的按键键值 */
	atomic_t releasekey;	/* 表示按键是否被释放，如果按键被释放表示发生了一次完整的按键过程，包括按下和释放 */

	unsigned char curkeynum;	/* 当前的按键号 */
	
};

struct imx6ulirq_dev imx6ulirq;


/* @description		: 中断服务函数，开启定时器，延时10ms，
 *				  	  定时器用于按键消抖。
 * @param - irq 	: 中断号 
 * @param - dev_id	: 设备结构。
 * @return 			: 中断执行结果
 */
static irqreturn_t key0_handler(int irq, void *dev_id)
{	
	struct imx6ulirq_dev *dev=(struct imx6ulirq_dev *)dev_id;
	dev->curkeynum=0;
	/* 开启定时器 */
	dev->timer.data=(unsigned long)dev;  /* 设置要传递给function函数的参数 */
	mod_timer(&dev->timer,jiffies+msecs_to_jiffies(dev->timerperiod));

	return IRQ_RETVAL(IRQ_HANDLED);
	
}


/* static一般用于修饰局部变量，全局变量，函数 */

/* @description	: 定时器服务函数，用于按键消抖，定时器到了以后
 *				  再次读取按键值，如果按键还是处于按下状态就表示按键有效。
 * @param - arg	: 设备结构变量
 * @return 		: 无
 */
void timer_function(unsigned long arg)
{
	struct imx6ulirq_dev *dev=(struct imx6ulirq_dev *)arg;
	unsigned char num;
	struct irq_keydesc *keydesc;

	num=dev->curkeynum;
	keydesc=&dev->irqkeydesc[num];

	if(gpio_get_value(keydesc->gpio_key)==0){  /* 10ms定时器内按键被按下 */
		atomic_set(&dev->keyvalue,KEY0ONE);
	}else{
		atomic_set(&dev->keyvalue,KEY0VALUE);   /* 表示按键值被按下与释放 */
		atomic_set(&dev->releasekey,1);   /* 表示一次有效的按键释放 */
	}
}

 /*
  * @description : 初始化按键 IO， open 函数打开驱动的时候
  *  初始化按键所使用的 GPIO 引脚。
  *  @param : 无
  *  @return : 无
 */
static int keyio_init(void)
{
	int ret=0;
	unsigned char i=0;

	/* 设置key所使用的GPIO */
	/* 1、获取设备节点：key */
	imx6ulirq.nd=of_find_node_by_path("/key"); /* 通过路径来查找指定的节点，path：带有全路径的节点名，可以使用节点的别名 */
	if(imx6ulirq.nd==NULL){
		printk("key node not find!\r\n");
		return -EINVAL;  /* 返回无效参数符号 */
	}else{
		printk("key node find!\r\n");
	}

	/* 2、获取设备树中的gpio属性，得到key所使用的key编号*/
	for(i=0;i<KEY_NUM;i++){
		imx6ulirq.irqkeydesc[i].gpio_key=of_get_named_gpio(imx6ulirq.nd,"key-gpios",i); 
		 /* 类似<&gpio5 7 GPIO_ACTIVE_LOW>的属性信息转换为对应的GPIO编号 */
	/* 函数原型：int of_get_named_gpio(struct device_node *np,const char *propname,int index) */
		if(imx6ulirq.irqkeydesc[i].gpio_key<0){
			printk("can't get key-gpios!\r\n");
			return -EINVAL;  /* 返回无效参数符号 */
		}else{
			printk("key%d-gpios num=%d\r\n",i,imx6ulirq.irqkeydesc[i].gpio_key);
		}
	}

	/* 3、始化key所使用的IO，并且设置成中断模式 */
	for(i=0;i<KEY_NUM;i++){
		memset(imx6ulirq.irqkeydesc[i].name,0,sizeof(imx6ulirq.irqkeydesc[i].name));   /* memset一般使用“0”初始化内存单元,而且通常是给数组或结构体进行初始化 */
		sprintf(imx6ulirq.irqkeydesc[i].name,"KEY%d",i);

		/* 申请IO，申请后能被其他设备检测，避免重复使用 */
		ret=gpio_request(imx6ulirq.irqkeydesc[i].gpio_key,imx6ulirq.irqkeydesc[i].name);
		/* 函数原型： int gpio_request(unsigned gpio, const char *label)*/
		if(ret<0){
			printk("key%d io request fail!\r\n",i);
			return -EINVAL;  /* 返回无效参数符号 */
		}
		/* 设置key gpio为输入 */
		ret=gpio_direction_input(imx6ulirq.irqkeydesc[i].gpio_key);
		/* 函数原型：int gpio_direction_input(unsigned gpio, int value) */
		if(ret<0){
			printk("can't set key%d gpio!\r\n",i);
			return -EINVAL;  /* 返回无效参数符号 */
		}
		/* 获取中断号 */
		imx6ulirq.irqkeydesc[i].irqnum=irq_of_parse_and_map(imx6ulirq.nd,i);

		printk("key%d:gpio=%d, irqnum=%d\r\n",i,imx6ulirq.irqkeydesc[i].gpio_key,imx6ulirq.irqkeydesc[i].irqnum);
	}

	/* 申请中断 */
	imx6ulirq.irqkeydesc[0].handler=key0_handler;
	imx6ulirq.irqkeydesc[0].value = KEY0VALUE;
	

	for(i=0;i<KEY_NUM;i++){
		ret=request_irq(imx6ulirq.irqkeydesc[i].irqnum,imx6ulirq.irqkeydesc[i].handler, IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING,
						imx6ulirq.irqkeydesc[i].name,&imx6ulirq);
		if(ret<0){
			printk("key%d irq-%d request failed!\r\n",i,imx6ulirq.irqkeydesc[i].irqnum);
			return -EFAULT;
		}
	}

	/* 创建定时器 */
	init_timer(&imx6ulirq.timer);
	imx6ulirq.timer.function=timer_function;
	imx6ulirq.timerperiod=10;        /* 定时周期为10ms */

	/* 初始化按键 */
	atomic_set(&imx6ulirq.keyvalue,INVAKEY);  /* 初始化按键没被按下 */
	atomic_set(&imx6ulirq.releasekey,0);    /* 没有一次有效的按键释放 */

	return ret;
}

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int imx6ulirq_open(struct inode *inode, struct file *filp)
{
	int ret=0;
	ret=keyio_init();   /* 初始化ledio */
	if(ret<0){
		return ret;
	}

	filp->private_data=&imx6ulirq;
	return 0;
}

/*
 * @description		: 从设备读取数据 
 * @param - filp 	: 要打开的设备文件(文件描述符)
 * @param - buf 	: 返回给用户空间的数据缓冲区
 * @param - cnt 	: 要读取的数据长度
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 			: 读取的字节数，如果为负值，表示读取失败
 */
static ssize_t imx6ulirq_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
	struct imx6ulirq_dev *dev=filp->private_data;
	unsigned char keyvalue=0;
	unsigned char releasekey=0;
	int ret=0;

	keyvalue=atomic_read(&dev->keyvalue);
	releasekey=atomic_read(&dev->releasekey);

	if(releasekey){   /* 按键被按下 */
		if(keyvalue==KEY0VALUE){   /* 按键为按下和释放 */
			ret=copy_to_user(buf,&keyvalue,sizeof(keyvalue));
		}else{
			return -EINVAL;
		}
		atomic_set(&dev->releasekey,0);   /* 清除标志位 */
	}else{
		return -EINVAL;
	}
	return ret;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int imx6ulirq_release(struct inode *inode, struct file *filp)
{
	struct imx6ulirq_dev *dev=filp->private_data;
	unsigned char i=0;
	del_timer_sync(&dev->timer);	/* 删除timer定时期 */

	/* 释放中断 */
	for(i=0;i<KEY_NUM;i++){
		free_irq(dev->irqkeydesc[i].irqnum,dev);  /* dev为指针参数，不需要取址符号& */
		/* 释放IO,申请和释放配合使用 */
		gpio_free(dev->irqkeydesc[i].gpio_key);
	}
	return 0;
}

/* 设备操作函数 */
static struct file_operations imx6ulirq_fops={
	.owner=THIS_MODULE,
	.open=imx6ulirq_open,
	.read=imx6ulirq_read,
	.release=imx6ulirq_release,
};


/* 驱动入口 */
static int __init imx6ulirq_init(void)
{	
	/* 注册字符设备驱动 */
	/* 1、创建设备号 */
	if(imx6ulirq.major){
		imx6ulirq.devid=MKDEV(imx6ulirq.devid,0);  /* 由高12位的主设备号和低20位的次设备号组成完全设备号 */
		register_chrdev_region(imx6ulirq.devid,IMX6ULIRQ_CNT,IMX6ULIRQ_NAME);
		/* 函数原型为int register_chrdev_region(dev_t from, unsigned count, const char *name) */
		/*要申请的起始设备号，也就是给定的设备号；申请的数量，一般都是一个；设备名字 */
	}else{
		alloc_chrdev_region(&imx6ulirq.devid,0,IMX6ULIRQ_CNT,IMX6ULIRQ_NAME);
		/*函数原型为int alloc_chrdev_region(dev_t *dev,unsigned baseminor,unsigned count,const char *name)*/
		imx6ulirq.major=MAJOR(imx6ulirq.devid);
		imx6ulirq.minor=MINOR(imx6ulirq.devid);
	}
	printk("imx6ulirq major=%d,minor=%d\r\n",imx6ulirq.major,imx6ulirq.minor);

	/* 2、初始化cdev */
	imx6ulirq.cdev.owner=THIS_MODULE;
	cdev_init(&imx6ulirq.cdev,&imx6ulirq_fops);
	/* 函数原型为void cdev_init(struct cdev *cdev, const struct file_operations *fops) */

	/* 3、添加一个cdev*/
	cdev_add(&imx6ulirq.cdev,imx6ulirq.devid,IMX6ULIRQ_CNT);
	/* 函数原型int cdev_add(struct cdev *p, dev_t dev, unsigned count) */

	/* 4、创建类 */
	imx6ulirq.class=class_create(THIS_MODULE,IMX6ULIRQ_NAME);
	/* 函数原型为struct class *class_create (struct module *owner, const char *name) */
	/* IS_ERR()将传入的值与(unsigned long)-MAX_ERRNO比较，MAX_ERRNO为4095，其含义就是最大错误号，-4095补码为0xfffff001，
		即大于等于0xfffff001的指针为非法指针，内核一页4k(4095)记录内核空间的错误指针 */
	if(IS_ERR(imx6ulirq.class)){   /*  判断是否为指针错误，IS_ERR有效指针、空指针返回false，错误指针返回true  */
		return PTR_ERR(imx6ulirq.class);  /* PTR_ERR()将传入的void *类型指针强转为long类型，从而返回出错误类型 */
	}

	/* 5、创建设备 */
	imx6ulirq.device=device_create(imx6ulirq.class,NULL,imx6ulirq.devid,NULL,IMX6ULIRQ_NAME);
	/* 函数原型为struct device *device_create(struct class *cls, struct device *parent,dev_t devt, void *drvdata,const char *fmt, ...); 
	参数class就是设备要创建哪个类下面；参数parent是父设备，一般为NULL;参数devt是设备号；参数drvdata是设备可能会使用的一些数据，一般为NULL；
	参数fmt是设备名字，如果设置fmt=xxx的话，就会生成/dev/xxx这个设备文件 */

	if(IS_ERR(imx6ulirq.device)){   /*  判断是否为指针错误，IS_ERR有效指针、空指针返回false，错误指针返回true  */
		return PTR_ERR(imx6ulirq.device);  /* PTR_ERR()将传入的void *类型指针强转为long类型，从而返回出错误类型 */
	}

	return 0;
}

/* 驱动出口 */
static void __exit imx6ulirq_exit(void)
{

	/* 删除cdev字符设备，采用cdev来描述字符设备 */
	cdev_del(&imx6ulirq.cdev);

	/* 注销设备号 */
	unregister_chrdev_region(imx6ulirq.devid,IMX6ULIRQ_CNT);

	/* 摧毁设备，注意先后顺序 */
	device_destroy(imx6ulirq.class,imx6ulirq.devid);  /* void device_destroy(struct class *cls, dev_t devt); */

	/* 销毁类 */
	class_destroy(imx6ulirq.class);


	printk("timer_exit\r\n");

}

/* 注册驱动加载和卸载 */
module_init(imx6ulirq_init);
module_exit(imx6ulirq_exit);

/* LICENSE和作者信息 */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("CVVO");