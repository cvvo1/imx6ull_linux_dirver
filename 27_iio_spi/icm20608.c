#include <linux/spi/spi.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include "icm20608reg.h"
#include <linux/gpio.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/regmap.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/unaligned/be_byteshift.h>

/***************************************************************
Copyright © ALIENTEK Co., Ltd. 1998-2029. All rights reserved.
文件名		: icm20608.c
作者	  	: 左忠凯
版本	   	: V1.0
描述	   	: ICM20608 SPI驱动程序
其他	   	: 无
论坛 	   	: www.openedv.com
日志	   	: 初版V1.0 2019/9/2 左忠凯创建
***************************************************************/
#define ICM20608_NAME	"icm20608"
#define ICM20608_TEMP_OFFSET	     0
#define ICM20608_TEMP_SCALE		     326800000

#define ICM20608_CHAN(_type,_channel12,_index)    \
	{                         \
		.type=_type,          \
		.modified=1,           \
		.channel2=_channel12,       /*通道修饰符，描述方向  */      \
		.scan_index=_index, 		\
		.scan_type={                      \
			.sign='s',                  \
			.realbits=16,                   \
			.storagebits=16,                \
			.shift=0,                      \
			.endianness=IIO_BE,   \
		},                      \
		.info_mask_separate= BIT(IIO_CHAN_INFO_RAW)| BIT(IIO_CHAN_INFO_CALIBBIAS),  \
		.info_mask_shared_by_type=BIT(IIO_CHAN_INFO_SCALE),                \
	}                                                       \

/* 
 * ICM20608的扫描元素，3轴加速度计、
 * 3轴陀螺仪、1路温度传感器，1路时间戳 
 */
enum inv_icm20608_scan {
	INV_ICM20608_SCAN_ACCL_X,
	INV_ICM20608_SCAN_ACCL_Y,
	INV_ICM20608_SCAN_ACCL_Z,
	INV_ICM20608_SCAN_TEMP,
	INV_ICM20608_SCAN_GYRO_X,
	INV_ICM20608_SCAN_GYRO_Y,
	INV_ICM20608_SCAN_GYRO_Z,
};

struct icm20608_dev {
	struct spi_device *spi;		/* spi设备 */
	struct regmap *regmap_spi;
	struct regmap_config config_spi;   /* 不能定义成指针类型,存在回调函数 */
	struct mutex lock;    /* 定义互斥体 */
};

/*
 * icm20608陀螺仪分辨率，对应250、500、1000、2000，计算方法：
 * 以正负250度量程为例，500/2^16=0.007629，扩大1000000倍，就是7629
 */
static const int gyro_scale_icm20608[] = {7629, 15258, 30517, 61035};

/* 
 * icm20608加速度计分辨率，对应2、4、8、16 计算方法：
 * 以正负2g量程为例，4/2^16=0.000061035，扩大1000000000倍，就是61035
 */
static const int accel_scale_icm20608[] = {61035, 122070, 244140, 488281};


/*
 * icm20608通道，1路温度通道，3路陀螺仪，3路加速度计
 */
static const struct iio_chan_spec icm20608_channels[]={
	/* 温度通道 */
	{
		.type=IIO_TEMP,
		.scan_index=INV_ICM20608_SCAN_TEMP,
		.scan_type={
			.sign='s',
			.realbits=16,
			.storagebits=16,
			.shift=0,
			.endianness=IIO_BE,   /* 大端存储 */
		},
		.info_mask_separate= BIT(IIO_CHAN_INFO_RAW)| BIT(IIO_CHAN_INFO_OFFSET)| BIT(IIO_CHAN_INFO_SCALE),  /* BIT(a)为1<<a */
	},

	ICM20608_CHAN(IIO_ACCEL,IIO_MOD_X,INV_ICM20608_SCAN_ACCL_X),   /* 加速度X轴 */
	ICM20608_CHAN(IIO_ACCEL,IIO_MOD_Y,INV_ICM20608_SCAN_ACCL_Y),
	ICM20608_CHAN(IIO_ACCEL,IIO_MOD_Z,INV_ICM20608_SCAN_ACCL_Z),

	ICM20608_CHAN(IIO_ANGL_VEL,IIO_MOD_X,INV_ICM20608_SCAN_GYRO_X),   /* 加速度X轴 */
	ICM20608_CHAN(IIO_ANGL_VEL,IIO_MOD_Y,INV_ICM20608_SCAN_GYRO_Y),
	ICM20608_CHAN(IIO_ANGL_VEL,IIO_MOD_Z,INV_ICM20608_SCAN_GYRO_Z),	
};


