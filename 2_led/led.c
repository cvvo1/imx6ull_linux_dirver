#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>

#define LED_MAJOR  200
#define LED_NAME  "led"

#define LEDOFF 	0				/* 关灯，定义为字符串 */
#define LEDON 	1				/* 开灯 */

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


static struct file_operations led_fop={
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


	/* 5、注册字符设备 */
	ret=register_chrdev(LED_MAJOR,LED_NAME,&led_fop);
	if (ret<0){
		printk("led register failed!\r\n");
		return -EIO;
	}
	printk("led_init\r\n");
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

	unregister_chrdev(LED_MAJOR,LED_NAME);
	printk("led_exit\r\n");
}

/* 注册驱动加载和卸载 */
module_init(led_init);
module_exit(led_exit);

/* LICENSE和作者信息 */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("CVVO");