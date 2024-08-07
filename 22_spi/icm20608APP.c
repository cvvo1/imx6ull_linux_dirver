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
 * 使用方法	 ：./icm20608App /dev/icm20608	
 */

int main(int argc, char *argv[])
{
    int fd,ret=0;
    char *filename;
    signed int databuf[7];

	signed int accel_x_adc, accel_y_adc, accel_z_adc;
	signed int temp_adc;
    signed int gyro_x_adc, gyro_y_adc, gyro_z_adc;

	float accel_x_act, accel_y_act, accel_z_act;
	float temp_act;
    float gyro_x_act, gyro_y_act, gyro_z_act;

    if(argc!= 2){
		printf("Error Usage!\r\n");
		return -1;
	}

    filename=argv[1];

    /* 打开led驱动 */
    fd=open(filename,O_RDWR);
    if(fd<0){
        printf("file %s open failed!\r\n", argv[1]);
		return -1;
    }

    while(1){
        ret=read(fd,databuf,sizeof(databuf));
        if(ret<0){
            /* 错误处理 */
        }else{
			accel_x_adc = databuf[0];
			accel_y_adc = databuf[1];
			accel_z_adc = databuf[2];
			temp_adc = databuf[3];
            gyro_x_adc = databuf[4];
			gyro_y_adc = databuf[5];
			gyro_z_adc = databuf[6];

			/* 计算实际值 */
			accel_x_act = (float)(accel_x_adc) / (65535/(2.0*2));   /* 加速度计量程设置±2g*/ 
			accel_y_act = (float)(accel_y_adc) / (65535/(2.0*2));
			accel_z_act = (float)(accel_z_adc) / (65535/(2.0*2));
			temp_act = ((float)(temp_adc) - 25 ) / 326.8 + 25;
            gyro_x_act = (float)(gyro_x_adc)  / (65535/(250.0*2));    /*陀螺仪量程为±250dps*/
			gyro_y_act = (float)(gyro_y_adc)  / (65535/(250.0*2));
			gyro_z_act = (float)(gyro_z_adc)  / (65535/(250.0*2));



            printf("\r\n原始值:\r\n");
			printf("gx = %d, gy = %d, gz = %d\r\n", gyro_x_adc, gyro_y_adc, gyro_z_adc);
			printf("ax = %d, ay = %d, az = %d\r\n", accel_x_adc, accel_y_adc, accel_z_adc);
			printf("temp = %d\r\n", temp_adc);
			printf("实际值:");
			printf("act gx = %.2f°/S, act gy = %.2f°/S, act gz = %.2f°/S\r\n", gyro_x_act, gyro_y_act, gyro_z_act);
			printf("act ax = %.2fg, act ay = %.2fg, act az = %.2fg\r\n", accel_x_act, accel_y_act, accel_z_act);
			printf("act temp = %.2f°C\r\n", temp_act);
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
