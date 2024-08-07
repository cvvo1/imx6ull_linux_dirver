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
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>

#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define RAMDISK_NAME 	"ramdisk"    /* 名字 */
#define RADMISK_MINOR	3					/* 表示有三个磁盘分区！不是次设备号为3！ */
#define RAMDISK_SIZE	(2 * 1024 * 1024) 	/* 容量大小为2MB */

/* ramdisk设备结构体 */
struct ramdisk_dev{
	int major;      /* 主设备号 */
	struct gendisk *gendisk;  /* 请求队列 */
	struct request_queue *queue;   /* gendisk */
	spinlock_t lock;  /* 自旋锁 */
	unsigned char *ramdiskbuf;   /* ramdisk内存空间,用于模拟块设备 */     
};

struct ramdisk_dev ramdisk;

/*
 * @description		: 打开块设备
 * @param - dev 	: 块设备
 * @param - mode 	: 打开模式
 * @return 			: 0 成功;其他 失败
 */
static int ramdisk_open (struct block_device *dev, fmode_t mode)
{
	printk("ramdisk open\r\n");
	return 0;
}

/*
 * @description		: 释放块设备
 * @param - disk 	: gendisk
 * @param - mode 	: 模式
 * @return 			: 0 成功;其他 失败
 */
static void ramdisk_release (struct gendisk *gendisk, fmode_t mode)
{	
	printk("ramdisk release\r\n");
}

/*
 * @description		: 获取磁盘信息    存储容量=磁头x柱面x扇区x每扇区字节数(512)
 * @param - dev 	: 块设备
 * @param - geo 	: 模式
 * @return 			: 0 成功;其他 失败
 */
static int ramdisk_getgeo (struct block_device *dev, struct hd_geometry *geo)
{
	/* 这是相对于机械硬盘的概念 */
	geo->heads = 2;			/* 磁头 */
	geo->cylinders = 32;	/* 柱面 */
	geo->sectors = RAMDISK_SIZE / (2 * 32 *512); /* 一个磁道上的扇区数量 */
	return 0;
}

/* 
 * 块设备操作函数 
 */
static struct block_device_operations ramdisk_fops=
{
	.owner=THIS_MODULE,
	.open=ramdisk_open,
	.release=ramdisk_release,
	.getgeo=ramdisk_getgeo,
};

/*
 * @description	: 处理传输过程
 * @param-req 	: 请求
 * @return 		: 无
 */
static void ramdisk_transfer(struct request *req)
{	
	unsigned long start = blk_rq_pos(req) << 9;  	/* blk_rq_pos获取到的是扇区地址偏移，左移9位(需要将扇区地址转换为原始地址-512字节=2^9)*/
	unsigned long len  = blk_rq_cur_bytes(req);		/* 获取请求要操作的数据长度   */

	/* bio中的数据缓冲区
	 * 读：从磁盘读取到的数据存放到buffer中
	 * 写：buffer保存这要写入磁盘的数据
	 */
	void *buffer = bio_data(req->bio);	/* bio_data函数获取请求中bio缓存区(ram)保存的数据 */
	
	/* 调用 rq_data_dir 函数判断当前是读还是写 */
	if(rq_data_dir(req) == READ) 		/* 读数据 */	
		memcpy(buffer, ramdisk.ramdiskbuf + start, len);   /* 内存模拟采用memcpy */
	else if(rq_data_dir(req) == WRITE) 	/* 写数据 */
		memcpy(ramdisk.ramdiskbuf + start, buffer, len);

}


/*
 * @description	: 请求处理函数，完成从块设备中读取数据，或者向块设备中写入数据
 * @param-q 	: 请求队列
 * @return 		: 无
 */
void ramdisk_request_fn(struct request_queue *q)
{
	int err=0;
	struct request *req;
	req=blk_fetch_request(q);  /* 包含I/O调度算法 */
	/* struct request *blk_fetch_request(struct request_queue *q){}-一次性完成请求的获取和开启,获取到请求队列中第一个请求 */
	while(req != NULL) {     /* 依次处理完请求队列中的每个请求 */
		/* 针对请求做具体的传输处理 */
		ramdisk_transfer(req);   /* 请求不为空的话就调用 ramdisk_transfer 函数进行对请求做进一步的处理 */

		/* 检查是否为最后一个请求，如果不是的话就继续获取下一个，直至整个请求队列处理完成 */
		if (!__blk_end_request_cur(req, err))  /* 当前请求中的chunk，且需要持有队列锁，参考*/
			req = blk_fetch_request(q);    
	}
}

