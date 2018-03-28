#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char **argv)
{
	int fd;

	fd = open("/dev/led", O_RDWR);
	/* cmd which */
	ioctl(fd, 1, 0);
	ioctl(fd, 1, 1);
	ioctl(fd, 0, 0);
	ioctl(fd, 0, 1);
	close(fd);
	
}



