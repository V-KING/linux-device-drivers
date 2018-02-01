#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

int main(int argc, char **argv)
{
	int val;
	int fd;
	if(argc != 2)
	{
		printf(" Usage: ./led_app on/off \n");
		return -1;
	}

	if(strcmp(argv[1], "on") == 0)
	{
		val = 1;
	}

	if(strcmp(argv[1], "off") == 0)
	{
		val = 0;
	}

	fd = open("/dev/led", O_RDWR);
	write(fd, val, 4);

	return 0;
}
