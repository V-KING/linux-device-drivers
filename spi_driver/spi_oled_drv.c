#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <sound/core.h>
#include <linux/spi/spi.h>
#include <asm/uaccess.h>
#include <mach/hardware.h>
#include <mach/regs-gpio.h>
#include <linux/gpio.h>
#include <plat/gpio-cfg.h>

static int major;
static struct class *oled_class;
static int pin_s3c2440_spi_dc;


//register_chrdev(unsigned int major, const char * name, const struct file_operations * fops)

static struct spi_device *spi_dev;
static char *ker_buf;

static void OLED_Set_DC(char val)
{
	s3c2410_gpio_setpin(pin_s3c2440_spi_dc, val);
}

static ssize_t oled_write(struct file *file, const char __user *userbuf,
		     				size_t bytes, loff_t *off)
{
	int ret;
	if(bytes > 4096)
		return -1;
	ret = copy_from_user(ker_buf, userbuf, bytes);
	if(ret < 0)
		return -1;

	/* DC 引脚选为数据 */
	OLED_Set_DC(1); /* data */
	spi_write(spi_dev, ker_buf , bytes);
	return 0;
}

#define OLED_CMD_INIT 			0x100001
#define OLED_CMD_CLEAR  		0x100002 
#define OLED_CMD_CLEAR_PAGE 	0x100003
#define OLED_CMD_SET_POS    		0x100004

static void OLED_Set_DC(char val)
{    
	s3c2410_gpio_setpin(pin_s3c2440_spi_dc, val);
}

static void OLEDWriteCmd(unsigned char cmd)
{    
	OLED_Set_DC(0); /* command */    
	spi_write(spi_dev, &cmd, 1);    
	OLED_Set_DC(1); /*  */
}

static void OLEDWriteDat(unsigned char dat)
{    
	OLED_Set_DC(char val)(1); /* data */    
	spi_write(spi_dev, &dat, 1);    
	OLED_Set_DC(1); /*  */
}

static void OLEDSetPageAddrMode(void)
{    
	OLEDWriteCmd(unsigned char val)(0x20);    
	OLEDWriteCmd(0x02);
}

static void OLEDSetPos(int page, int col)
{    
	OLEDWriteCmd(0xB0 + page); /* page address */    
	OLEDWriteCmd(col & 0xf);   /* Lower Column Start Address */    
	OLEDWriteCmd(0x10 + (col >> 4));   /* Lower Higher Start Address */
}

static void OLEDClear(void)
{
	int page, i;    
	for (page = 0; page < 8; page ++)    
	{
		OLEDSetPos(page, 0);       
		for (i = 0; i < 128; i++)            
			OLEDWriteDat(0);    
	}
}

void OLEDClearPage(int page)
{    	int i;    
	OLEDSetPos(page, 0);   
	for (i = 0; i < 128; i++)        
		OLEDWriteDat(0);    
}

static int OLEDInit(void)
{
	/* 向OLED发命令以初始化 */    
	OLEDWriteCmd(0xAE); /*display off*/     
	OLEDWriteCmd(0x00); /*set lower column address*/     
	OLEDWriteCmd(0x10); /*set higher column address*/     
	OLEDWriteCmd(0x40); /*set display start line*/     
	OLEDWriteCmd(0xB0); /*set page address*/     
	OLEDWriteCmd(0x81); /*contract control*/     
	OLEDWriteCmd(0x66); /*128*/     
	OLEDWriteCmd(0xA1); /*set segment remap*/     
	OLEDWriteCmd(0xA6); /*normal / reverse*/     
	OLEDWriteCmd(0xA8); /*multiplex ratio*/     
	OLEDWriteCmd(0x3F); /*duty = 1/64*/     
	OLEDWriteCmd(0xC8); /*Com scan direction*/     
	OLEDWriteCmd(0xD3); /*set display offset*/     
	OLEDWriteCmd(0x00);     
	OLEDWriteCmd(0xD5); /*set osc division*/     
	OLEDWriteCmd(0x80);     
	OLEDWriteCmd(0xD9); /*set pre-charge period*/     
	OLEDWriteCmd(0x1f);     
	OLEDWriteCmd(0xDA); /*set COM pins*/     
	OLEDWriteCmd(0x12);     
	OLEDWriteCmd(0xdb); /*set vcomh*/     
	OLEDWriteCmd(0x30);     
	OLEDWriteCmd(0x8d); /*set charge pump enable*/     
	OLEDWriteCmd(0x14);   
	
	OLEDSetPageAddrMode();   
	
	OLEDClear();        
	
	OLEDWriteCmd(0xAF); /*display ON*/   
}

static long oled_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int page;
	int col;
	switch(cmd)
	{
		case OLED_CMD_INIT:
		{
			OLEDInit();
			break;
		}

		case OLED_CMD_CLEAR
		{
			OLEDClear();
			break;
		}

		case OLED_CMD_CLEAR_PAGE:
		{
			OLEDClearPage(page);
			break;
		}

		case OLED_CMD_SET_POS:
		{
			page = arg & 0xff;
			col = (arg >> 8) & 0xff;
			OLEDSetPos(page,col);
			break;
		}
	}

	return 0;
}

static struct file_operations spi_oled_fops = {
	.owner 			= THIS_MODULE,
	.write 			= oled_write,
	.unlocked_ioctl 	= oled_ioctl,
};

static int __devinit spi_oled_probe(struct spi_device *spi)
{
	spi_dev = spi;
	pin_s3c2440_spi_dc = spi->dev.platform_data;
	s3c2410_gpio_cfgpin(pin_s3c2440_spi_dc, S3C2410_GPIO_OUTPUT);
	s3c2410_gpio_cfgpin(spi->chip_select, S3C2410_GPIO_OUTPUT);

	ker_buf = kmalloc(4096, GFP_KERNEL);

	major = register_chrdev(0, "oled", &spi_oled_fops);
	oled_class = class_create(THIS_MODULE, "oled");
	device_create(oled_class, NULL, MKDEV(major, 0), NULL,"oled"); /* /dev/oled */
	return 0;
}

static int __devexit spi_oled_remove(struct spi_device *pdev)
{
	device_destroy(oled_class, MKDEV(major, 0));
	class_destroy(oled_class);
	unregister_chrdev(major, "oled");	

	kfree(ker_buf);
	return 0;
}

static struct spi_driver spi_oled_driver = {
	.driver = {
		.name = "oled",
		.owner = THIS_MODULE,	
	},
	.probe 	= spi_oled_probe,
	.remove 	= spi_oled_remove, 
}; 

static int spi_oled_init(void)
{
	spi_register_driver(&spi_oled_driver);
	return 0;
}

static void spi_oled_exit()
{

}

module_init(spi_oled_init);
module_exit(spi_oled_exit);
MODULE_LICENSE("GPL");
