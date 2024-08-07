#include <linux/module.h>
#include <linux/ratelimit.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/input/mt.h>
#include <linux/input/touchscreen.h>
#include <linux/input/edt-ft5x06.h>
#include <linux/i2c.h>

#define WIGHT 1024
#define HIGH  600

#define MAX_SUPPORT_POINTS		5			/* 5点触摸 	*/
#define TOUCH_EVENT_DOWN		0x00		/* 按下 	*/
#define TOUCH_EVENT_UP			0x01		/* 抬起 	*/
#define TOUCH_EVENT_ON			0x02		/* 接触 	*/
#define TOUCH_EVENT_RESERVED	0x03		/* 保留 	*/

/* FT5X06寄存器相关宏定义 */
#define FT5X06_TD_STATUS_REG	0X02	/*	状态寄存器地址 		*/
#define FT5426_TOUCH1_XH		0X03	/* 触摸点坐标寄存器,开始记录着触摸屏的触摸点坐标信息
										   一个触摸点坐标信息用12bit表示，其中H寄存器的(bit3:0)这4个bit为高4位，L寄存器的(bit7:0)为低8位
										   XH寄存器，(bit7:6)表示事件，事件标志：00：按下、01：抬起、10：接触、11：保留
										   YH寄存器，(bit7:4)表示触摸ID，
										 * 一个触摸点用6个寄存器存储坐标数据，后两个寄存器保留，共30个寄存器*/
#define FT5x06_DEVICE_MODE_REG	0X00 		/* 模式寄存器 			*/
#define FT5426_IDGLIB_VERSION1	0XA1 	/* 固件版本寄存器，(bit7:0)高字节 */
#define FT5426_IDGLIB_VERSION2	0XA2 	/* 固件版本寄存器，(bit7:0)低字节 */
#define FT5426_IDG_MODE_REG		0XA4		/* 中断模式				*/
#define FT5X06_READLEN			29			/* 要读取的寄存器个数，从0x02~0x1E */

/* ft5x06设备结构体 */
struct ft5x06_dev{
	struct device_node *nd;   /* 设备都是以节点的形式“挂”到设备树上的，因此要想获取这个设备的其他属性信息，必须先获取到这个设备的节点。
							  Linux内核使用device_node结构体来描述一个节点 */ 
	int gpio_irq,gpio_reset;   /* 中断和复位IO		*/
	int irqnum;             /* 中断号    		*/
	struct i2c_client *client;				/* I2C客户端 		*/
	struct input_dev *input;  /* input结构体变量 */
};

struct ft5x06_dev ft5x06;

/*
 * @description     : 复位FT5X06
 * @param - client 	: 要操作的i2c
 * @param - multidev: 自定义的multitouch设备
 * @return          : 0，成功;其他负值,失败
 */
static int ft5x06_ts_reset(struct i2c_client *client, struct ft5x06_dev *dev)
{
	int ret=0;

	if(gpio_is_valid(dev->gpio_reset)){    /* 检查IO是否有效 */
		/* 申请复位IO，并且默认输出低电平 */
		ret=devm_gpio_request_one(&client->dev,dev->gpio_reset,GPIOF_OUT_INIT_LOW,"edt-ft5x06 reset");   /* 底板上CT_INT和CT_RST接上拉电阻，默认高电平  */
		/*  GPIOF_OUT_INIT_LOW->将GPIO设置成默认低电平输出（注意，这里是物理电平） */
		/* int devm_gpio_request_one(struct device *dev, unsigned gpio, unsigned long flags, const char *label); */

		if(ret){
			return ret;
		}

		msleep(5);
		gpio_set_value(dev->gpio_reset,1); /* 输出高电平，停止复位 */
		msleep(300);
	}
	return 0;
}

