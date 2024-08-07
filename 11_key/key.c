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
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define KEY_CNT    1     /* 设备号个数 */
#define KEY_NAME  "key"   /* 设备名字 */

/* 定义按键值 */
#define KEY0VALUE 	0xF0				/* 按键值 */
#define INVAKEY 	0x00				/* 无效按键值 */

/* key设备结构体 */
struct key_dev{
	dev_t devid;   /* 设备号，由dev_t数据类型为（unsigned int） */
	struct cdev cdev;     /* cdev结构体表示字符设备 */
	struct class *class;  /* 类 */
	struct device *device;	/* 设备 */
	int major;   /* 主设备号 */
	int minor;   /* 次设备号 */
	struct device_node *nd;   /* 设备都是以节点的形式“挂”到设备树上的，因此要想获取这个设备的其他属性信息，必须先获取到这个设备的节点。
							 Linux内核使用device_node结构体来描述一个节点 */ 
	int gpio_key;    /* key所使用的GPIO编号 */
	atomic_t keyvalue;
};

struct key_dev key;


 /*
  * @description : 初始化按键 IO， open 函数打开驱动的时候
  *  初始化按键所使用的 GPIO 引脚。
  *  @param : 无
  *  @return : 无
 */

static int key_gpio_init(void)
{
	int ret=0;

	/* 设置key所使用的GPIO */
	/* 1、获取设备节点：key */
	key.nd=of_find_node_by_path("/key"); /* 通过路径来查找指定的节点，path：带有全路径的节点名，可以使用节点的别名 */
	if(key.nd==NULL){
		printk("key node not find!\r\n");
		return -EINVAL;  /* 返回无效参数符号 */
	}else{
		printk("key node find!\r\n");
	}

	/* 2、获取设备树中的gpio属性，得到key所使用的key编号*/
	key.gpio_key=of_get_named_gpio(key.nd,"key-gpios",0);  /* 类似<&gpio5 7 GPIO_ACTIVE_LOW>的属性信息转换为对应的GPIO编号 */
	/* 函数原型：int of_get_named_gpio(struct device_node *np,const char *propname,int index) */
	if(key.gpio_key<0){
		printk("can't get gpio_key!\r\n");
		return -EINVAL;  /* 返回无效参数符号 */
	}else{
		printk("gpip_key num=%d\r\n",key.gpio_key);
	}
	
	/* 3、申请IO，申请后能被其他设备检测，避免重复使用 */
	ret=gpio_request(key.gpio_key,"gpio-key");
	/* 函数原型： int gpio_request(unsigned gpio, const char *label)*/
	if(ret<0){
		printk("gpio_key request fail!\r\n");
		return -EINVAL;  /* 返回无效参数符号 */
	}

	/* 4、设置GPIO1_IO03为输出，并且输出高电平，默认关闭key灯(低电平点亮) */ 
	ret=gpio_direction_input(key.gpio_key);    /* 设置为输入 */
	/* 函数原型：int gpio_direction_input(unsigned gpio, int value) */
	if(ret<0){
		printk("can't set gpio!\r\n");
	}
	return ret;
}



/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int key_open(struct inode *inode, struct file *filp)
{
	int ret=0;
	filp->private_data=&key;   /* 设置私有数据 */
	ret=key_gpio_init();
	if(ret<0){
		return ret;
	}
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
static ssize_t key_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
	int ret=0;
	unsigned char value=0;
	struct key_dev *dev=filp->private_data;
	if(gpio_get_value(dev->gpio_key)==0){
		while(!gpio_get_value(dev->gpio_key));   /* 等待按键释放，按键默认为高电平，按下后为低电平 */
		atomic_set(&dev->keyvalue,KEY0VALUE);
	}else{
		atomic_set(&dev->keyvalue,INVAKEY);      /* 无效按键值 */
	}

	value=atomic_read(&dev->keyvalue);
	ret=copy_to_user(buf,&value,sizeof(value));
	/* 应用程序不能直接访问内核数据，驱动给应用传递数据需要用到copy_to_user，
			函数原型为：static inline long copy_to_user(void __user *to, const void *from, unsigned long n)
			to表示目的，from表示源，n表示要复制的数据长度。复制成功，返回值为0，如果复制失败则返回负数 */
	return ret;
}


