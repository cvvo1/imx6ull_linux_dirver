#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>

#define CHRDEVBASE_MAJOR 	200   		 // 主设备号
#define CHRDEVBASE_NAME  "chrdevbase"   // 名字

static char readbuf[100];   /* 读缓冲区 */
static char writebuf[100];  /* 写缓冲区 */
static char kerneldata[]={"kernel data!"};

/* 字符设备驱动
  * 主要就是驱动对应的open、close、read等，即file_operations结构体的成员变量实现
  * */

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */

/* static+函数就变为静态函数，其作用域由之前的整个项目内的文件都可访问变为了只能在本文件内被访问 */
static int chrdevbase_open(struct inode *inode, struct file *filp)     
{
	// printk("chrdevbase_open\r\n");
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
static ssize_t chrdevbase_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
	int ret=0;
	/* 向用户空间发送数据 */
	memcpy(readbuf,kerneldata,sizeof(kerneldata));   /* 内存拷贝 */
	ret=copy_to_user(buf,readbuf,cnt);  /* 应用程序不能直接访问内核数据，驱动给应用传递数据需要用到copy_to_user，
				函数原型为：static inline long copy_to_user(void __user *to, const void *from, unsigned long n)
				to表示目的，from表示源，n表示要复制的数据长度。复制成功，返回值为0，如果复制失败则返回负数 */
	if(ret == 0){
		printk("kernel senddata ok!\r\n");
	}else{
		printk("kernel senddata failed!\r\n");
	}

	// printk("chrdevbase_read\r\n");
	return 0;
	
}

/*
 * @description		: 向设备写数据 
 * @param - filp 	: 设备文件，表示打开的文件描述符
 * @param - buf 	: 要写给设备写入的数据
 * @param - cnt 	: 要写入的数据长度，字节
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 			: 写入的字节数，如果为负值，表示写入失败
 */
static ssize_t chrdevbase_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	int ret=0;
	/* 接收用户空间传递给内核的数据并且打印出来  */
	ret=copy_from_user(writebuf,buf,cnt);  /* 将用户空间的数据复制到writebuf这个内核空间中 */

	if(ret == 0){
		printk("kernel recevdata:%s\r\n", writebuf);
	}else{
		printk("kernel recevdata failed!\r\n");
	}

	// printk("chrdevbase_write\r\n");
	return 0;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int chrdevbase_release(struct inode *inode, struct file *filp)     
{
	// printk("chrdevbase_close\r\n");
	return 0;
}



/* 设备操作函数结构体(操作函数集合) */
static struct file_operations chrdevbase_fops={
	.owner = THIS_MODULE,
	.open =  chrdevbase_open,
	.read =chrdevbase_read,
	.write=chrdevbase_write,
	.release = chrdevbase_release,
};

/*
 * @description	: 驱动入口函数 
 * @param 		: 无
 * @return 		: 0 成功;其他 失败
*/
static int __init chrdevbase_init(void)
{
	int ret=0;
	 
	/* 注册字符设备，linux内核启动 */
	ret=register_chrdev(CHRDEVBASE_MAJOR,CHRDEVBASE_NAME,&chrdevbase_fops);
	MKDEV

	// 包括major、minor和name，其中设备号由主设备号和此设备号两部分组成，由dev_t数据类型确定（unsigned int）
	// 32位组成，其中高12位(0~4095)为主设备号(MAJOR(dev_t))，低20位次设备号(MINOR(dev_t)) 

	if(ret < 0){
		printk("chrdevbase driver register failed\r\n");
	}
	printk("chrdevbase init!\r\n");  /* printf运行在用户态，printk运行在内核态（向控制台输出或显示一些内容），默认等级为4，KERN_WARNING */
	return 0;
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
*/
static void __exit chrdevbase_exit(void)
{
	/* 注销字符设备 */
	unregister_chrdev(CHRDEVBASE_MAJOR,CHRDEVBASE_NAME);

	printk("chrdevbase_exit\r\n");
}


/* 驱动模块的加载和卸载 */
module_init(chrdevbase_init);   /* 注册模块加载函数 */

module_exit(chrdevbase_exit);   /* 注册模块卸载函数 */

MODULE_LICENSE("GPL");   /* 需要在驱动中加入LICENSE信息，必须添加否则会报错 */
MODULE_AUTHOR("CVVO");   /* 添加模块作者信息 */