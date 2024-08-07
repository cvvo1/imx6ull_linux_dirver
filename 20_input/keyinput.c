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
#include <linux/input.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/of_irq.h>
#include <linux/irq.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define KEYINPUT_CNT    1     /* 设备号个数 */
#define KEYINPUT_NAME  "keyinput"   /* 设备名字 */

/* 定义按键值 */
#define KEY_NUM     1                   /* 按键数量 	*/

/* 中断IO描述结构体 */
struct irq_keydesc{
	int gpio_key;   /* gpio编号 */
	int irqnum;   /* 中断号     */
	unsigned char value;  /* 按键对应的键值 */
	char name[10];   /* 中断名字 */
	irqreturn_t (*handler) (int, void *);    /* 中断服务函数 */
};

/* timer设备结构体 */
struct keyinput_dev{
	struct device_node *nd;   /* 设备都是以节点的形式“挂”到设备树上的，因此要想获取这个设备的其他属性信息，必须先获取到这个设备的节点。
							 Linux内核使用device_node结构体来描述一个节点 */ 

	int timerperiod;   /* 定时周期，单位为ms，系统频率为100 Hz */
	struct timer_list timer;  /* 定义一个定时器 */

	struct irq_keydesc irqkeydesc[KEY_NUM]; /* 按键描述结构体数组,开发板上只有一个按键，因此irqkeydesc数组只有一个元素*/  

	unsigned char curkeynum;	/* 当前的按键号 */

	struct input_dev *keyinput;  /* input结构体变量 */
	
};

struct keyinput_dev keyinput;


/* @description		: 中断服务函数，开启定时器，延时10ms，
 *				  	  定时器用于按键消抖。
 * @param - irq 	: 中断号 
 * @param - dev_id	: 设备结构。
 * @return 			: 中断执行结果
 */
static irqreturn_t key0_handler(int irq, void *dev_id)
{	
	struct keyinput_dev *dev=(struct keyinput_dev *)dev_id;
	dev->curkeynum=0;
	/* 开启定时器 */
	dev->timer.data=(unsigned long)dev;  /* 设置要传递给function函数的参数 */
	mod_timer(&dev->timer,jiffies+msecs_to_jiffies(dev->timerperiod));

	return IRQ_RETVAL(IRQ_HANDLED);
	
}


/* static一般用于修饰局部变量，全局变量，函数 */

/* @description	: 定时器服务函数，用于按键消抖，定时器到了以后
 *				  再次读取按键值，如果按键还是处于按下状态就表示按键有效。
 * @param - arg	: 设备结构变量
 * @return 		: 无
 */
void timer_function(unsigned long arg)
{
	struct keyinput_dev *dev=(struct keyinput_dev *)arg;
	unsigned char num,keyvalue;
	struct irq_keydesc *keydesc;

	num=dev->curkeynum;
	keydesc=&dev->irqkeydesc[num];
	keyvalue=gpio_get_value(keydesc->gpio_key);  /* 读取IO值 */

	/* 上报事件 */
	if(keyvalue==0){       /* 按下按键 */
		input_report_key(dev->keyinput,KEY_0,1); /* 最后一个参数表示按下还是松开，1为按下，0为松开 */
		input_sync(dev->keyinput);
	}else{               /* 按键松开 */
		input_report_key(dev->keyinput,KEY_0,0);
		input_sync(dev->keyinput);
	}
}

 /*
  * @description : 初始化按键 IO， open 函数打开驱动的时候
  *  初始化按键所使用的 GPIO 引脚。
  *  @param : 无
  *  @return : 无
 */
