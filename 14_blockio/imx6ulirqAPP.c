#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <linux/ioctl.h>

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
    
    if(argc!= 2){
		printf("Error Usage!\r\n");
		return -1;
	}

    filename=argv[1];

    /* 打开timer驱动 */
    fd=open(filename, O_RDWR);
    if(fd<0){
        printf("file %s open failed!\r\n", argv[1]);
		return -1;
    }

    while(1){
        ret=read(fd,&data,sizeof(data));
        if(ret<0){   /* 数据读取错误或无效 */
    
        }else{
            if(data){   /* 读取到正确数据 */
                printf("key value data=%#X\r\n",data);
            }
        }
    }

    ret=close(fd);
    if(ret < 0){
		printf("file %s close failed!\r\n", argv[1]);
		return -1;
	}

    return ret;
}
