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

/* 命令值 */
#define CLOSE_CMD 		(_IO(0XEF, 0x1))	/* 关闭定时器 */
#define OPEN_CMD		(_IO(0XEF, 0x2))	/* 打开定时器 */
#define SETPERIOD_CMD	(_IO(0XEF, 0x3))	/* 设置定时器周期命令 */

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
    int fd;
    int ret=0;
    char *filename;
    unsigned char data;
    fd_set readfds;
    struct timeval timerout;
    struct pollfd fds;
    
    if(argc!= 2){
		printf("Error Usage!\r\n");
		return -1;
	}

    filename=argv[1];

    /* 打开timer驱动 */
    fd=open(filename, O_RDWR|O_NONBLOCK);  /* 非阻塞访问 */
    if(fd<0){
        printf("file %s open failed!\r\n", argv[1]);
		return -1;
    }

#if 0
    /* 采用poll处理轮询 */
    fds.fd=fd;   /* 文件描述符 */
    fds.events=POLLIN;  /* 请求的事件 */

    while(1){
        ret=poll(&fds,1,500);
        /* 函数原型 int poll(struct pollfd *fds, nfds_t nfds, int timeout) */
        if(ret){  /* 数据有效，读取数据 */
            ret=read(fd,&data,sizeof(data));
            if(ret<0){   /* 数据读取错误或无效 */
    
            }else{
                if(data){   /* 读取到正确数据 */
                    printf("key value data=%#X\r\n",data);
                }
            }    
        }else if(ret==0){  /* 超时 */
            /* 超时处理 */
        }else if(ret<0){  /* 错误 */
            /* 错误处理 */
        }
    }
#endif

    /* 采用select处理轮询 */
    while(1){
        timerout.tv_sec=0;
        timerout.tv_usec=500000;  /* 500ms，为轮询时间 */
        FD_ZERO(&readfds);  /* 将 fd_set 变量的所有位都清零，初始化 */  
        FD_SET(fd,&readfds);   /* FD_SET用于将fd_set变量(readfds)的某个位置1，也就是向fd_set添加一个文件描述符 */

        ret=select(fd+1,&readfds,NULL,NULL,&timerout);    /* 写操作和异常处理不设置 */
        printf("select num\r\n");
        /*函数原型 int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)*/
        switch(ret){
            case 0:   /* 超时 */
                /* 定义超时处理 */
                break;
            case -1:  /* 错误 */
                /* 定义错误处理 */
                break;
            default:
                if(FD_ISSET(fd,&readfds)){   /* 用于测试一个文件是否属于某个集合 */
                    ret=read(fd,&data,sizeof(data));
                    if(ret<0){   /* 数据读取错误或无效 */
    
                    }else{
                        if(data){   /* 读取到正确数据 */
                            printf("key value data=%#X\r\n",data);
                        }
                    }
                }
                break;
        }
    }

    ret=close(fd);
    if(ret < 0){
		printf("file %s close failed!\r\n", argv[1]);
		return -1;
	}

    return ret;
}
