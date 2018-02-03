#include <stdio.h>
#include <linux/type.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c_dev.h>

static struct i2c_msg {
	unsigned short addr;	/* slave address			*/
	unsigned short flags;
#define I2C_M_TEN		0x0010	/* this is a ten bit chip address */
#define I2C_M_RD		0x0001	/* read data, from slave to master */
#define I2C_M_NOSTART		0x4000	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_REV_DIR_ADDR	0x2000	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_IGNORE_NAK	0x1000	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_NO_RD_ACK		0x0800	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_RECV_LEN		0x0400	/* length will be first received byte */
	unsigned short len;		/* msg length				*/
	unsigned char *buf;		/* pointer to msg data			*/
};

static struct i2c_rdwr_ioctl_data {
	struct i2c_msg __user *msgs;	/* pointers to i2c_msgs */
	unsigned int nmsgs;			/* number of i2c_msgs */
};

int main(int argc ,char **argv)
{
	struct i2c_rdwr_ioctl_data eeprom_data;
	unsigned int fb;
	unsigned int slave_addr, word_addr, value;
	unsigned char buffer[2];
	int msg_num;
	int ret;
	unsigned char buffer[2];
	if(argc < 5)
	{
		printf(" Usage: %s /dev/i2c-x device_addr word_addr value\n ",argv[0]);
		return -1;
	}

	fd = open(argv[1], O_RDWR);
	sscanf(argv[2], "%x", &slave_addr);
	sscanf(argv[3], "%x", &word_addr);
	sscanf(argv[4], "%x", &value);
	msg_num = 2;
	eeprom_data.msgs = (struct i2c_msg*)malloc(msg_num * sizeof(struct i2c_msg));

	eeprom_data.nmsgs = 1;
	eeprom_data.msgs[0].len = 2;
	eeprom_data.msgs[0].addr = slave_addr;	/* IIC 设备地址 */
	eeprom_data.msgs[0].flags = 0;
	eeprom_data.msgs[0].buf = buffer;
	eeprom_data.msgs[0].buf[0] = word_addr;	/* 数据地址 */
	eeprom_data.msgs[0].buf[1] = value; /* 写入的数值 */

	ioctl(fb, I2C_RDWR , (unsigned long)&eeprom_data);
	printf(" write %d at the address of %d \n",value, word_addr);

	/* 这个操作是伪写操作发送数据地址 */
	eeprom_data.nmsgs = 2;
	eeprom_data.msgs[0].len = 1;
	eeprom_data.msgs[0].addr = slave_addr;	/* IIC 设备地址 */
	eeprom_data.msgs[0].flags = 0;
	eeprom_data.msgs[0].buf[0] = word_addr;	/* 数据地址 */

	eeprom_data.msgs[1].len = 1;
	eeprom_data.msgs[1].addr = slave_addr;	/* IIC 设备地址 */
	eeprom_data.msgs[1].flags = I2C_RD;
	eeprom_data.msgs[1].buf = buffer;
	eeprom_data.msgs[1].buf[0] = 0;	/* 读取的数据 */

	ioctl(fb, I2C_RDWR, (unsigned long)&eeprom_data);
	printf(" read %d from %d \n ",eeprom_data.msgs[1].buf[0], word_addr);
	close(fd);

	return 0;
}
