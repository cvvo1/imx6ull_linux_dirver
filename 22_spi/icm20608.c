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
#include <linux/spi/spi.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include "icm20608reg.h"

#define ICM20608_CNT    1     /* 设备号个数 */
#define ICM20608_NAME  "icm20608"   /* 设备名字 */

/* icm20608设备结构体 */
struct icm20608_dev{
	dev_t devid;   /* 设备号，由dev_t数据类型为（unsigned int） */
	struct cdev cdev;     /* cdev结构体表示字符设备 */
	struct class *class;  /* 类 */
	struct device *device;	/* 设备 */
	int major;   /* 主设备号 */
	int minor;   /* 次设备号 */
	struct device_node *nd;   /* 设备都是以节点的形式“挂”到设备树上的，因此要想获取这个设备的其他属性信息，必须先获取到这个设备的节点。
							  Linux内核使用device_node结构体来描述一个节点 */ 
	int cs_gpio;
	void *private_data;	/* 私有数据 */
	s16 readdata[7];

	#if 0
	s16 accel_x_adc;    /* 加速度计X轴原始值 	*/
	s16 accel_y_adc;	/* 加速度计Y轴原始值	*/
	s16 accel_z_adc;	/* 加速度计Z轴原始值 	*/
	s16 atemp_adc;		/* 温度原始值 			*/
	s16 gyro_x_adc;		/* 陀螺仪X轴原始值 	 */
	s16 gyro_y_adc;		/* 陀螺仪Y轴原始值		*/
	s16 gyro_z_adc;		/* 陀螺仪Z轴原始值 		*/
	#endif
};

struct icm20608_dev icm20608;


/* ICM20608在使用SPI接口的时候寄存器地址只有低7位有效,寄存器地址最高位是读/写标志位读的时候要为1，写的时候要为0 */ 
/*
 * @description	: 从icm20608读取多个寄存器数据
 * @param - dev:  icm20608设备
 * @param - reg:  要读取的寄存器首地址
 * @param - val:  读取到的数据
 * @param - len:  要读取的数据长度
 * @return 		: 操作结果
 */
static int icm20608_read_regs(struct icm20608_dev *dev,u8 reg,void *val, int len)
{
	int ret=-1;
	u8 txdata[1];   /* 发送数据 */
	u8 *rxdata;   /* 接受数据 */
	struct spi_message m;
	struct spi_transfer *t;
	struct spi_device *spi=(struct spi_device *)dev->private_data;
	/* 申请内存 ??? */
	t=kzalloc(sizeof(struct spi_transfer),GFP_KERNEL);    /* GFP_KERNEL——正常分配内存 */
	if(!t){
		return -ENOMEM;   /* 返回Out of memory  */
	}
	rxdata=kzalloc(sizeof(char) * (len+1),GFP_KERNEL); 
	if(!rxdata){
		goto out1;
	}
	/* 一共发送len+1个字节的数据，第一个字节为寄存器首地址，一共要读取len个字节长度的数据 */
	txdata[0]=reg|0x80;
	t->tx_buf=txdata;  /* 要发送的数据 */
	t->rx_buf=rxdata;  /* 要读取的数据 */
	t->len=len+1;      /* t->len=发送的长度+读取的长度 */

	spi_message_init(&m);   /* spi_message之前需要对其进行初始化 */
	/* void spi_message_init(struct spi_message *m) */
	spi_message_add_tail(t,&m);  /*  spi_transfer 添加到 spi_message 队列中 */
	/* void spi_message_add_tail(struct spi_transfer *t, struct spi_message *m) */

	ret=spi_sync(spi,&m);   /* 同步传输 */
	/* int spi_sync(struct spi_device *spi, struct spi_message *message) */
	if(ret){
		/* 传输失败 */
		goto out2;
	}
	memcpy(val,rxdata+1,len);    /* 发送寄存器地址时受到的数据不需要 */
out2:
	kfree(rxdata);    /* 释放rxdata内存 */
out1:
	kfree(t);   	/* 释放spi_transfer内存 */
	return ret;
}