/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int key_release(struct inode *inode, struct file *filp)
{
	struct key_dev *dev=filp->private_data;
	/* 释放IO,申请和释放配合使用 */
	gpio_free(dev->gpio_key);
	
	return 0;
}


static struct file_operations key_fops={
	.owner=THIS_MODULE,
	.open=key_open,
	.read=key_read,
	.release=key_release,
};


/* 驱动入口 */
static int __init mykey_init(void)
{
	
	/* 注册字符设备驱动 */
	/* 1、创建设备号 */
	if(key.major){
		key.devid=MKDEV(key.devid,0);  /* 由高12位的主设备号和低20位的次设备号组成完全设备号 */
		register_chrdev_region(key.devid,KEY_CNT,KEY_NAME);
		/* 函数原型为int register_chrdev_region(dev_t from, unsigned count, const char *name) */
		/*要申请的起始设备号，也就是给定的设备号；申请的数量，一般都是一个；设备名字 */
	}else{
		alloc_chrdev_region(&key.devid,0,KEY_CNT,KEY_NAME);
		/*函数原型为int alloc_chrdev_region(dev_t *dev,unsigned baseminor,unsigned count,const char *name)*/
		key.major=MAJOR(key.devid);
		key.minor=MINOR(key.devid);
	}
	printk("key major=%d,minor=%d\r\n",key.major,key.minor);

	/* 2、初始化cdev */
	key.cdev.owner=THIS_MODULE;
	cdev_init(&key.cdev,&key_fops);
	/* 函数原型为void cdev_init(struct cdev *cdev, const struct file_operations *fops) */

	/* 3、添加一个cdev*/
	cdev_add(&key.cdev,key.devid,KEY_CNT);
	/* 函数原型int cdev_add(struct cdev *p, dev_t dev, unsigned count) */

	/* 4、创建类 */
	key.class=class_create(THIS_MODULE,KEY_NAME);
	/* 函数原型为struct class *class_create (struct module *owner, const char *name) */
	/* IS_ERR()将传入的值与(unsigned long)-MAX_ERRNO比较，MAX_ERRNO为4095，其含义就是最大错误号，-4095补码为0xfffff001，
		即大于等于0xfffff001的指针为非法指针，内核一页4k(4095)记录内核空间的错误指针 */
	if(IS_ERR(key.class)){   /*  判断是否为指针错误，IS_ERR有效指针、空指针返回false，错误指针返回true  */
		return PTR_ERR(key.class);  /* PTR_ERR()将传入的void *类型指针强转为long类型，从而返回出错误类型 */
	}

	/* 5、创建设备 */
	key.device=device_create(key.class,NULL,key.devid,NULL,KEY_NAME);
	/* 函数原型为struct device *device_create(struct class *cls, struct device *parent,dev_t devt, void *drvdata,const char *fmt, ...); 
	参数class就是设备要创建哪个类下面；参数parent是父设备，一般为NULL;参数devt是设备号；参数drvdata是设备可能会使用的一些数据，一般为NULL；
	参数fmt是设备名字，如果设置fmt=xxx的话，就会生成/dev/xxx这个设备文件 */

	if(IS_ERR(key.device)){   /*  判断是否为指针错误，IS_ERR有效指针、空指针返回false，错误指针返回true  */
		return PTR_ERR(key.device);  /* PTR_ERR()将传入的void *类型指针强转为long类型，从而返回出错误类型 */
	}

	return 0;
}

/* 驱动出口 */
static void __exit mykey_exit(void)
{
	/* 删除cdev字符设备，采用cdev来描述字符设备 */
	cdev_del(&key.cdev);

	/* 注销设备号 */
	unregister_chrdev_region(key.devid,KEY_CNT);

	/* 摧毁设备，注意先后顺序 */
	device_destroy(key.class,key.devid);  /* void device_destroy(struct class *cls, dev_t devt); */

	/* 销毁类 */
	class_destroy(key.class);



	printk("key_exit\r\n");

}

/* 注册驱动加载和卸载 */
module_init(mykey_init);
module_exit(mykey_exit);

/* LICENSE和作者信息 */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("CVVO");