/*
 * @description	: 向icm20608指定寄存器写入指定的值，写一个寄存器
 * @param - dev:  icm20608设备
 * @param - reg:  要写的寄存器
 * @param - data: 要写入的值
 * @return   :    无
 */
static void icm20608_write_onereg(struct icm20608_dev *dev, u8 reg, u8 data)
{
	regmap_write(dev->regmap_spi,reg,data);
	/*int regmap_write(struct regmap *map, unsigned int reg, unsigned int val)*/
}

/*
 * @description	: 读取icm20608指定寄存器值，读取一个寄存器
 * @param - dev:  icm20608设备
 * @param - reg:  要读取的寄存器
 * @return 	  :   读取到的寄存器值
 */
static u8 icm20608_read_onereg(struct icm20608_dev *dev,u8 reg)
{
	unsigned int data=0;
	int ret=0;
	ret=regmap_read(dev->regmap_spi,reg,&data);
	/* int regmap_read(struct regmap *map, unsigned int reg, unsigned int *val) */
	return data;
}


/*
 * ICM20608内部寄存器初始化函数 
 * @param  	: 无
 * @return 	: 无
 */
void icm20608_reginit(struct icm20608_dev *dev)
{
	u8 value = 0;
	
	icm20608_write_onereg(dev, ICM20_PWR_MGMT_1, 0x80);
	mdelay(50);
	icm20608_write_onereg(dev, ICM20_PWR_MGMT_1, 0x01);
	mdelay(50);

	value = icm20608_read_onereg(dev, ICM20_WHO_AM_I);
	printk("ICM20608 ID = %#X\r\n", value);	

	icm20608_write_onereg(dev, ICM20_SMPLRT_DIV, 0x00); 	/* 输出速率是内部采样率					*/
	icm20608_write_onereg(dev, ICM20_GYRO_CONFIG, 0x18); 	/* 陀螺仪±2000dps量程 				*/
	icm20608_write_onereg(dev, ICM20_ACCEL_CONFIG, 0x18); 	/* 加速度计±16G量程 					*/
	icm20608_write_onereg(dev, ICM20_CONFIG, 0x04); 		/* 陀螺仪低通滤波BW=20Hz 				*/
	icm20608_write_onereg(dev, ICM20_ACCEL_CONFIG2, 0x04); /* 加速度计低通滤波BW=21.2Hz 			*/
	icm20608_write_onereg(dev, ICM20_PWR_MGMT_2, 0x00); 	/* 打开加速度计和陀螺仪所有轴 				*/
	icm20608_write_onereg(dev, ICM20_LP_MODE_CFG, 0x00); 	/* 关闭低功耗 						*/
	icm20608_write_onereg(dev, ICM20_FIFO_EN, 0x00);		/* 关闭FIFO						*/
}

/*
  * @description  	: 设置ICM20608传感器，可以用于陀螺仪、加速度计设置校准值
  * @param - dev	: icm20608设备 
  * @param - reg  	: 要设置的通道寄存器首地址。
  * @param - anix  	: 要设置的通道，比如X，Y，Z。
  * @param - val  	: 要设置的值。
  * @return			: 0，成功；其他值，错误
  */
static int icm20608_sensor_set(struct icm20608_dev *dev, int reg,
				int axis, int val)
{
	int ind, result;
	__be16 d = cpu_to_be16(val);

	ind = (axis - IIO_MOD_X) * 2;
	result = regmap_bulk_write(dev->regmap_spi, reg + ind, (u8 *)&d, 2);
	if (result)
		return -EINVAL;

	return 0;
}

