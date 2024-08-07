#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/* 定义按键值 */
#define KEY0VALUE 	0xF0				/* 按键值 */
#define INVAKEY 	0x00				/* 无效按键值 */

/* 字符设备应用开发 */
/*
 * @description		: main主程序
 * @param - argc 	: argv数组元素个数，应用程序参数个数，如使用 ls -l：argv=2，argv为字符串 
 * @param - argv[] 	: 具体参数
 * @return 			: 0 成功;其他 失败
 * 使用方法	 ：./keyAPP /dev/key  
 */

int main(int argc, char *argv[])
{
    int fd,ret=0;
    char *filename;
    unsigned char keyvalue;

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

    /* 循环读取按键值 */
    while(1){
        ret=read(fd,&keyvalue,sizeof(keyvalue));
        if(keyvalue==KEY0VALUE){
            printf("KEY0 Press,value=%#X\r\n",keyvalue);
        }
    }

    ret=close(fd);
    if(ret < 0){
		printf("file %s close failed!\r\n", argv[1]);
		return -1;
	}

    return 0;

}
