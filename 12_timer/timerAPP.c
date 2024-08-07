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
 * 使用方法	 ：./timerAPP /dev/timer
  			 argv[2] 1:读文件
  			 argv[2] 2:写文件	
 */

int main(int argc, char *argv[])
{
    int fd,ret=0,inputret=0;
    char *filename;
    unsigned char databuf[1];
    unsigned int cmd,cmdvalue;
    unsigned long arg;
    unsigned char str[100];

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
        printf("Please Input CMD:");
        inputret=scanf("%d",&cmdvalue);
        if (inputret != 1) {				/* 参数输入错误 */
			gets(str);				/* 防止卡死 ? */ 
		}

        if(cmdvalue==1){      /* 关闭LED灯 */
            cmd=CLOSE_CMD;  
        }else if(cmdvalue==2){  /* 打开LED灯 */
            cmd=OPEN_CMD;
        }else if(cmdvalue==3){ /* 设置周期值 */
            cmd=SETPERIOD_CMD;
            printf("Please Input Period(ms):");
            inputret=scanf("%d",&arg);
            if (inputret != 1){				/* 参数输入错误 */
			    gets(str);				/* 防止卡死 */
		    }
        }
        ioctl(fd,cmd,arg);
    }

    /* int ioctl(int fd, unsigned long request, ...); */


    ret=close(fd);
    if(ret < 0){
		printf("file %s close failed!\r\n", argv[1]);
		return -1;
	}

    return 0;

}
