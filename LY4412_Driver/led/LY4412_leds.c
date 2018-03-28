/* LED2 GPL2(0) 
  * LED3 GPK1(1)*/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <mach/regs-gpio.h>
#include <asm/io.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>


#define LED_NUM 2
#define DRIVER_NAME "led"
static int led_gpios[] = {
	EXYNOS4_GPL2(0),
	EXYNOS4_GPK1(1),
};
static int major;
static struct class * led_class;



static int led_open (struct inode *inode, struct file *file)
{
	printk(" led_open() ");
	return 0;
}

static int led_release (struct inode *inode, struct file *file)
{
	printk(" led_release() ");
	return 0;
}

static long led_ioctl (struct file *file, unsigned int cmd , unsigned long args) 
{
	printk(" led_ioctl() ");
	switch(cmd)
	{
		case 0:
		case 1:
			if(args >= LED_NUM) {
				return -EINVAL;
			}

			gpio_set_value(led_gpios[args], cmd);
			break;
		default:
			return -EINVAL;		
	}

	return 0;
}

static int led_close (struct inode *inode, struct file *file)
{
	return 0;
}

static struct file_operations  led_fops = {
	.owner = THIS_MODULE,
	.open = led_open,
	//.close = led_close,
	.release = led_release,
	.unlocked_ioctl = led_ioctl,
};

static int led_probe(struct platform_device *pdev)
{
	int i=0;
	int ret = 0;	
	/* 将两个GPIO 配置为输出 */
	for(i=0; i<LED_NUM; i++)
	{
		/* drivers/gpio/Gpiolib.c ->  int gpio_request(unsigned gpio, const char *label)函数*/
		/* 表示这个GPIO 是分配给LED 的，并且"LED"只是便签作用 */
		ret = gpio_request(led_gpios[i], "LED");
		if (ret) {
			printk("%s: request GPIO %d for LED failed, ret = %d\n", DRIVER_NAME,
					led_gpios[i], ret);
			return ret;
		}

		s3c_gpio_cfgpin(led_gpios[i], S3C_GPIO_OUTPUT);
		gpio_set_value(led_gpios[i], 1);
	}
	
	major = register_chrdev(0, "led", &led_fops);
	led_class = class_create(THIS_MODULE, "led");
	device_create(led_class, NULL, MKDEV(major, 0) , NULL, "led");	

	return 0;
}

static int led_remove (struct platform_device *pdev)
{
	int i=0;
	device_destroy(led_class, MKDEV(major, 0));
	class_destroy(led_class);
	unregister_chrdev(major, "led");
	for(i=0; i< LED_NUM; i++)
	{
		gpio_free(led_gpios[i]);
	}

	return 0;
}

static struct platform_driver led_driver = {
	.probe = led_probe,
	.remove = led_remove,	
	.driver = {
		.name = "led",
		.owner = THIS_MODULE,
	}
};

static struct platform_device led_device = {
        .name   = "led",
        .id             = -1,
};

static int LY4412_LEDinit(void)
{
	platform_driver_register(&led_driver);
	platform_device_register(&led_device);
	return 0;
}
static void LY4412_LEDexit(void)
{
	platform_driver_unregister(&led_driver);
	platform_device_unregister(&led_device);
}

module_init(LY4412_LEDinit);
module_exit(LY4412_LEDexit);
