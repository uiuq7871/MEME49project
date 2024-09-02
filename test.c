#include <stdio.h>
#include <stdlib.h>	//system
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#define DEVICE_BLTEST "/dev/dht11"
int main(void)
{
	int temperaturefd;
	int ret = 0;
	char buf[5];
	unsigned char  tempz = 0;
	unsigned char  tempx = 0;
	
	temperaturefd = open(DEVICE_BLTEST, O_RDONLY);
	if(temperaturefd < 0)
	{
		perror("can not open device");
		exit(1);
	}
	while(1)
	{
		ret = read(temperaturefd,buf,sizeof(buf));
    	if(ret < 0)
    	{
    		printf("read err!\n");
		}
        else
		{
			tempz		=buf[2];
			tempx		=buf[3];
			printf("temperature = %d.%d\n", tempz, tempx);	
        }
		sleep(2);
	}
	
	if temperaturefd >= 0)	
	{
		close(temperaturefd);
	}
	
	return 0;
}
