#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "sys/ioctl.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include <poll.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>

/* 字符设备应用开发 */
/*
 * @description		: main主程序
 * @param - argc 	: argv数组元素个数，应用程序参数个数，如使用 ls -l：argv=2，argv为字符串 
 * @param - argv[] 	: 具体参数
 * @return 			: 0 成功;其他 失败
 * 使用方法	 ：./ap3216cApp /dev/ap3216c	
 */

int main(int argc, char *argv[])
{
    int fd,ret=0;
    char *filename;
    unsigned short databuf[3];
    unsigned short ir,als,ps;

    if(argc!= 2){
		printf("Error Usage!\r\n");
		return -1;
	}

    filename=argv[1];

    /* 打开led驱动 */
    fd=open(filename, O_RDWR);
    if(fd<0){
        printf("file %s open failed!\r\n", argv[1]);
		return -1;
    }

    while(1){
        ret=read(fd,databuf,sizeof(databuf));
        if(ret<0){
            /* 错误处理 */
        }else{
            ir=databuf[0];   /* ir传感器数据，红外LED灯 */
            als=databuf[1];  /* 环境光 */
            ps=databuf[2];     /* 接近传感器 */
            printf("ir=%d als=%d ps=%d\r\n",ir,als,ps); 
        }
        sleep(1);  /*100ms */
    }

    ret=close(fd);
    if(ret < 0){
		printf("file %s close failed!\r\n", argv[1]);
		return -1;
	}

    return 0;

}
