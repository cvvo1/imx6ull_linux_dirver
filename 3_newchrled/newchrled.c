#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/device.h>

#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define NEWCHRLED_CNT    1     /* 设备号个数 */
#define NEWCHRLED_NAME  "newchrled"   /* 设备名字 */

#define LEDOFF 	0				/* 关灯，定义为字符串 */
#define LEDON 	1				/* 开灯 */

/* LED设备结构体 */
struct newchrled_dev{
	dev_t devid;   /* 设备号，由dev_t数据类型为（unsigned int） */
	struct cdev cdev;     /* cdev结构体表示字符设备 */
	struct class *class;
	struct device *device;	/* 设备 */
	int major;   /* 主设备号 */
	int minor;   /* 次设备号 */
};

struct newchrled_dev newchrled;

/* 寄存器物理地址 */
#define CCM_CCGR1_BASE            (0X020C406C)
#define SW_MUX_GPIO1_IO03_BASE    (0X020E0068)
#define SW_PAD_GPIO1_IO03_BASE    (0X020E02F4)
#define GPIO1_GDIR_BASE           (0X0209C004)
#define GPIO1_DR_BASE             (0X0209C000)

/* 映射后的寄存器虚拟地址指针 */
static void __iomem *CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;

/*
 * @description		: LED打开/关闭
 * @param - sta 	: LEDON(0) 打开LED，LEDOFF(1) 关闭LED
 * @return 			: 无
 */
void led_switch(uint8_t sta)
{
	uint32_t val;
	if (sta==LEDON){
		val=readl(GPIO1_DR);
		val &=~(1<<3);
		writel(val,GPIO1_DR);    /* 打开led */
	}else if(sta==LEDOFF){
		val=readl(GPIO1_DR);
		val &=~(1<<3);
		val |=(1<<3);
		writel(val,GPIO1_DR);    /* 关闭led */
	}

}

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int led_open(struct inode *inode, struct file *filp)
{

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
static ssize_t led_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
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
	int ret;
	uint8_t databuf[1];
	uint8_t ledstate;

	ret=copy_from_user(databuf,buf,cnt);
	if (ret<0){
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}

	/* 判断是开灯还是关灯 */
	ledstate=databuf[0];
	if(ledstate==LEDON){
		led_switch(LEDON);
	}else if(ledstate==LEDOFF){
		led_switch(LEDOFF);
	}

	return 0;
}

static int led_release(struct inode *inode, struct file *filp)
{
	return 0;
}


static struct file_operations newchrled_fop={
	.owner=THIS_MODULE,
	.open=led_open,
	.read=led_read,
	.write=led_write,
	.release=led_release,
};