/*
 * @description	: 从ft5x06读取多个寄存器数据，此函数在测试其他 I2C 设备的时候可以实现多给字节连续读取，但是在 FT5X06 上不能连续读取多个字节。不过读取一个字节没有问题的
 * @param - dev:  ft5x06设备
 * @param - reg:  要读取的寄存器首地址
 * @param - val:  读取到的数据
 * @param - len:  要读取的数据长度
 * @return 		: 操作结果
 */
static int ft5x06_read_regs(struct ft5x06_dev *dev,u8 reg,void *val, int len)
{
	int ret=0;
	struct i2c_msg msg[2];
	struct i2c_client *client =(struct i2c_client *)dev->client;
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
		printk("i2c rd failed=%d reg=%#X len=%d addr=%#X\n",ret,reg,len,client->addr);
		ret = -EREMOTEIO;
	}
	return ret;
}

/*
 * @description	: 向ft5x06多个寄存器写入数据
 * @param - dev:  ft5x06设备
 * @param - reg:  要写入的寄存器首地址
 * @param - val:  要写入的数据缓冲区
 * @param - len:  要写入的数据长度
 * @return 	  :   操作结果
 */
static signed int ft5x06_write_regs(struct ft5x06_dev *dev, u8 reg, u8 *val,u8 len)
{
	u8 buf[256];
	struct i2c_msg msg;
	struct i2c_client *client =(struct i2c_client *)dev->client;
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
 * @description	: 向ft5x06指定寄存器写入指定的值，写一个寄存器
 * @param - dev:  ft5x06设备
 * @param - reg:  要写的寄存器
 * @param - data: 要写入的值
 * @return   :    无
 */
static void ft5x06_write_reg(struct ft5x06_dev *dev, u8 reg, u8 data)
{
	u8 buf=0;
	buf=data;
	ft5x06_write_regs(dev,reg,&buf,1);    /* 写入一个寄存器值 */
}


/*
 * @description     : FT5X06中断服务函数
 * @param - irq 	: 中断号 
 * @param - dev_id	: 设备结构。
 * @return 			: 中断执行结果
 */
static irqreturn_t ft5x06_handler(int irq, void *dev_id)
{
	struct ft5x06_dev *multidata=(struct ft5x06_dev *)dev_id;

	int ret;
	u8 i=0;
	u8 offset,tplen;
	u8 rxdata[29];
	u16 id,x,y,type;
	bool down;
	

	offset = 1; 	/* 偏移1，也就是0X02+1=0x03,从0X03开始是触摸值 */
	tplen = 6;		/* 一个触摸点有6个寄存器来保存触摸值 */

	memset(rxdata,0,sizeof(rxdata));  /* 清除 */

	/* 读取FT5X06触摸点坐标从0X02寄存器开始，连续读取29个寄存器 */
	ret=ft5x06_read_regs(multidata,FT5X06_TD_STATUS_REG,rxdata,FT5X06_READLEN);
		if (ret) {
		goto fail;
	}

	/* 上报每一个触摸点坐标 ??? */
	for(i=0;i<MAX_SUPPORT_POINTS;i++){
		type=rxdata[i*tplen+offset+0]>>6;
		if (type == TOUCH_EVENT_RESERVED)
			continue;

		/* 我们所使用的触摸屏和FT5X06是反过来的 */ 
		x=((rxdata[i*tplen+offset+2]<<8)|(rxdata[i*tplen+offset+3]))&0x0fff;
		y=((rxdata[i*tplen+offset+0]<<8)|(rxdata[i*tplen+offset+1]))&0x0fff;

		id=(rxdata[i*tplen+offset+2]>>4)&0x0f;

		down = type != TOUCH_EVENT_UP;

		input_mt_slot(multidata->input,id);
		/* void input_mt_slot(struct input_dev *dev, int slot), 上报当前触摸点SLOT，触摸点的SLOT其实就是触摸点ID*/

		input_mt_report_slot_state(multidata->input,MT_TOOL_FINGER, down);   /* ??? */
/* void input_mt_report_slot_state( struct input_dev *dev, unsigned int tool_type, bool active)——通过修改SLOT关联的ABS_MT_TRACKING_ID来完成对触摸点的添加、替换或删除 */

		if (!down){
			continue;
		}
			
		input_report_abs(multidata->input,ABS_MT_POSITION_X,x);
		/* void input_report_abs( struct input_dev *dev, unsigned int code, int value) 上报触摸点 坐标*/
		input_report_abs(multidata->input,ABS_MT_POSITION_Y,y);

	}
	input_mt_report_pointer_emulation(multidata->input,true);    /* ??? */
	/* void input_mt_report_pointer_emulation(struct input_dev *dev, bool use_count) */
	input_sync(multidata->input);   /* 所有的触摸点坐标都上传完毕以后就得发送 SYN_REPORT 事件 */

fail:
	return IRQ_RETVAL(IRQ_HANDLED);
}

/*
 * @description     : FT5x06中断初始化
 * @param - client 	: 要操作的i2c
 * @param - multidev: 自定义的multitouch设备
 * @return          : 0，成功;其他负值,失败
 */
static int ft5x06_ts_irq(struct i2c_client *client, struct ft5x06_dev *dev)
{
	int ret=0;
	/* 1、申请IO */
	if(gpio_is_valid(dev->gpio_irq)){
		ret=devm_gpio_request_one(&client->dev,dev->gpio_irq,GPIOF_IN,"edt-ft5x06 irq");   /* 输入模式 */
		if (ret) {
			dev_err(&client->dev,"Failed to request GPIO %d, error %d\n",dev->gpio_irq, ret);   /* ??? */
			return ret;
		}
	}

	/* 2、申请中断 ??? */
	ret=devm_request_threaded_irq(&client->dev,client->irq,NULL,ft5x06_handler,IRQF_TRIGGER_FALLING | IRQF_ONESHOT,client->name,&ft5x06);
	/* int devm_request_threaded_irq(struct device *dev, unsigned int irq,irq_handler_t handler, irq_handler_t thread_fn,
	unsigned long irqflags, const char *devname,void *dev_id);
	hardirq处理程序完成后，中断不会重新启用。由需要保持的线程中断使用在线程处理程序运行之前，irq行被禁用。*/
	if(ret){
		dev_err(&client->dev, "Unable to request touchscreen IRQ.\n");
		return ret;
	}
	return 0;
}

 /*
  * @description     : i2c驱动的probe函数，当驱动与
  *                    设备匹配以后此函数就会执行
  * @param - client  : i2c设备
  * @param - id      : i2c设备ID
  * @return          : 0，成功;其他负值,失败
  */
static int ft5x06_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret=0;
	u8 value[4];    /* 后面会对value取地址，赋初始值会产生错误*/
	u8 test_value;

	printk("led driver and device has matched!\r\n");


	ft5x06.client = client;

	/* 获取设备树中的中断和复位引脚 */
	ft5x06.gpio_irq=of_get_named_gpio(client->dev.of_node,"interrupt-gpios",0);
	ft5x06.gpio_reset=of_get_named_gpio(client->dev.of_node,"reset-gpios",0);
	printk("gpio_irq=%d gpio_reset=%d\r\n",ft5x06.gpio_irq,ft5x06.gpio_reset);

	/*复位FT5426*/
	ret=ft5x06_ts_reset(client,&ft5x06);
	if(ret < 0) {
		goto fail;
	}
	
	/* 初始化中断 */
	ret=ft5x06_ts_irq(client,&ft5x06);
	if(ret < 0) {
		goto fail;
	}

	/* 初始化FT5X06 */
	ft5x06_write_reg(&ft5x06,FT5x06_DEVICE_MODE_REG,0);    /*设置进入正常模式*/
  	ft5x06_write_reg(&ft5x06,FT5426_IDG_MODE_REG,1);      /*设置中断模式为触发模式1*/

	ft5x06_read_regs(&ft5x06,FT5426_IDGLIB_VERSION1,value,4);
	printk("IDGLIB_VERSION1=%#X IDGLIB_VERSION2=%#X 0xA3=%#X DG_MODE_REG=%#X\r\n",value[0],value[1],value[2],value[3]);

	ft5x06_read_regs(&ft5x06,FT5x06_DEVICE_MODE_REG,&test_value,1);
	printk("DEVICE_MODE_RE=%#X\r\n",test_value);

#if 0
	/* 注册input_dev  */
	/* 1、申请input_dev */
	ft5x06.input=devm_input_allocate_device(&ft5x06.client->dev);   /* 无需释放 */
	if(!ft5x06.input){
		ret = -ENOMEM;
		goto fail;
	}
	ft5x06.input->name=ft5x06.client->name;
	/* ？？？？ */
	ft5x06.input->id.bustype = BUS_I2C;
	ft5x06.input->dev.parent = &ft5x06.client->dev;

	/* 2、初始化input_dev和MT */
	/* 第一种设置产生哪些事件*/
	__set_bit(EV_KEY,ft5x06.input->evbit);   /* 设置产生按键事件 */
	__set_bit(EV_ABS,ft5x06.input->evbit);   /* 绝对值触摸屏 */
	__set_bit(BTN_TOUCH,ft5x06.input->evbit);   /* 点触摸 */
	
	/* 初始化input_dev，设置产生哪些按键 */
	input_set_abs_params(ft5x06.input,ABS_X,0,WIGHT,0,0);
	input_set_abs_params(ft5x06.input,ABS_Y,0,HIGH,0,0);
	input_set_abs_params(ft5x06.input,ABS_MT_POSITION_X,0,WIGHT,0,0);
	input_set_abs_params(ft5x06.input,ABS_MT_POSITION_Y,0,HIGH,0,0);

	ret=input_mt_init_slots(ft5x06.input,MAX_SUPPORT_POINTS,0);   /* 用于初始化MT的输入slots */
	/* int input_mt_init_slots( struct input_dev *dev, unsigned int num_slots, unsigned int flags) */
	if (ret) {
		goto fail;
	}

	ret=input_register_device(ft5x06.input);
	if(ret){
		printk("register input device failed!\r\n");
		goto fail;
	}
#endif
	return 0;

fail:
	return ret;
}