/*
 * @description	: 驱动模块加载函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init ramdisk_init(void)
{
	int ret=0;
	printk("ramdisk init\r\n");

	/* 1、申请用于ramdisk内存 */
	ramdisk.ramdiskbuf=kzalloc(RAMDISK_SIZE,GFP_KERNEL);    /* 正常分配内存 */
	if(ramdisk.ramdiskbuf==NULL){
		ret = -EINVAL;
		goto ram_fail;
	}

	/* 2、注册块设备 */
	ramdisk.major=register_blkdev(0,RAMDISK_NAME);
	/* int register_blkdev(unsigned int major, const char *name),由系统自动分配主设备号，那么返回值就是系统分配的主设备号(1~255)，如果返回负值那就表示注册失败 */
	if(ramdisk.major<0){
		goto register_blkdev_fail;
	}
	printk("ramdisk major = %d\r\n", ramdisk.major);

	/* 3、申请gendisk */
	ramdisk.gendisk=alloc_disk(RADMISK_MINOR);
	/* struct gendisk *alloc_disk(int minors),次设备号数量，也就是gendisk对应的分区数量 */
	if(!ramdisk.gendisk){
		ret=-EINVAL;
		goto gendisk_alloc_fail;
	}

	/* 4、初始化自旋锁 */
	spin_lock_init(&ramdisk.lock);

	/* 5、初始化请求队列 */
	ramdisk.queue=blk_init_queue(ramdisk_request_fn,&ramdisk.lock);
	/* request_queue *blk_init_queue(request_fn_proc *rfn, spinlock_t *lock) 请求处理函数指针 */
	if(!ramdisk.queue){
		ret=-EINVAL;
		goto blk_init_fail;
	}
	
	/* 6、初始化gendisk 参考z2ram.c(drivers/block/z2ram.c)*/
	ramdisk.gendisk->major=ramdisk.major;  /* 主设备号 */
	ramdisk.gendisk->first_minor=0;    /* 第一个次设备号(起始次设备号) */
	ramdisk.gendisk->fops=&ramdisk_fops;   /* 操作函数 */
	ramdisk.gendisk->private_data=&ramdisk;   /* 私有数据 */
	ramdisk.gendisk->queue=ramdisk.queue;  /* 请求队列 */

	//ramdisk.gendisk->disk_name=RAMDISK_NAME;      /* 名字 */
	sprintf(ramdisk.gendisk->disk_name, RAMDISK_NAME);   /* 名字，给字符数组类型赋值 */

	set_capacity(ramdisk.gendisk,RAMDISK_SIZE/512);  /* 设备容量(单位为扇区) */
	/* void set_capacity(struct gendisk *disk, sector_t size)-设置 gendisk 容量 ,扇区数量(1个扇区512字节)*/

	/* 7、将gendisk添加内核 */
	add_disk(ramdisk.gendisk);
	/* void add_disk(struct gendisk *disk)-将 gendisk 添加到内核 */

	return 0;

blk_init_fail:
	put_disk(ramdisk.gendisk);   /* put_disk 是减少 gendisk 的引用计数 */
	//del_gendisk(ramdisk.gendisk);
gendisk_alloc_fail:
	unregister_blkdev(ramdisk.major,RAMDISK_NAME);
register_blkdev_fail:
	kfree(ramdisk.ramdiskbuf);  /* 释放内存 */
ram_fail:
	return ret;
}

/*
 * @description	: 驱动模块卸载函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit ramdisk_exit(void)
{
	printk("ramdisk exit!\r\n");

	/* 删除gendisk */
	del_gendisk(ramdisk.gendisk);
	put_disk(ramdisk.gendisk);   /* 参考 */


	/* 删除请求队列 */
	blk_cleanup_queue(ramdisk.queue);

	/* 注销块设备 */
	unregister_blkdev(ramdisk.major,RAMDISK_NAME);

	/* 释放内存 */
	kfree(ramdisk.ramdiskbuf);
}

/* 注册驱动加载和卸载 */
module_init(ramdisk_init);
module_exit(ramdisk_exit);

/* LICENSE和作者信息 */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("CVVO");