/* 驱动入口 */
static int __init led_init(void)
{
	int ret=0;
	uint32_t val=0;
	
	/* 初始化LED */
	/* 1、寄存器地址映射 */
	CCM_CCGR1=ioremap(CCM_CCGR1_BASE,4);
	SW_MUX_GPIO1_IO03=ioremap(SW_MUX_GPIO1_IO03_BASE,4);
	SW_PAD_GPIO1_IO03=ioremap(SW_PAD_GPIO1_IO03_BASE,4);
	GPIO1_GDIR=ioremap(GPIO1_GDIR_BASE,4);
	GPIO1_DR=ioremap(GPIO1_DR_BASE,4);

	/* 2、使能时钟 */
	val =readl(CCM_CCGR1);
	val &=~(0x3<<26);    /* 先清零 */
	val |=(0x3<<26);    /* 再置1 */
	writel(val,CCM_CCGR1);

	/* 3、初始化led */
	writel(0x5,SW_MUX_GPIO1_IO03);   /*复用为GPIO1_IO03*/

	/*寄存器SW_PAD_GPIO1_IO03设置IO属性
	 *bit 16:0 HYS关闭
	 *bit [15:14]: 00 默认下拉
     *bit [13]: 0 kepper功能
     *bit [12]: 1 pull/keeper使能
     *bit [11]: 0 关闭开路输出
     *bit [7:6]: 10 速度100Mhz
     *bit [5:3]: 110 R0/6驱动能力
     *bit [0]: 0 低转换率
	 */
	writel(0x10b0,SW_PAD_GPIO1_IO03);   /* 设置电气属性 */

	/* 4、GPIO初始化 */
	val=readl(GPIO1_GDIR);
	val &=~(1<<3);
	val |=(1<<3);
	writel(val,GPIO1_GDIR);  /*设置方向寄存器为输出模式*/

	val=readl(GPIO1_DR);
	val|=(1<<3);      /* 置1 */
	writel(val,GPIO1_DR);    /* 默认关闭led */


	/* 注册字符设备驱动 */
	/* 1、创建设备号 */
	if(newchrled.major){    /* 给定主设备号 */
		newchrled.devid=MKDEV(newchrled.major,0);   /* 由高12位的主设备号和低20位的次设备号组成完全设备号 */
		ret=register_chrdev_region(newchrled.devid,NEWCHRLED_CNT,NEWCHRLED_NAME);  
		/* 函数原型为int register_chrdev_region(dev_t from, unsigned count, const char *name) */
		/*要申请的起始设备号，也就是给定的设备号；申请的数量，一般都是一个；设备名字 */
	}else{   /* 没有给定主设备号 */
		ret=alloc_chrdev_region(&newchrled.devid,0,NEWCHRLED_CNT,NEWCHRLED_NAME); 
		/*函数原型为int alloc_chrdev_region(dev_t *dev,unsigned baseminor,unsigned count,const char *name)*/
		newchrled.major=MAJOR(newchrled.devid);   
		newchrled.minor=MINOR(newchrled.devid);
	}
	if(ret<0){
		printk("newchrled register_chrdev_region err!\r\n");
		return -1;
	}
	printk("newcheled major=%d,minor=%d\r\n",newchrled.major,newchrled.minor);

	/* 2、初始化CDEV */
	newchrled.cdev.owner= THIS_MODULE;
	cdev_init(&newchrled.cdev,&newchrled_fop);   
	/* 函数原型为void cdev_init(struct cdev *cdev, const struct file_operations *fops) */

	/* 3、添加一个cdev */
	ret=cdev_add(&newchrled.cdev,newchrled.devid,NEWCHRLED_CNT);
	/* 函数原型int cdev_add(struct cdev *p, dev_t dev, unsigned count) */

	/* 4、创建类 */
	newchrled.class=class_create(newchrled_fop.owner,NEWCHRLED_NAME);
	/* 函数原型为struct class *class_create (struct module *owner, const char *name) */
	
	/* IS_ERR()将传入的值与(unsigned long)-MAX_ERRNO比较，MAX_ERRNO为4095，其含义就是最大错误号，-4095补码为0xfffff001，
		即大于等于0xfffff001的指针为非法指针，内核一页4k(4095)记录内核空间的错误指针 */
	if (IS_ERR(newchrled.class)) {    /* 判断是否为指针错误，IS_ERR有效指针、空指针返回false，错误指针返回true */
		return PTR_ERR(newchrled.class);  /* PTR_ERR()将传入的void *类型指针强转为long类型，从而返回出错误类型 */
	}

	/* 5、创建设备 */
	newchrled.device=device_create(newchrled.class,NULL,newchrled.devid,NULL,NEWCHRLED_NAME);
	/* 函数原型为struct device *device_create(struct class *cls, struct device *parent,dev_t devt, void *drvdata,const char *fmt, ...); 
	参数class就是设备要创建哪个类下面；参数parent是父设备，一般为NULL;参数devt是设备号；参数drvdata是设备可能会使用的一些数据，一般为NULL；
	参数fmt是设备名字，如果设置fmt=xxx的话，就会生成/dev/xxx这个设备文件 */

	if (IS_ERR(newchrled.device)) {    
		return PTR_ERR(newchrled.device); 
	}
	
	return 0;
}

/* 驱动出口 */
static void __exit led_exit(void)
{
	/* 取消映射 */
	iounmap(CCM_CCGR1);
	iounmap(SW_MUX_GPIO1_IO03);
	iounmap(SW_PAD_GPIO1_IO03);
	iounmap(GPIO1_GDIR);
	iounmap(GPIO1_DR);

	/* 删除cdev字符设备 */
	cdev_del(&newchrled.cdev);

	/* 注销设备号 */
	unregister_chrdev_region(newchrled.devid,NEWCHRLED_CNT);

	/* 摧毁设备，注意先后顺序 */
	device_destroy(newchrled.class,newchrled.devid);  /* void device_destroy(struct class *cls, dev_t devt); */

	/* 销毁类 */
	class_destroy(newchrled.class);

	printk("led_exit\r\n");
}

/* 注册驱动加载和卸载 */
module_init(led_init);
module_exit(led_exit);

/* LICENSE和作者信息 */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("CVVO");