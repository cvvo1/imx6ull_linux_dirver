#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/* 字符设备应用开发 */
/*
 * @description		: main主程序
 * @param - argc 	: argv数组元素个数，应用程序参数个数，如使用 ls -l：argv=2，argv为字符串 
 * @param - argv[] 	: 具体参数
 * @return 			: 0 成功;其他 失败
 * 使用方法 ./chrdevbaseAPP /dev/chrdevbase <1>|<2>
  			 argv[2] 1:读文件
  			 argv[2] 2:写文件	
 */

static char usrdata[] = {"usr data!"};

int main(int argc,char *argv[])  /* 主函数传参 */
{   
    int fd=0,retvalue=0;
    char *filename;
    char readbuf[100],writebuf[100];

    if(argc != 3){
        printf("Error usage!\r\n");
        return -1;
    }

    filename = argv[1];

    /* 文件打开 */
    fd=open(filename,O_RDWR);   /* 函数原型为int open(const char *pathname, int flags)，pathname：要打开的设备或者文件名，flags：文件打开模式，以下三种模式必选其一
                                O_RDONLY：只读模式、O_WRONLY：只写模式、O_RDWR：读写模式。返回文件描述符(类似文件ID),错误返回-1 */
    if(fd<0)
    {
        printf("Can't open file %s\r\n",filename);
        return -1;
    }

    if (atoi(argv[2])==1){    /* 输入为字符串，通过atoi函数，将string 转化为 int */
    /* 从驱动文件读取数据 */
        retvalue=read(fd,readbuf,100);  /* 函数原型为ssize_t read(int fd, void *buf, size_t count)，fd:open函数打开文件成功后的文件描述符
            buf:数据读取到此buf中，count:要读取的数据长度，也就是字节数。返回读取的字节数,返回 0 表示读取到了文件末尾；如果返回负值，表示读取失败 */
        if(retvalue < 0){
		    printf("read file %s failed!\r\n", filename);
	    }else{
			/* 读取成功，打印出读取成功的数据 */
		    printf("usr read data:%s\r\n",readbuf);
	    }
    }

    if (atoi(argv[2])==2){
    /* 向设备驱动写数据 */
        memcpy(writebuf,usrdata,sizeof(usrdata));
        retvalue=write(fd,writebuf,100); /*函数原型为ssize_t write(int fd, const void *buf, size_t count),fd:open函数打开文件成功后的文件描述符
                        buf:要写入的数据，count:要写入的数据长度，也就是字节数，返回写入的字节数，返回 0 表示没有写入任何数据；如果返回负值，表示写入失败*/
        if(retvalue < 0){
		    printf("write file %s failed!\r\n", filename);
	    }
    }
    
    /* 关闭设备 */
    retvalue=close(fd);     /*函数原型为int close(int fd)，fd：要关闭的文件描述符，返回值： 0 表示关闭成功，负值表示关闭失败*/
    if(retvalue){
        printf("close file %s failed!\r\n",filename);
        return -1;
    }

    return 0;
}
