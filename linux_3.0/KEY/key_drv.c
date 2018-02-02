#include <mach/irqs-exynos5.h>	/* ÖÐ¶ÏºÅ */
#include <mach/exynos-ion.h>
#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/irq.h>

#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
/* GPX1_2  back 	XEINT_10		SPI26 ID58 EINT[10] 	External Interrupt
  * GPX3_3  sleep 	XEINT_27		SPI32 ID64 EINT16_31 	External Interrupt
  * GPX2_1  VOL+ 	XEINT_17		SPI32 ID64 EINT16_31 	External Interrupt
  * GPX2_0  VOL- 	XEINT_16		SPI32 ID64 EINT16_31 	External Interrupt
  */

static struct key_desc {
	int irq;
	char *name;
	unsigned int pin;
	unsigned int key_val;
};

static struct input_dev *key_dev;
static struct key_desc *irq_meg;

static struct key_desc key_desc[4] = {
	{IRQ_SPI(26), "L", EXYNOS4_GPX1(2), KEY_L},
	{IRQ_SPI(32), "S", EXYNOS4_GPX2(0), KEY_S},
	{IRQ_SPI(32), "enter", EXYNOS4_GPX2(1) ,KEY_ENTER},
	{IRQ_SPI(32), "shift", EXYNOS4_GPX3(3),KEY_LEFTSHIFT},
};


static irqreturn_t keyirq_handle(int irq, void *dev_id)
{
	input_event(key_dev, EV_KEY, pindesc->key_val, 1);
	input_sync(key_dev);
	return IRQ_RETVAL(IRQ_HANDLED);
}

static int key_init(void)
{

	key_dev = input_allocate_device();

	set_bit(EV_KEY, key_dev->evbit);
	set_bit(EV_REP, key_dev->evbit);

	set_bit(KEY_L,key_dev->keybit);
	set_bit(KEY_S,key_dev->keybit);
	set_bit(KEY_ENTER, key_dev->keybit);
	set_bit(KEY_LEFTSHIFT, key_dev->keybit);

	input_register_device(key_dev);

	for(i=0; i<4; i++)
	{
		request_irq(key_desc[i].irq , keyirq_handle , IRQF_SHARED, key_desc[i].name , &key_desc[i])
	}
	return 0;
}

static void key_exit(void)
{
	int i;
	for(i=0; i < 4; i++)
	{
		free_irq(key_desc[i].irq , &key_desc[i]);
	}
	input_unregister_device(key_dev);
	input_free_device(key_dev);
}

module_init(key_init);
module_exit(key_exit);
MODULE_LICENSE("GPL");
