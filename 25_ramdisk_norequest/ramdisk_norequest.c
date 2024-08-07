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
int ramdisk_open (struct block_device *dev, fmode_t mode)
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
void ramdisk_release (struct gendisk *gendisk, fmode_t mode)
{	
	printk("ramdisk release\r\n");
}

/*
 * @description		: 获取磁盘信息   
 * @param - dev 	: 块设备
 * @param - geo 	: 模式
 * @return 			: 0 成功;其他 失败
 */
int ramdisk_getgeo (struct block_device *dev, struct hd_geometry *geo)
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
 * @description	: “制造请求”函数--抛开 I/O 调度器
 * @param-q 	: 请求队列
 * @return 		: 无
 */
void ramdisk_make_request_fn (struct request_queue *q, struct bio *bio)
{
	int offset;
	struct bio_vec bvl;    /* RAM信息，比如页地址、页偏移以及长度，实际地址为页地址+页偏移 */
	struct bvec_iter iter;  /* 变量就用于描述物理存储设备地址信息，比如要操作的扇区地址 */
	unsigned long len = 0;

	offset=(bio->bi_iter.bi_sector)<<9;    /* 由扇区偏移地址左移9位转换为字节地址(2^9=512字节) */

	bio_for_each_segment(bvl,bio,iter){  /* 遍历 bio 中的所有段，宏定义包括for循环*/
		char *ptr = page_address(bvl.bv_page) + bvl.bv_offset;    /* 根据页地址以及偏移地址转换为真正的数据起始地址 */
		len = bvl.bv_len;   /* 数据长度 */

		if(bio_data_dir(bio) == READ)	/* 读数据 */
			memcpy(ptr, ramdisk.ramdiskbuf + offset, len);  /* 目的、原数据、长度。ramdisk.ramdiskbuf为实际块设备地址 */
		else if(bio_data_dir(bio) == WRITE)	/* 写数据 */
			memcpy(ramdisk.ramdiskbuf + offset, ptr, len);
		offset += len;   /* ramdisk.ramdiskbuf为实际块设备地址加上偏移长度 */
	}   
	/* #define bio_for_each_segment(bvl, bio, iter)  __bio_for_each_segment(bvl, bio, iter, (bio)->bi_iter) */

	set_bit(BIO_UPTODATE, &bio->bi_flags);  /* 参考 */
	bio_endio(bio,0);  /* 通知 bio 处理结束 */
	/* bvoid bio_endio(struct bio *bio, int error) */
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

	/* 3、申请gendisk */
	ramdisk.gendisk=alloc_disk(RADMISK_MINOR);
	/* struct gendisk *alloc_disk(int minors),次设备号数量，也就是gendisk对应的分区数量 */
	if(!ramdisk.gendisk){
		ret=-EINVAL;
		goto gendisk_alloc_fail;
	}

	/* 4、初始化自旋锁 */
	spin_lock_init(&ramdisk.lock);

	/* 5、申请请求队列 */
	ramdisk.queue=blk_alloc_queue(GFP_KERNEL);
	/* struct request_queue *blk_alloc_queue(gfp_t gfp_mask)，内存分配掩码，一般为GFP_KERNEL */
	if(!ramdisk.queue){
		ret=-EINVAL;
		goto blk_init_fail;
	}
	
	/* 6、申请到的请求队列绑定一个“制造请求”函数 */
	blk_queue_make_request(ramdisk.queue,ramdisk_make_request_fn);
	/* void blk_queue_make_request(struct request_queue *q, make_request_fn *mfn) */

	/* 6、初始化gendisk 参考(drivers/block/zram/zram_drv.c)*/
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