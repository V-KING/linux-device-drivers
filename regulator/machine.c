#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/mfd/core.h>
#include <linux/regulator/machine.h>

#if 0
 regulator_consumer_supply:
 const char *dev_name;	 /* consumer的名字 */
 const char *supply;     /* consumer的电源引脚名称 */
#endif

static struct regulator_consumer_supply myregulator_supplies[] = {
	REGULATOR_SUPPLY("VCC", "mylcd"),
						/* 引脚名称 , consumer name */
};

static struct regulator_init_data myregulator_init_data = {
	.constraints = {
		.min_uV 		= 12000000,
		.max_uV    	= 12000000,
		.vaild_modes_mask = REGULATOR_MODE_NORMAL,
		.vaild_ops_mask 	 = REGULATOR_CHANGE_STATUS,
	},

	.num_consumer_supplies = 1,
	.consumer_supplies = myregulator_supplies,
};

static void myregulator_release(struct device *dev)
{
}

static struct platform_device myregulator_dev = {
	.name = " myregulator ",
	id = -1,
	.dev = {
		.release = myregulator_release,
		.platform_data = &myregulator_init_data,	
	},
};


static int regulator_machine_init(void)
{
	platform_device_register(&myregulator_dev);
	return 0;
}

static void regulator_machine_exit(void)
{
	platform_device_unregister(&myregulator_dev);
}

module_init(regulator_machine_init);
module_exit(regulator_machine_exit);

MODULE_LICENSE("GPL");