/*
 * @description	: 向icm20608多个寄存器写入数据
 * @param - dev:  icm20608设备
 * @param - reg:  要写入的寄存器首地址
 * @param - val:  要写入的数据缓冲区
 * @param - len:  要写入的数据长度
 * @return 	  :   操作结果
 */

static signed int icm20608_write_regs(struct icm20608_dev *dev, u8 reg, u8 *val,u8 len)
{
	int ret=-1;   /* ??? */
	unsigned char *txdata;
	struct spi_message m;     /* 如果定义结构体指针会报错 */
	struct spi_transfer *t;
	struct spi_device *spi=(struct spi_device *)dev->private_data;

	/* 申请内存 ??? */
	t=kzalloc(sizeof(struct spi_transfer),GFP_KERNEL);    /* GFP_KERNEL——正常分配内存 */
	if(!t){
		return -ENOMEM;   /* 返回Out of memory  */
	}

	txdata=kzalloc(sizeof(char) * (len+1),GFP_KERNEL); 
	if(!txdata){
		goto out1;
	}

	/* 一共发送len+1个字节的数据，第一个字节为寄存器首地址，一共要读取len个字节长度的数据 */
	*txdata=reg & (~0x80);  		/* 写数据的时候首寄存器地址bit8要清零 */
	memcpy(txdata+1,val,len); /* 把len个寄存器拷贝到txdata里，等待发送 */
	t->tx_buf=txdata;  /* 要发送的数据 */
	t->len=len+1;      /* t->len=发送的长度+读取的长度 */

	spi_message_init(&m);   /* spi_message之前需要对其进行初始化 */
	/* void spi_message_init(struct spi_message *m) */
	spi_message_add_tail(t,&m);  /*  spi_transfer 添加到 spi_message 队列中 */
	/* void spi_message_add_tail(struct spi_transfer *t, struct spi_message *m) */

	ret=spi_sync(spi,&m);   /* 同步传输 */
	/* int spi_sync(struct spi_device *spi, struct spi_message *message) */
	if(ret){
		/* 传输失败 */
		goto out2;
	}
	
out2:
	kfree(txdata);    /* 释放rxdata内存 */

out1:
	kfree(t);   	/* 释放spi_transfer内存 */
	return ret;	
}

/*
 * @description	: 读取icm20608指定寄存器值，读取一个寄存器
 * @param - dev:  icm20608设备
 * @param - reg:  要读取的寄存器
 * @return 	  :   读取到的寄存器值
 */
static u8 icm20608_read_reg(struct icm20608_dev *dev,u8 reg)
{
	u8 buf=0;
	icm20608_read_regs(dev,reg,&buf,1);

	return buf;
}

/*
 * @description	: 向icm20608指定寄存器写入指定的值，写一个寄存器
 * @param - dev:  icm20608设备
 * @param - reg:  要写的寄存器
 * @param - data: 要写入的值
 * @return   :    无
 */
static void icm20608_write_reg(struct icm20608_dev *dev, u8 reg, u8 data)
{
	unsigned char buf=0;
	buf=data;
	icm20608_write_regs(dev,reg,&buf,1);    /* 写入一个寄存器值 */
}


/*
 * @description	: 读取ICM20608的数据，读取原始数据，包括ALS,PS和IR, 注意！
 *				: 如果同时打开ALS,IR+PS的话两次数据读取的时间间隔要大于112.5ms
 * @param - ir	: ir数据
 * @param - ps 	: ps数据
 * @param - ps 	: als数据 
 * @return 		: 无。
 */