/*
  * @description  	: 读取ICM20608传感器数据，可以用于陀螺仪、加速度计、温度的读取
  * @param - dev	: icm20608设备 
  * @param - reg  	: 要读取的通道寄存器首地址。
  * @param - anix  	: 需要读取的通道，比如X，Y，Z。
  * @param - val  	: 保存读取到的值。
  * @return			: 0，成功；其他值，错误
  */
static int icm20608_sensor_show(struct icm20608_dev *dev, int reg,
				   int axis, int *val)
{
	int ind, result;
	__be16 d;

	ind = (axis - IIO_MOD_X) * 2;
	result = regmap_bulk_read(dev->regmap_spi, reg + ind, (u8 *)&d, 2);  /* 通过地址偏移读取，如果IIO_MOD_Y,则ind=(2-1)*2=2, reg+2*/
	if (result)
		return -EINVAL;
	*val = (short)be16_to_cpup(&d);

	return IIO_VAL_INT;
}

/*
  * @description  		: 读取ICM20608陀螺仪、加速度计、温度通道值
  * @param - indio_dev	: iio设备 
  * @param - chan  		: 通道。
  * @param - val  		: 保存读取到的通道值。
  * @return				: 0，成功；其他值，错误
  */
static int icm20608_read_channel_data(struct iio_dev *indio_dev,
					 struct iio_chan_spec const *chan,
					 int *val)
{
	struct icm20608_dev *dev = iio_priv(indio_dev);
	int ret = 0;

	switch (chan->type) {
	case IIO_ANGL_VEL:	/* 读取陀螺仪数据 */
		ret = icm20608_sensor_show(dev, ICM20_GYRO_XOUT_H, chan->channel2, val);  /* channel2为X、Y、Z轴，细分*/
		break;
	case IIO_ACCEL:		/* 读取加速度计数据 */
		ret = icm20608_sensor_show(dev, ICM20_ACCEL_XOUT_H, chan->channel2, val); /* channel2为X、Y、Z轴 */
		break;
	case IIO_TEMP:		/* 读取温度 */
		ret = icm20608_sensor_show(dev, ICM20_TEMP_OUT_H, IIO_MOD_X, val);  
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

/*
  * @description  	: 设置ICM20608的陀螺仪计量程(分辨率)
  * @param - dev	: icm20608设备
  * @param - val   	: 量程(分辨率值)。
  * @return			: 0，成功；其他值，错误
  */
static int icm20608_write_gyro_scale(struct icm20608_dev *dev, int val)
{
	int result, i;
	u8 d;

	for (i = 0; i < ARRAY_SIZE(gyro_scale_icm20608); ++i) {  /* 循环找出匹配值 */
		if (gyro_scale_icm20608[i] == val) {
			d = (i << 3);
			result = regmap_write(dev->regmap_spi, ICM20_GYRO_CONFIG, d);
			if (result)
				return result;
			return 0;
		}
	}
	return -EINVAL;
}

 /*
  * @description  	: 设置ICM20608的加速度计量程(分辨率)
  * @param - dev	: icm20608设备
  * @param - val   	: 量程(分辨率值)。
  * @return			: 0，成功；其他值，错误
  */
static int icm20608_write_accel_scale(struct icm20608_dev *dev, int val)
{
	int result, i;
	u8 d;

	for (i = 0; i < ARRAY_SIZE(accel_scale_icm20608); ++i) {
		if (accel_scale_icm20608[i] == val) {
			d = (i << 3);
			result = regmap_write(dev->regmap_spi, ICM20_ACCEL_CONFIG, d);
			if (result)
				return result;
			return 0;
		}
	}
	return -EINVAL;
}

/*
  * @description     	: 读函数，当读取sysfs中的文件的时候最终此函数会执行，此函数
  * 					：里面会从传感器里面读取各种数据，然后上传给应用。由通道设置区分不同读取
  * @param - indio_dev	: iio_dev
  * @param - chan   	: 通道
  * @param - val   		: 读取的值，如果是小数值的话，val是整数部分。
  * @param - val2   	: 读取的值，如果是小数值的话，val2是小数部分。
  * @param - mask   	: 掩码。区分读取的数据类型
  * @return				: 0，成功；其他值，错误
  */
static int icm20608_read_raw(struct iio_dev *indio_dev, struct iio_chan_spec const *chan,
			int *val, int *val2, long mask)
{
	struct icm20608_dev *dev = iio_priv(indio_dev);
	int ret = 0;
	unsigned char regdata = 0;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:								/* 读取ICM20608加速度计、陀螺仪、温度传感器(原始值) */
		mutex_lock(&dev->lock);								/* 上锁 			*/
		ret = icm20608_read_channel_data(indio_dev, chan, val); 	/* 读取通道值，返回值细分各个通道 */
		mutex_unlock(&dev->lock);							/* 释放锁 			*/
		return ret;
	case IIO_CHAN_INFO_SCALE:    /* (比例sacle) */
		switch (chan->type) {
		case IIO_ANGL_VEL:
			mutex_lock(&dev->lock);
			regdata = (icm20608_read_onereg(dev, ICM20_GYRO_CONFIG) & 0X18) >> 3;
			*val  = 0;
			*val2 = gyro_scale_icm20608[regdata];
			mutex_unlock(&dev->lock);
			return IIO_VAL_INT_PLUS_MICRO;	/* 值为val+val2/1000000 */
		case IIO_ACCEL:
			mutex_lock(&dev->lock);
			regdata = (icm20608_read_onereg(dev, ICM20_ACCEL_CONFIG) & 0X18) >> 3;
			*val = 0;
			*val2 = accel_scale_icm20608[regdata];;
			mutex_unlock(&dev->lock);
			return IIO_VAL_INT_PLUS_NANO;/* 值为val+val2/1000000000 */
		case IIO_TEMP:					
			*val = ICM20608_TEMP_SCALE/ 1000000;
			*val2 = ICM20608_TEMP_SCALE % 1000000;
			return IIO_VAL_INT_PLUS_MICRO;	/* 值为val+val2/1000000 */
		default:
			return -EINVAL;
		}
		return ret;
	case IIO_CHAN_INFO_OFFSET:		/* ICM20608温度传感器(offset)值 */
		switch (chan->type) {
		case IIO_TEMP:
			*val = ICM20608_TEMP_OFFSET;
			return IIO_VAL_INT;
		default:
			return -EINVAL;
		}
		return ret;
	case IIO_CHAN_INFO_CALIBBIAS:	/* ICM20608加速度计和陀螺仪校准(cailbbias)值 */
		switch (chan->type) {
		case IIO_ANGL_VEL:		/* 陀螺仪的校准值 */
			mutex_lock(&dev->lock);
			ret = icm20608_sensor_show(dev, ICM20_XG_OFFS_USRH, chan->channel2, val);
			mutex_unlock(&dev->lock);
			return ret;
		case IIO_ACCEL:			/* 加速度计的校准值 */
			mutex_lock(&dev->lock);	
			ret = icm20608_sensor_show(dev, ICM20_XA_OFFSET_H, chan->channel2, val);
			mutex_unlock(&dev->lock);
			return ret;
		default:
			return -EINVAL;
		}
		
	default:
		return ret -EINVAL;
	}
}

/*
  * @description     	: 写函数，当向sysfs中的文件写数据的时候最终此函数会执行，一般在此函数
  * 					：里面设置传感器，比如量程等。
  * @param - indio_dev	: iio_dev
  * @param - chan   	: 通道
  * @param - val   		: 应用程序写入的值，如果是小数值的话，val是整数部分。
  * @param - val2   	: 应用程序写入的值，如果是小数值的话，val2是小数部分。
  * @return				: 0，成功；其他值，错误
  */
static int icm20608_write_raw(struct iio_dev *indio_dev, struct iio_chan_spec const *chan,
			int val, int val2, long mask)
{
	struct icm20608_dev *dev = iio_priv(indio_dev);
	int ret = 0;

	switch (mask) {
	case IIO_CHAN_INFO_SCALE:	/* 设置陀螺仪和加速度计的分辨率(scale) */
		switch (chan->type) {
		case IIO_ANGL_VEL:		/* 设置陀螺仪 */
			mutex_lock(&dev->lock);
			ret = icm20608_write_gyro_scale(dev, val2);
			mutex_unlock(&dev->lock);
			break;
		case IIO_ACCEL:			/* 设置加速度计 */
			mutex_lock(&dev->lock);
			ret = icm20608_write_accel_scale(dev, val2);
			mutex_unlock(&dev->lock);
			break;
		default:
			ret = -EINVAL;
			break;
		}
		break;
	case IIO_CHAN_INFO_CALIBBIAS:	/* 设置陀螺仪和加速度计的校准值(cailbbias)*/
		switch (chan->type) {
		case IIO_ANGL_VEL:		/* 设置陀螺仪校准值 */
			mutex_lock(&dev->lock);
			ret = icm20608_sensor_set(dev, ICM20_XG_OFFS_USRH,
									    chan->channel2, val);
			mutex_unlock(&dev->lock);
			break;
		case IIO_ACCEL:			/* 加速度计校准值 */
			mutex_lock(&dev->lock);
			ret = icm20608_sensor_set(dev, ICM20_XA_OFFSET_H,
							             chan->channel2, val);
			mutex_unlock(&dev->lock);
			break;
		default:
			ret = -EINVAL;
			break;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

/*
  * @description     	: 用户空间写数据格式，比如我们在用户空间操作sysfs来设置传感器的分辨率，
  * 					：如果分辨率带小数，那么这个小数传递到内核空间应该扩大多少倍，此函数就是
  *						: 用来设置这个的。
  * @param - indio_dev	: iio_dev
  * @param - chan   	: 通道
  * @param - mask   	: 掩码
  * @return				: 0，成功；其他值，错误
  */
static int icm20608_write_raw_get_fmt(struct iio_dev *indio_dev,struct iio_chan_spec const *chan,long mask)
{
	switch (mask) {
	case IIO_CHAN_INFO_SCALE:
		switch (chan->type) {
		case IIO_ANGL_VEL:		/* 用户空间写的陀螺仪分辨率数据要乘以1000000 */
			return IIO_VAL_INT_PLUS_MICRO;
		default:				/* 用户空间写的加速度计分辨率数据要乘以1000000000 */
			return IIO_VAL_INT_PLUS_NANO;
		}
	default:
		return IIO_VAL_INT_PLUS_MICRO;
	}
	return -EINVAL;
}

/*
 * iio_info结构体变量
 */
static const struct iio_info icm20608_info={
	.read_raw=icm20608_read_raw,
	.write_raw=icm20608_write_raw,
	.write_raw_get_fmt=icm20608_write_raw_get_fmt,     /* 用户空间写数据格式 */ 
};

 /*
  * @description     : spi驱动的probe函数，当驱动与
  *                    设备匹配以后此函数就会执行
  * @param - client  : i2c设备
  * @param - id      : i2c设备ID
  * 
  */	
static int icm20608_probe(struct spi_device *spi)
{
	int ret;
	struct icm20608_dev *icm20608;
	struct iio_dev *indio_dev;   /* icm20608作为隐私数据存储到indio_dev中 */

	printk("led driver and device has matched!\r\n");

	/* 1、iio_申请 */
	indio_dev=devm_iio_device_alloc(&spi->dev,sizeof(icm20608));
	/* struct iio_dev *devm_iio_device_alloc(struct device *dev, int sizeof_priv); */
	if(!indio_dev){
		return -ENOMEM;
	}

	/* 2、获取icm20608_dev结构体地址 */
	icm20608=iio_priv(indio_dev);
	/* static inline void *iio_priv(const struct iio_dev *indio_dev) */
	icm20608->spi = spi;
	spi_set_drvdata(spi, indio_dev);    /* 保存icm20608结构体 ，设置私有数据 */
	mutex_init(&icm20608->lock);   /* 初始化互斥体 */
	
	/* 3、iio_dev的其他成员变量 */
	indio_dev->dev.parent=&spi->dev;    /* 参考bma180.c */
	indio_dev->modes=INDIO_DIRECT_MODE;   /* 直接模式，提供sysfs接口 */
	indio_dev->channels=icm20608_channels;    /* IIO设备通道，为iio_chan_spec结构体类型 */
	indio_dev->info=&icm20608_info;    /* iio_info 结构体类型，这个结构体里面有很多函数，需要驱动开发人员编写，从用户空间读取IIO设备内部数据 */
	indio_dev->name=ICM20608_NAME;      
	indio_dev->num_channels=ARRAY_SIZE(icm20608_channels);    /* 用于计算数组的大小（元素数量）-> (sizeof(arr) / sizeof(arr[0])), 为 IIO 设备的通道数 */

	ret=iio_device_register(indio_dev);
	if (ret < 0) {
		dev_err(&spi->dev, "iio_device_register failed\n");   /* 打印pirntk */
		goto err_iio_register;
	}

	/* 初始化regmap_config设置 */
	icm20608->config_spi.reg_bits=8;  /* 寄存器长度8bit */
	icm20608->config_spi.val_bits=8;  /* 值长度8bit */
	icm20608->config_spi.read_flag_mask=0x80;   /* 读掩码设置为0X80，ICM20608使用SPI接口读的时候寄存器最高位应该为1 */

	icm20608->regmap_spi=regmap_init_spi(spi,&icm20608->config_spi);
	/*struct regmap * regmap_init_spi(struct spi_device *spi, const struct regmap_config *config)*/
	if (IS_ERR(icm20608->regmap_spi)) {
		return  PTR_ERR(icm20608->regmap_spi);
		goto err_regmap_init;
	}

	/*初始化spi_device */
	spi->mode = SPI_MODE_0;	/*MODE0，CPOL=0，CPHA=0*/
	spi_setup(spi);   /* 参数的一些默认设置和检查 */
	
	/* 初始化ICM20608内部寄存器 */
	icm20608_reginit(icm20608);	

	return 0;
err_regmap_init:
    iio_device_unregister(indio_dev);
err_iio_register:
	return ret;
}

/*
 * @description     : i2c驱动的remove函数，移除i2c驱动的时候此函数会执行
 * @param - client 	: i2c设备
 * @return          : 0，成功;其他负值,失败
 */
static int icm20608_remove(struct spi_device *spi)
{
	struct iio_dev *indio_dev = spi_get_drvdata(spi);
	struct icm20608_dev *icm20608;
	icm20608=iio_priv(indio_dev);
	/* 删除设备 */
	regmap_exit(icm20608->regmap_spi);

	/* 注销IIO */
	iio_device_unregister(indio_dev);

	return 0;
}

/* 传统匹配方式ID列表 */
static const struct spi_device_id icm20608_id[] = {
	{"alientek,icm20608", 0},  
	{}
};

/* 设备树匹配列表 */
static const struct of_device_id icm20608_of_match[] = {
	{ .compatible = "myspi,icm20608" },
	{ /* Sentinel */ }
};

/* SPI驱动结构体 */	
static struct spi_driver icm20608_driver = {
	.probe = icm20608_probe,
	.remove = icm20608_remove,
	.driver = {
			.owner = THIS_MODULE,
		   	.name = "icm20608",
		   	.of_match_table = icm20608_of_match, 
		   },
	.id_table = icm20608_id,
};
		   
/*
 * @description	: 驱动入口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init icm20608_init(void)
{
	return spi_register_driver(&icm20608_driver);
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit icm20608_exit(void)
{
	spi_unregister_driver(&icm20608_driver);
}

module_init(icm20608_init);
module_exit(icm20608_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("cvvo");



