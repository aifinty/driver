#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/unistd.h>
#include <fcntl.h>
#include <string.h>

int main(int argc, char const *argv[])
{
	int iFd;
	int iRet;
	char buf[10]={0};
	iFd = open("/dev/key",O_RDWR);
	if(iFd<0){
		perror("fail to open /dev/key.");
		return -1;
	}
	while(1)
	{
		iRet = read(iFd,buf,sizeof(buf));
		if(iRet<0){
			perror("read error.\n");
			close(iFd);
			return -1;
		}
		printf("key_val:%s\n",buf);
		memset(buf,0,sizeof(buf));
	}
	return 0;
}


