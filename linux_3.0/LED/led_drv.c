#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/ioport.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/pci.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/gpio.h>


#define LED_ON 1
/*GPL2_0*/
static volatile unsigned long  *gpl2con;
static volatile unsigned char  *gpl2dat;
static int major; 
static struct class *led_class;

static int led_open(struct inode *inode, struct file *file)
{
	/* ÅäÖÃ¼Ä´æÆ÷ÎªÊä³ö */
	*gpl2con &=  0xfffffff0;
	*gpl2con |=  0x00000001;
	return 0;
}

static int led_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	int val;
	int ret;
	ret =  copy_from_user(&val, user_buf, count);
	if(ret < 0)
		return -1;
	if(val == 1)
		*gpl2dat |= 0x01;
	
	if(val == 0)
		*gpl2dat &= 0xfe;
	
	return 0;
}

static struct file_operations led_fops = {
	.owner = THIS_MODULE,		
	.open = led_open,
	.write = led_write,	
};

static __devinit int led_drv_probe(struct platform_device *pdev)
{

	struct resource *gpl2con_res;
	struct resource *gpl2dat_res;

      /**
 	* platform_get_resource - get a resource for a device
	* @dev: platform device
 	* @type: resource type
 	* @num: resource index
 	*/
	gpl2con_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	gpl2dat_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);	  
	
	
	gpl2con = ioremap(gpl2con_res->start, gpl2con_res->end - gpl2con_res->start + 1);
	gpl2dat  = ioremap(gpl2dat_res->start, gpl2dat_res->end - gpl2dat_res->start + 1);


	major = register_chrdev(0, "led", &led_fops);
	led_class = class_create(THIS_MODULE, "led");
	device_create(led_class, NULL, MKDEV(major, 0), NULL, "led");	
	
	return 0;
	
}

static __devexit int led_drv_remove(struct platform_device *pdev)
{
	iounmap(gpl2con);
	iounmap(gpl2dat);
	return 0;
}

static struct platform_driver led_drv = {
	.probe		= led_drv_probe,
	.remove		= __devexit_p(led_drv_remove),
	.driver		= {
		.name	= "led",
		.owner	= THIS_MODULE,
	},
};

static int led_init(void)
{
	return platform_driver_register(&led_drv);
}

static void led_exit(void)
{
	platform_driver_register(&led_drv);
}

module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");
