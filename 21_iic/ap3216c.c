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
#include <linux/i2c.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include "ap3216creg.h"

#define AP3216C_CNT    1     /* 设备号个数 */
#define AP3216C_NAME  "ap3216c"   /* 设备名字 */

/* ap3216c设备结构体 */
struct ap3216c_dev{
	dev_t devid;   /* 设备号，由dev_t数据类型为（unsigned int） */
	struct cdev cdev;     /* cdev结构体表示字符设备 */
	struct class *class;  /* 类 */
	struct device *device;	/* 设备 */
	int major;   /* 主设备号 */
	int minor;   /* 次设备号 */
	struct device_node *nd;   /* 设备都是以节点的形式“挂”到设备树上的，因此要想获取这个设备的其他属性信息，必须先获取到这个设备的节点。
							  Linux内核使用device_node结构体来描述一个节点 */ 
	void *private_data;	/* 私有数据 */
	unsigned short ir,als,ps;		/* 三个光传感器数据 */
};

struct ap3216c_dev ap3216c;

/*
 * @description	: 从ap3216c读取多个寄存器数据，此函数在测试其他 I2C 设备的时候可以实现多给字节连续读取，但是在 AP3216C 上不能连续读取多个字节。不过读取一个字节没有问题的
 * @param - dev:  ap3216c设备
 * @param - reg:  要读取的寄存器首地址
 * @param - val:  读取到的数据
 * @param - len:  要读取的数据长度
 * @return 		: 操作结果
 */
static int ap3216c_read_regs(struct ap3216c_dev *dev,u8 reg,void *val, int len)
{
	int ret=0;
	struct i2c_msg msg[2];
	struct i2c_client *client =(struct i2c_client *)dev->private_data;
	/* i2c读数据时序，发送要读取的I2C从设备地址(写信号)->主机发送要读取的寄存器地址+写位->重新发送要读取的I2C从设备地址(读信号)->从I2C器件里面读取到的数据 */

	/* msg[0]为发送要读取的寄存器地址 */
	msg[0].addr=client->addr;     /* 驱动设设备匹配后进入probe的i2c_client *client包含了从机地址 */
	msg[0].flags=0;               /* 标记为写数据 */
	msg[0].buf=&reg;			/* 读取的寄存器地址 */
	msg[0].len=1;   

	/* msg[1]读取数据 */
	msg[1].addr=client->addr; 
	msg[1].flags=I2C_M_RD;     /* 标记为读取数据*/
	msg[1].buf=val;            /* 读取数据缓冲区 */
	msg[1].len=len;           /* 要读取的数据长度*/

	ret=i2c_transfer(client->adapter,msg,2);
	/* 原型extern int i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num);——num： 消息数量，也就是 msgs 的数量 */
	if(ret==2){
		ret=0;
	}else{
		printk("i2c rd failed=%d reg=%06x len=%d\n",ret, reg, len);
		ret = -EREMOTEIO;
	}
	return ret;
}

/*
 * @description	: 向ap3216c多个寄存器写入数据
 * @param - dev:  ap3216c设备
 * @param - reg:  要写入的寄存器首地址
 * @param - val:  要写入的数据缓冲区
 * @param - len:  要写入的数据长度
 * @return 	  :   操作结果
 */
static signed int ap3216c_write_regs(struct ap3216c_dev *dev, u8 reg, u8 *val,u8 len)
{
	u8 buf[256];
	struct i2c_msg msg;
	struct i2c_client *client =(struct i2c_client *)dev->private_data;
	/* i2c写数据时序，发送I2C设备地址->发送要写入数据的寄存器地址->发送要写入寄存器的数据 */
	buf[0]=reg;
	memcpy(&buf[1],val,len);    /* 将要写入的数据拷贝到数组b里面 */
	/* 函数原型：void *memcpy(void *destin, void *source, unsigned n)——以source指向的地址为起点，将连续的n个字节数据，复制到以destin指向的地址为起点的内存中。*/

	msg.flags=0;						/* 标记为写数据 */
	msg.addr=client->addr;              /* 驱动设设备匹配后进入probe的i2c_client *client包含了从机地址 */

	msg.len=len+1;
	msg.buf=buf;

	return i2c_transfer(client->adapter,&msg,1);
	/* 原型extern int i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num); */
}

/*
 * @description	: 读取ap3216c指定寄存器值，读取一个寄存器
 * @param - dev:  ap3216c设备
 * @param - reg:  要读取的寄存器
 * @return 	  :   读取到的寄存器值
 */
static u8 ap3216c_read_reg(struct ap3216c_dev *dev,u8 reg)
{
	u8 buf=0;
	ap3216c_read_regs(dev,reg,&buf,1);

	return buf;
}


