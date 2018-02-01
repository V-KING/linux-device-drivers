#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>


static struct resource led_resource[] = {
		[0] = {
			.start  = 0x11000100,
			.end   = 0x11000100 + 4 - 1,  /* gpl2con */
			.flags   = IORESOURCE_MEM,
		},

		[1] = {
			.start  = 0x11000104,
			.end   = 0x11000104,		/* gpl2dat */
			.flags   = IORESOURCE_MEM,
		},
};

static void led_release(struct device *pdev)
{
}

static struct platform_device led_dev = {
	.name = "led",
	.id = -1,
	.num_resources = ARRAY_SIZE(led_resource),
	.resource = led_resource,
	.dev = {
		.release = led_release,
	},
};

static int led_dev_init(void)
{
	return platform_device_register(&led_dev);
}

static void led_dev_exit(void)
{
	platform_device_unregister(&led_dev);
}

module_init(led_dev_init);
module_exit(led_dev_exit);
MODULE_LICENSE("GPL");
