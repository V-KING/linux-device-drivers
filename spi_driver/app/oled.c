#include "oledfont.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
void OLEDPutChar(int fd, int page, int col, char c)
{
	const unsigned char *dots = oled_asc2_8x16[c - ' '];
	ioctl(fd, OLED_CMD_SET_POS, page | (col << 8));
	write(fd, dots[0], 8);
	ioctl(fd, OLED_CMD_SET_POS, page + 1 | (col << 8));
	write(fd, dots[8], 8);
}

void OLEDPrint(int fd, int page, int col, char *str)
{
	int i = 0;
	//char c;
	ioctl(fd, OLED_CMD_CLEAR_PAGE, page);
	ioctl(fd, OLED_CMD_CLEAR_PAGE, page+1);
	while(str[i])
	{
		OLEDPutChar(fd, page, col, str[i]);
		col += 8;
		if(col > 127)
		{
			col = 0;
			page += 2;
			ioctl(fd, OLED_CMD_CLEAR_PAGE, page);
			ioctl(fd, OLED_CMD_CLEAR_PAGE, page+1);
		}
		i++;
	}
}

void printf_usage(char *cmd)
{
	printf("Usage:\n");
    	printf("%s init\n", cmd);
    	printf("%s clear\n", cmd);
    	printf("%s clear <page>\n", cmd);
    	printf("%s <page> <col> <string>\n", cmd);
    	printf("eg:\n");
    	printf("%s 2 0 100ask.taobao.com\n", cmd);
    	printf("page is 0,1,...,7\n");
    	printf("col is 0,1,...,127\n");
}

int main(int argc , char **argv)
{
	int do_init, do_clear, do_show;
	int page = 0,col = 0;
	char *str = NULL ;
	int fd;
	if(argv == 2 && !strcmp(argv[1], "init"))
		do_init = 1;
	if(argv == 2 && !strcmp(argv[1], "clear"))
		do_clear = 1;
	if(argv == 3 && !strcmp(argv[1], "clear"))
	{
		do_clear = 1;
		page = argv[2];
	}

	if(argv ==4)
	{
		do_show = 1;
		page = argv[1];
		col = argv[2];
		str = (char *)argv[3];
	}

	if(!do_init && !do_clear && !do_show )
	{
		printf_usage(argv[0]);
		return -1;
	}

	fd = open("/dev/oled", O_RDWR);
	if(fd < 0)
		return -1;

	if(do_init)
		ioctl(fd, OLED_CMD_INIT);
	else if(do_clear)
	{
		if(page == 0)
			ioctl(fd, OLED_CMD_CLEAR);
		else
			ioctl(fd, OLED_CMD_CLEAR_PAGE, page);
	}
	else if(do_show)
	{
		OLEDPrint(fd, page, col, str);
	}

	return 0;
}
