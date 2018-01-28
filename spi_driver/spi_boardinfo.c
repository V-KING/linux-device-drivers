#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <mach/regs-gpio.h>
#include <plat/gpio-cfg.h>
/* 编译进内核/driver/spi/makefile 添加这一项 */
static struct  spi_board_info spi_board_info_jz2440[] = {
	{
		.modalias = "oled",
		.max_speed_hz = 10000000,
		.bus_num = 1,		/* spi control 1*/
		.mode = SPI_MODE_0,
		.chip_select = S3C2410_GPF(1),
		.platform_data = (const void *)S3C2410_GPG(4) ,
	},
	
	{
		.modalias = "spi_flash",
		.max_speed_hz = 80000000,
		.bus_num = 1,		/* spi control 1*/
		.mode = SPI_MODE_0,
		.chip_select = S3C2410_GPG(2),
	},
};

static int spi_boardinfo_init(void)
{
	return spi_register_board_info(spi_board_info_jz2440, ARRAY_SIZE(spi_board_info_jz2440));
	
}

static void spi_boardinfo_exit(void)
{	
}

module_init(spi_boardinfo_init);
module_exit(spi_boardinfo_exit);
MODULE_LICENSE("GPL");