static int keyio_init(void)
{
	int ret=0;
	unsigned char i=0;

	/* 设置key所使用的GPIO */
	/* 1、获取设备节点：key */
	keyinput.nd=of_find_node_by_path("/key"); /* 通过路径来查找指定的节点，path：带有全路径的节点名，可以使用节点的别名 */
	if(keyinput.nd==NULL){
		printk("key node not find!\r\n");
		return -EINVAL;  /* 返回无效参数符号 */
	}else{
		printk("key node find!\r\n");
	}

	/* 2、获取设备树中的gpio属性，得到key所使用的key编号*/
	for(i=0;i<KEY_NUM;i++){
		keyinput.irqkeydesc[i].gpio_key=of_get_named_gpio(keyinput.nd,"key-gpios",i); 
		 /* 类似<&gpio5 7 GPIO_ACTIVE_LOW>的属性信息转换为对应的GPIO编号 */
	/* 函数原型：int of_get_named_gpio(struct device_node *np,const char *propname,int index) */
		if(keyinput.irqkeydesc[i].gpio_key<0){
			printk("can't get key-gpios!\r\n");
			return -EINVAL;  /* 返回无效参数符号 */
		}else{
			printk("key%d-gpios num=%d\r\n",i,keyinput.irqkeydesc[i].gpio_key);
		}
	}

	/* 3、始化key所使用的IO，并且设置成中断模式 */
	for(i=0;i<KEY_NUM;i++){
		memset(keyinput.irqkeydesc[i].name,0,sizeof(keyinput.irqkeydesc[i].name));   /* memset一般使用“0”初始化内存单元,而且通常是给数组或结构体进行初始化 */
		sprintf(keyinput.irqkeydesc[i].name,"KEY%d",i);

		/* 申请IO，申请后能被其他设备检测，避免重复使用 */
		ret=gpio_request(keyinput.irqkeydesc[i].gpio_key,keyinput.irqkeydesc[i].name);
		/* 函数原型： int gpio_request(unsigned gpio, const char *label)*/
		if(ret<0){
			printk("key%d io request fail!\r\n",i);
			return -EINVAL;  /* 返回无效参数符号 */
		}
		/* 设置key gpio为输入 */
		ret=gpio_direction_input(keyinput.irqkeydesc[i].gpio_key);
		/* 函数原型：int gpio_direction_input(unsigned gpio, int value) */
		if(ret<0){
			printk("can't set key%d gpio!\r\n",i);
			return -EINVAL;  /* 返回无效参数符号 */
		}
		/* 获取中断号 */
		keyinput.irqkeydesc[i].irqnum=irq_of_parse_and_map(keyinput.nd,i);

		printk("key%d:gpio=%d, irqnum=%d\r\n",i,keyinput.irqkeydesc[i].gpio_key,keyinput.irqkeydesc[i].irqnum);
	}

	/* 申请中断 */
	keyinput.irqkeydesc[0].handler=key0_handler;
	keyinput.irqkeydesc[0].value = KEY_0;
	for(i=0;i<KEY_NUM;i++){
		ret=request_irq(keyinput.irqkeydesc[i].irqnum,keyinput.irqkeydesc[i].handler, IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING,
						keyinput.irqkeydesc[i].name,&keyinput);
		if(ret<0){
			printk("key%d irq-%d request failed!\r\n",i,keyinput.irqkeydesc[i].irqnum);
			return -EFAULT;
		}
	}

	/* 创建定时器 */
	init_timer(&keyinput.timer);
	keyinput.timer.function=timer_function;
	keyinput.timerperiod=10;        /* 定时周期为10ms */

	/* 注册input_dev  */
	/* 1、申请input_dev */
	keyinput.keyinput=input_allocate_device();
	keyinput.keyinput->name=KEYINPUT_NAME;

	/* 2、初始化input_dev */
#if 0
	/* 第一种设置产生哪些事件*/
	__set_bit(EV_KEY,keyinput.keyinput->evbit);   /* 设置产生按键事件 */
	__set_bit(EV_REP,keyinput.keyinput->evbit);   /* 重复事件，比如按下去不放开，就会一直输出信息 */
	/* 初始化input_dev，设置产生哪些按键 */
	__set_bit(KEY_0,keyinput.keyinput->keybit); 
 
	/* 第二种 */
	keyinput.keyinput->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP);   /* BIT_MASK和 BIT_WORD？？？*/
	keyinput.keyinput->keybit[BIT_WORD(KEY_0)] |= BIT_MASK(KEY_0); 
#endif                                  /* BIT_WORD查看对应的位图为第几个32位，如nr=35,BIT_WORD(35)=35/32=1 */
										/* BIT_MASK(nr),nr=35,=1<<(35%32)——综上所述：addr[1]|=1<<3 */
	/* 第三种 */
	keyinput.keyinput->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP);
	input_set_capability(keyinput.keyinput, EV_KEY, KEY_0);   /* input_set_capability——设置输入设备各种能力
						void input_set_capability(struct input_dev *dev, unsigned int type, unsigned int code); */

	ret=input_register_device(keyinput.keyinput);
	if(ret<0){
		printk("register input device failed!\r\n");
		return ret;
	}

	return ret;
}

/* 驱动入口 */
static int __init keyinput_init(void)
{	
	int ret=0;
	ret=keyio_init();   /* 初始化ledio */
	if(ret<0){
		return ret;
	}
	return 0;
}

/* 驱动出口 */
static void __exit keyinput_exit(void)
{
	unsigned char i=0;
	del_timer_sync(&keyinput.timer);	/* 删除timer定时期 */

	/* 释放中断 */
	for(i=0;i<KEY_NUM;i++){
		free_irq(keyinput.irqkeydesc[i].irqnum,&keyinput);  /* dev为指针参数，不需要取址符号& */
		/* 释放IO,申请和释放配合使用 */
		gpio_free(keyinput.irqkeydesc[i].gpio_key);
	}

	/* 释放input_dev */
	input_unregister_device(keyinput.keyinput);
	input_free_device(keyinput.keyinput);

	printk("timer_exit\r\n");

}

/* 注册驱动加载和卸载 */
module_init(keyinput_init);
module_exit(keyinput_exit);

/* LICENSE和作者信息 */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("CVVO");