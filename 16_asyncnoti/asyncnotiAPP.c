#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include "poll.h"
#include "sys/select.h"
#include "sys/time.h"
#include "linux/ioctl.h"
#include "signal.h"

/* 命令值 */
#define CLOSE_CMD 		(_IO(0XEF, 0x1))	/* 关闭定时器 */
#define OPEN_CMD		(_IO(0XEF, 0x2))	/* 打开定时器 */
#define SETPERIOD_CMD	(_IO(0XEF, 0x3))	/* 设置定时器周期命令 */

static int fd=0;  /* 文件描述符 */

/*
 * SIGIO信号处理函数
 * @param - signum 	: 信号值
 * @return 			: 无 
 *  信号处理函数原型如下所示
 * typedef void (*sighandler_t)(int)

 */
static void sigio_signal_func(int signum)
{
    unsigned char keyvalue;
    int ret;

    ret=read(fd,&keyvalue,sizeof(keyvalue));   /*此函数通过 read 函数读取按键值，然后通过printf 函数打印在终端上*/
    if(ret<0){   /* 数据读取错误或无效 */
    
    }else{
        if(keyvalue){   /* 读取到正确数据 */
            printf("key value data=%#X\r\n",keyvalue);
        }
    }    
}

/* 字符设备应用开发 */
/*
 * @description		: main主程序
 * @param - argc 	: argv数组元素个数，应用程序参数个数，如使用 ls -l：argv=2，argv为字符串 
 * @param - argv[] 	: 具体参数
 * @return 			: 0 成功;其他 失败
 * 使用方法	 ：./imx6ullirqAPP /dev/imx6ullirq 	
 */

int main(int argc, char *argv[])
{
    int ret=0;
    int flags=0;
    char *filename;
    
    if(argc!= 2){
		printf("Error Usage!\r\n");
		return -1;
	}

    filename=argv[1];

    /* 打开按键异步通知驱动 */
    fd=open(filename, O_RDWR|O_NONBLOCK);  /* 非阻塞访问 */
    if(fd<0){
        printf("file %s open failed!\r\n", argv[1]);
		return -1;
    }

    /* 设置信号SIGIO的处理函数 */
    signal(SIGIO,sigio_signal_func);
    /* 函数原型sighandler_t signal(int signum, sighandler_t handler) */

    /*fcntl函数——根据文件描述词来操作文件的特性*/
    fcntl(fd,F_SETOWN,getpid());    /* 设置当前进程接收SIGIO信号，F_SETOWN——设置当前接受SIGIO和SIGURG信号的进程和ID进程组ID */
    flags=fcntl(fd, F_GETFL);        /* 获取当前的进程状态，由文件描述符获得文件状态标志位，例如（O_RDONLY 、O_WRONLY、O_RDWR等） 	*/
    fcntl(fd,F_SETFL,flags|FASYNC);  /* 开启当前进程异步通知功能
                F_SETFL设置文件状态标志—状态标志总共有7个：O_RDONLY、O_WRONLY、O_RDWR、O_APPEND、O_NONBLOCK、O_SYNC和O_ASYNC */
    while(1){
        sleep(2);  /*在Linux下,sleep()里面的单位是秒，而不是毫秒 */
    }

    ret=close(fd);
    if(ret < 0){
		printf("file %s close failed!\r\n", argv[1]);
		return -1;
	}

    return ret;
}