/*
 * @description     : i2c驱动的remove函数，移除i2c驱动的时候此函数会执行
 * @param - client 	: i2c设备
 * @return          : 0，成功;其他负值,失败
 */
static int ft5x06_ts_remove(struct i2c_client *client)
{
	/* 释放input_dev */
	//input_unregister_device(ft5x06.input);

	printk("led_exit\r\n");
	return 0;
}

static const struct i2c_device_id ft5x06_ts_id[]={
	{"edt-ft5206", 0,},
	{"edt-ft5426", 0,},
	{/* Sentinel */}
};

static const struct of_device_id ft5x06_of_match[]={   /* 需要包括最后一个空元素 */
	{ .compatible = "edt,edt-ft5206", },
	{ .compatible = "edt,edt-ft5426", },
	{/* Sentinel */}
};

/* platform驱动结构体 */
static struct i2c_driver ft5x06_ts_driver={
	.probe=ft5x06_ts_probe,
	.remove=ft5x06_ts_remove,
	.driver={
		.owner=THIS_MODULE,
		.name="edt_ft5x06",       /* 驱动名字，用于和设备匹配 */
		.of_match_table=of_match_ptr(ft5x06_of_match),    /* ??? */
	},
	.id_table=ft5x06_ts_id,
};

/*
 * @description	: 驱动模块加载函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init ft5x06_init(void)
{
	int ret=0;
	ret=i2c_add_driver(&ft5x06_ts_driver);
	return ret;
}

/*
 * @description	: 驱动模块卸载函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit ft5x06_exit(void)
{
	i2c_del_driver(&ft5x06_ts_driver);
}

/* 注册驱动加载和卸载 */
module_init(ft5x06_init);
module_exit(ft5x06_exit);

/* LICENSE和作者信息 */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("CVVO");