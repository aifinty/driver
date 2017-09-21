#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "led_cmd.h"

int main(int argc, char const *argv[])
{
	int iFd;
	int iRet;
	iFd = open("/dev/led",O_RDWR);
	if(iFd<0){
		perror("open error.\n");
		return -1;
	}
	iRet = ioctl(iFd,IOCTL_LED_ON,1);
	if(iRet){
		perror("ioctl error.\n");
		return -1;
	}
	sleep(2);
	iRet = ioctl(iFd,IOCTL_LED_ON,2);
	if(iRet){
		perror("ioctl error.\n");
		return -1;
	}
	sleep(2);
	iRet = ioctl(iFd,IOCTL_LED_ON,3);
	if(iRet){
		perror("ioctl error.\n");
		return -1;
	}
	sleep(2);

	iRet = ioctl(iFd,IOCTL_LED_OFF,1);
	if(iRet){
		perror("ioctl error.\n");
		return -1;
	}
	sleep(2);
	iRet = ioctl(iFd,IOCTL_LED_OFF,2);
	if(iRet){
		perror("ioctl error.\n");
		return -1;
	}
	sleep(2);
	iRet = ioctl(iFd,IOCTL_LED_OFF,3);
	if(iRet){
		perror("ioctl error.\n");
		return -1;
	}
	sleep(2);
	close(iFd);
	return 0;
}


