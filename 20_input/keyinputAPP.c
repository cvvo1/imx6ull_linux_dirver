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
#include <linux/input.h>

/* 定义一个input_event变量，存放输入事件信息 */
static struct input_event inputevent;

/*
 * @description		: main主程序
 * @param - argc 	: argv数组元素个数，应用程序参数个数，如使用 ls -l：argv=2，argv为字符串 
 * @param - argv[] 	: 具体参数
 * @return 			: 0 成功;其他 失败
 * 使用方法	 ：./keyinputAPP /dev/input/event1 	
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
        ret=read(fd,&inputevent,sizeof(inputevent));
        if(ret>0){   /* 读取数据成功 */
            switch (inputevent.type)
            {
            case EV_KEY:
                if(inputevent.code < BTN_MISC){   /* BTN_MISC键盘键值最大 */
                    printf("key %d %s\r\n", inputevent.code, inputevent.value ? "press" : "release");
                }else{
                    printf("button %d %s\r\n", inputevent.code, inputevent.value ? "press" : "release");
                }
                break;
            /* 其他类型的事件，自行处理 */
			case EV_REL:
				break;
			case EV_ABS:
				break;
			case EV_MSC:
				break;
			case EV_SW:
				break;
            default:
                break;
            }

        }else{
            printf("Failed to read data!\r\n");
            ret=close(fd);
            if(ret < 0){
		        printf("file %s close failed!\r\n", argv[1]);
		        return -1;
	        }
        }
    }
    return ret;
}
