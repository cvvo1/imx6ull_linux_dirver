#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define LEDOFF 	0				/* 关灯，定义为字符串 */
#define LEDON 	1				/* 开灯 */

/* 字符设备应用开发 */
/*
 * @description		: main主程序
 * @param - argc 	: argv数组元素个数，应用程序参数个数，如使用 ls -l：argv=2，argv为字符串 
 * @param - argv[] 	: 具体参数
 * @return 			: 0 成功;其他 失败
 * 使用方法	 ：./ledtest /dev/led  0 关闭LED
		     ./ledtest /dev/led  1 打开LED	
  			 argv[2] 1:读文件
  			 argv[2] 2:写文件	
 */

int main(int argc, char *argv[])
{
    int fd,ret=0;
    char *filename;
    unsigned char databuf[1];

    if(argc!= 3){
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

    databuf[0]=atoi(argv[2]);   /* 要执行的操作：打开或关闭，将字符形式转换为数字格式 */

    ret=write(fd,databuf,sizeof(databuf));
    if(ret<0){
        printf("LED Control Failed!\r\n");
        close(fd);
        return -1;
    }

    ret=close(fd);
    if(ret < 0){
		printf("file %s close failed!\r\n", argv[1]);
		return -1;
	}

    return 0;

}