void icm20608_readdata(struct icm20608_dev *dev)
{	
	unsigned char i=0;
	unsigned char readdata[14]={0};   /* 数组静态初始化为0 */
	
	icm20608_read_regs(dev,ICM20_ACCEL_XOUT_H,readdata,14);
	for(i=0;i<7;i++){
		dev->readdata[i]=(s16)((readdata[2*i]<<8)|readdata[2*i+1]);
	}
}

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int icm20608_open(struct inode *inode, struct file *filp)
{
	
	filp->private_data=&icm20608;   /* 设置私有数据 */
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
static ssize_t icm20608_read(struct file *filp, char __user *buf, size_t cnt, loff_t *off)  
{	
	signed int data[7]={0};
	int ret=0;
	u8 i=0;
	struct icm20608_dev *dev=(struct icm20608_dev *)filp->private_data;
	icm20608_readdata(dev);

	for(i=0;i<7;i++){
		data[i]=dev->readdata[i];
	}

	ret=copy_to_user(buf,data,sizeof(data));
	if(ret){
		return -EINVAL;   /* 返回错误码 */
		printk("read icm20608 failed!\r\n");
	}
	return ret;
}


/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int icm20608_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static struct file_operations icm20608_fops={
	.owner=THIS_MODULE,
	.open=icm20608_open,
	.read=icm20608_read,
	.release=icm20608_release,
};

/*
 * ICM20608内部寄存器初始化函数 
 * @param  	: 无
 * @return 	: 无
 */
void icm20608_reginit(void)
{
	u8 value=0;

	icm20608_write_reg(&icm20608,ICM20_PWR_MGMT_1,0x80);   /*复位icm20609，复位后芯片默认处于睡眠模式*/
	mdelay(50);
	icm20608_write_reg(&icm20608,ICM20_PWR_MGMT_1,0x01);   /*关闭icm20609睡眠模式且自动选择时钟*/
	mdelay(50);

	value=icm20608_read_reg(&icm20608,ICM20_WHO_AM_I);
	printk("ICM20608 ID = %#X\r\n", value);

	icm20608_write_reg(&icm20608,ICM20_SMPLRT_DIV,0x00);   /*设置输出速率，为不分频，为采样率1 kHz*/
	icm20608_write_reg(&icm20608,ICM20_CONFIG,0x05);       /*陀螺仪低通滤波，3-dB BW为10 Hz，BW为频带带宽（终止频率-起始频率）*/
	icm20608_write_reg(&icm20608,ICM20_GYRO_CONFIG,0x00);   /*陀螺仪量程为±250dps*/
	icm20608_write_reg(&icm20608,ICM20_ACCEL_CONFIG,0x00);   /*加速度计量程设置±2g*/
	icm20608_write_reg(&icm20608,ICM20_ACCEL_CONFIG2,0x05);   /*加速度计低通滤波设置，3-dB BW为10.2 Hz，BW为频带带宽（终止频率-起始频率）*/
	icm20608_write_reg(&icm20608,ICM20_LP_MODE_CFG,0x00);     /*低功耗模式*/
	icm20608_write_reg(&icm20608,ICM20_FIFO_EN,0x00);       /*关闭FIFO   */ 
	icm20608_write_reg(&icm20608,ICM20_PWR_MGMT_2,0x00);      /*打开加速度计和加速度所有轴 */

}

  /*
  * @description     : spi驱动的probe函数，当驱动与
  *                    设备匹配以后此函数就会执行
  * @param - client  : spi设备
  * 
  */
static int icm20608_probe(struct spi_device *spi)
{
	printk("led driver and device has matched!\r\n");
	/* 注册字符设备驱动 */
	/* 1、创建设备号 */
	if(icm20608.major){
		icm20608.devid=MKDEV(icm20608.devid,0);  /* 由高12位的主设备号和低20位的次设备号组成完全设备号 */
		register_chrdev_region(icm20608.devid,ICM20608_CNT,ICM20608_NAME);
		/* 函数原型为int register_chrdev_region(dev_t from, unsigned count, const char *name) */
		/*要申请的起始设备号，也就是给定的设备号；申请的数量，一般都是一个；设备名字 */
	}else{
		alloc_chrdev_region(&icm20608.devid,0,ICM20608_CNT,ICM20608_NAME);
		/*函数原型为int alloc_chrdev_region(dev_t *dev,unsigned baseminor,unsigned count,const char *name)*/
		icm20608.major=MAJOR(icm20608.devid);
		icm20608.minor=MINOR(icm20608.devid);
	}
	printk("icm20608 major=%d,minor=%d\r\n",icm20608.major,icm20608.minor);

	/* 2、初始化cdev */
	icm20608.cdev.owner=THIS_MODULE;
	cdev_init(&icm20608.cdev,&icm20608_fops);
	/* 函数原型为void cdev_init(struct cdev *cdev, const struct file_operations *fops) */

	/* 3、添加一个cdev*/
	cdev_add(&icm20608.cdev,icm20608.devid,ICM20608_CNT);
	/* 函数原型int cdev_add(struct cdev *p, dev_t dev, unsigned count) */

	/* 4、创建类 */
	icm20608.class=class_create(THIS_MODULE,ICM20608_NAME);
	/* 函数原型为struct class *class_create (struct module *owner, const char *name) */
	/* IS_ERR()将传入的值与(unsigned long)-MAX_ERRNO比较，MAX_ERRNO为4095，其含义就是最大错误号，-4095补码为0xfffff001，
		即大于等于0xfffff001的指针为非法指针，内核一页4k(4095)记录内核空间的错误指针 */
	if(IS_ERR(icm20608.class)){   /*  判断是否为指针错误，IS_ERR有效指针、空指针返回false，错误指针返回true  */
		return PTR_ERR(icm20608.class);  /* PTR_ERR()将传入的void *类型指针强转为long类型，从而返回出错误类型 */
	}

	/* 5、创建设备 */
	icm20608.device=device_create(icm20608.class,NULL,icm20608.devid,NULL,ICM20608_NAME);
	/* 函数原型为struct device *device_create(struct class *cls, struct device *parent,dev_t devt, void *drvdata,const char *fmt, ...); 
	参数class就是设备要创建哪个类下面；参数parent是父设备，一般为NULL;参数devt是设备号；参数drvdata是设备可能会使用的一些数据，一般为NULL；
	参数fmt是设备名字，如果设置fmt=xxx的话，就会生成/dev/xxx这个设备文件 */

	if(IS_ERR(icm20608.device)){   /*  判断是否为指针错误，IS_ERR有效指针、空指针返回false，错误指针返回true  */
		return PTR_ERR(icm20608.device);  /* PTR_ERR()将传入的void *类型指针强转为long类型，从而返回出错误类型 */
	}

	/*初始化spi_device */
	spi->mode=SPI_MODE_0;
	spi_setup(spi);    /* ??? */

	icm20608.private_data=spi;   /* 设置私有数据	 */

	icm20608_reginit();

	return 0;
}

/*
 * @description     : spi驱动的remove函数，移除spi驱动的时候此函数会执行
 * @param - client 	: spi设备
 * @return          : 0，成功;其他负值,失败
 */
static int icm20608_remove(struct spi_device *spi)
{
	/* 删除cdev字符设备，采用cdev来描述字符设备 */
	cdev_del(&icm20608.cdev);

	/* 注销设备号 */
	unregister_chrdev_region(icm20608.devid,ICM20608_CNT);

	/* 摧毁设备，注意先后顺序 */
	device_destroy(icm20608.class,icm20608.devid);  /* void device_destroy(struct class *cls, dev_t devt); */

	/* 销毁类 */
	class_destroy(icm20608.class);
	
	printk("led_exit\r\n");
	return 0;
}

static const struct spi_device_id icm20608_id[]={
	{"myspi,icm20608", 0},
	{/* Sentinel */}
};

static const struct of_device_id icm20608_of_match[]={   /* 需要包括最后一个空元素 */
	{.compatible="myspi,icm20608"},
	{/* Sentinel */}
};

/* platform驱动结构体 */
static struct spi_driver icm20608_driver={
	.probe=icm20608_probe,
	.remove=icm20608_remove,
	.driver={
		.owner=THIS_MODULE,
		.name="icm20608",       /* 驱动名字，用于和设备匹配 */
		.of_match_table=icm20608_of_match,
		},
	.id_table=icm20608_id,
};

/*
 * @description	: 驱动模块加载函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init ap3116c_init(void)
{
	int ret=0;
	ret=spi_register_driver(&icm20608_driver);
	return ret;
}

/*
 * @description	: 驱动模块卸载函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit ap3116c_exit(void)
{
	spi_unregister_driver(&icm20608_driver);
}

/* 注册驱动加载和卸载 */
module_init(ap3116c_init);
module_exit(ap3116c_exit);

/* LICENSE和作者信息 */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("CVVO");