/*
 * @description	: 向ap3216c指定寄存器写入指定的值，写一个寄存器
 * @param - dev:  ap3216c设备
 * @param - reg:  要写的寄存器
 * @param - data: 要写入的值
 * @return   :    无
 */
static void ap3216c_write_reg(struct ap3216c_dev *dev, u8 reg, u8 data)
{
	unsigned char buf=0;
	buf=data;
	ap3216c_write_regs(dev,reg,&buf,1);    /* 写入一个寄存器值 */
}


/*
 * @description	: 读取AP3216C的数据，读取原始数据，包括ALS,PS和IR, 注意！
 *				: 如果同时打开ALS,IR+PS的话两次数据读取的时间间隔要大于112.5ms
 * @param - ir	: ir数据
 * @param - ps 	: ps数据
 * @param - ps 	: als数据 
 * @return 		: 无。
 */
void ap3216c_readdata(struct ap3216c_dev *dev)
{	
	unsigned char i=0;
	unsigned char buf[6];

	/* 循环读取所有传感器数据 */
	for(i=0;i<6;i++){
		buf[i]=ap3216c_read_reg(dev,AP3216C_IRDATALOW + i);
	}

	if(buf[0]&0x80)    /* IR数据低字节，bit(7)位为0：IR&PS 数据有效，1:无效 */{
		dev->ir=0;
	}else{
		dev->ir=((unsigned short)buf[1]<<2)|(buf[0]&0x03);
	}

	dev->als = ((unsigned short)buf[3] << 8) | buf[2];	/* 读取ALS传感器的数据 			 */  

	if(buf[4]&0x40){   /* PS数据低字节，bit(6)位0：IR&PS数据有效，1：IR&PS数据无效，bit(3:0)，PS低4位数据*/ 
		dev->ps=0;
	}else{
		dev->ps=((unsigned short)buf[5]&0x3f<<4)|(buf[4]&0x0f);
	}

}

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int ap3216c_open(struct inode *inode, struct file *filp)
{
	u8 ret=0;
	filp->private_data=&ap3216c;   /* 设置私有数据 */
	ap3216c_write_reg(filp->private_data,AP3216C_SYSTEMCONG,0x04);   /* 100-软复位，复位AP3216C */
	mdelay(50);
	ap3216c_write_reg(filp->private_data,AP3216C_SYSTEMCONG,0x03);   /* 开启ALS、PS+IR ,011-使能 ALS+PS+IR */

	ret=ap3216c_read_reg(filp->private_data,AP3216C_SYSTEMCONG);

	printk("AP3216C_SYSTEMCONG data=%d\r\n",ret);
	if(ret!=0x03){
		return -EINVAL;   /* 返回错误码 */
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
static ssize_t ap3216c_read(struct file *filp, char __user *buf, size_t cnt, loff_t *off)  
{	
	unsigned short data[3];
	int ret=0;
	struct ap3216c_dev *dev=(struct ap3216c_dev *)filp->private_data;
	ap3216c_readdata(dev);

	data[0]=dev->ir;
	data[1]=dev->als;
	data[2]=dev->ps;

	ret=copy_to_user(buf,data,sizeof(data));
	if(ret){
		return -EINVAL;   /* 返回错误码 */
		printk("read ap3216c failed!\r\n");
	}
	return ret;
}


/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int ap3216c_release(struct inode *inode, struct file *filp)
{
	return 0;
}


static struct file_operations ap3216c_fops={
	.owner=THIS_MODULE,
	.open=ap3216c_open,
	.read=ap3216c_read,
	.release=ap3216c_release,
};

 /*
  * @description     : i2c驱动的probe函数，当驱动与
  *                    设备匹配以后此函数就会执行
  * @param - client  : i2c设备
  * @param - id      : i2c设备ID
  * @return          : 0，成功;其他负值,失败
  */
static int ap3216c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	printk("led driver and device has matched!\r\n");
	/* 注册字符设备驱动 */
	/* 1、创建设备号 */
	if(ap3216c.major){
		ap3216c.devid=MKDEV(ap3216c.devid,0);  /* 由高12位的主设备号和低20位的次设备号组成完全设备号 */
		register_chrdev_region(ap3216c.devid,AP3216C_CNT,AP3216C_NAME);
		/* 函数原型为int register_chrdev_region(dev_t from, unsigned count, const char *name) */
		/*要申请的起始设备号，也就是给定的设备号；申请的数量，一般都是一个；设备名字 */
	}else{
		alloc_chrdev_region(&ap3216c.devid,0,AP3216C_CNT,AP3216C_NAME);
		/*函数原型为int alloc_chrdev_region(dev_t *dev,unsigned baseminor,unsigned count,const char *name)*/
		ap3216c.major=MAJOR(ap3216c.devid);
		ap3216c.minor=MINOR(ap3216c.devid);
	}
	printk("ap3216c major=%d,minor=%d\r\n",ap3216c.major,ap3216c.minor);

	/* 2、初始化cdev */
	ap3216c.cdev.owner=THIS_MODULE;
	cdev_init(&ap3216c.cdev,&ap3216c_fops);
	/* 函数原型为void cdev_init(struct cdev *cdev, const struct file_operations *fops) */

	/* 3、添加一个cdev*/
	cdev_add(&ap3216c.cdev,ap3216c.devid,AP3216C_CNT);
	/* 函数原型int cdev_add(struct cdev *p, dev_t dev, unsigned count) */

	/* 4、创建类 */
	ap3216c.class=class_create(THIS_MODULE,AP3216C_NAME);
	/* 函数原型为struct class *class_create (struct module *owner, const char *name) */
	/* IS_ERR()将传入的值与(unsigned long)-MAX_ERRNO比较，MAX_ERRNO为4095，其含义就是最大错误号，-4095补码为0xfffff001，
		即大于等于0xfffff001的指针为非法指针，内核一页4k(4095)记录内核空间的错误指针 */
	if(IS_ERR(ap3216c.class)){   /*  判断是否为指针错误，IS_ERR有效指针、空指针返回false，错误指针返回true  */
		return PTR_ERR(ap3216c.class);  /* PTR_ERR()将传入的void *类型指针强转为long类型，从而返回出错误类型 */
	}

	/* 5、创建设备 */
	ap3216c.device=device_create(ap3216c.class,NULL,ap3216c.devid,NULL,AP3216C_NAME);
	/* 函数原型为struct device *device_create(struct class *cls, struct device *parent,dev_t devt, void *drvdata,const char *fmt, ...); 
	参数class就是设备要创建哪个类下面；参数parent是父设备，一般为NULL;参数devt是设备号；参数drvdata是设备可能会使用的一些数据，一般为NULL；
	参数fmt是设备名字，如果设置fmt=xxx的话，就会生成/dev/xxx这个设备文件 */

	if(IS_ERR(ap3216c.device)){   /*  判断是否为指针错误，IS_ERR有效指针、空指针返回false，错误指针返回true  */
		return PTR_ERR(ap3216c.device);  /* PTR_ERR()将传入的void *类型指针强转为long类型，从而返回出错误类型 */
	}

	ap3216c.private_data=client;

	return 0;
}

/*
 * @description     : i2c驱动的remove函数，移除i2c驱动的时候此函数会执行
 * @param - client 	: i2c设备
 * @return          : 0，成功;其他负值,失败
 */
static int ap3216c_remove(struct i2c_client *client)
{
	/* 删除cdev字符设备，采用cdev来描述字符设备 */
	cdev_del(&ap3216c.cdev);

	/* 注销设备号 */
	unregister_chrdev_region(ap3216c.devid,AP3216C_CNT);

	/* 摧毁设备，注意先后顺序 */
	device_destroy(ap3216c.class,ap3216c.devid);  /* void device_destroy(struct class *cls, dev_t devt); */

	/* 销毁类 */
	class_destroy(ap3216c.class);

	printk("led_exit\r\n");
	return 0;
}

static const struct i2c_device_id ap3216c_id[]={
	{"myi2c,ap3216c", 0},
	{/* Sentinel */}
};

static const struct of_device_id ap3216c_of_match[]={   /* 需要包括最后一个空元素 */
	{.compatible="myi2c,ap3216c"},
	{/* Sentinel */}
};

/* platform驱动结构体 */
static struct i2c_driver ap3216c_driver={
	.probe=ap3216c_probe,
	.remove=ap3216c_remove,
	.driver={
		.owner=THIS_MODULE,
		.name="ap3216c",       /* 驱动名字，用于和设备匹配 */
		.of_match_table=ap3216c_of_match,
		},
	.id_table=ap3216c_id,
};

/*
 * @description	: 驱动模块加载函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init ap3116c_init(void)
{
	int ret=0;
	ret=i2c_add_driver(&ap3216c_driver);
	return ret;
}

/*
 * @description	: 驱动模块卸载函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit ap3116c_exit(void)
{
	i2c_del_driver(&ap3216c_driver);
}

/* 注册驱动加载和卸载 */
module_init(ap3116c_init);
module_exit(ap3116c_exit);

/* LICENSE和作者信息 */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("CVVO");