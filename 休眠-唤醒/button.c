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

struct pin_irq {
	int irq;
	char *name;
	unsigned int pin;
	unsigned int key_value;
};


struct pin_irq pin_irqs[4] = {
		{IRQ_EINT0,   "S2", S3C2410_GPF(0),   KEY_L},
		{IRQ_EINT2,   "S3", S3C2410_GPF(2),   KEY_S},
		{IRQ_EINT11, "S4", S3C2410_GPG(3),   KEY_ENTER},
		{IRQ_EINT19, "S5", S3C2410_GPG(11), KEY_LEFTSHIFT},
};


static struct input_dev *button_dev;
static struct timer_list button_timer;
static struct pin_irq *irq;

static irqreturn_t button_handle(int irq, void *dev_id)
{
	irq = (struct pin_irqs *)dev_id;
	mod_timer(&button_timer, jiffies + HZ / 100);
	return IRQ_RETVAL(IRQ_HANDLED);
}

static void button_timer_function(unsigned long data)
{
	struct pin_irqs *pindesc = irq;
	unsigned int pinval;
	/* ���������ж� */
	if(irq == NULL)
		return;
	
	pinval = s3c2410_gpio_getpin(pindesc->pin);
	/* ��ȡgpio�ĸߵ͵�ƽ */
	if(pinval)
	{
		/* input������ϵͳ�ϱ��¼� */
		input_event(button_dev, EV_KEY, pindesc->key_value, 0);
		input_sync(button_dev);
	}

	else
	{
		/* ���� */
		input_event(button_dev, EV_KEY, pindesc->key_value, 1);
		input_sync(button_dev);
	}
}

static int button_init(void)
{

	button_dev =  input_allocate_device();
	/* �����ظ����¼� */
	set_bit(EV_KEY, button_dev->evbit);
	set_bit(EV_REP, button_dev->evbit);
	/* �������¼��İ���ֵ */
	set_bit(KEY_L, button_dev->keybit);
	set_bit(KEY_S, button_dev->keybit);
	set_bit(KEY_ENTER, button_dev->keybit);
	set_bit(KEY_LEFTSHIFT, button_dev->keybit);

	input_register_device(button_dev);

	/* ���ö�ʱ�� */
	init_timer(&button_timer);
	button_timer.function = button_timer_function;
	add_timer(&button_timer);

	/* �����ж� */
	//request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags, const char * name, void * dev)
	for(i=0, i< 4; i++)
	{
		request_irq(pin_irqs[i].irq, button_handle, (IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING), \
					pin_irqs[i].name, &pin_irq[i]);	
	}
	/* ָ������Ϊ����Դ */
	/* echo mem  > /sys/power/state  ���� */
	/* echo disk > /sys/power/state  ����*/
	/* ָ����Щ�жϿ������ڻ���ϵͳ */ 
	/* ������Щ�����ỽ��ϵͳ�� ���廽�ѵ���ʲô�豸����Ҫ�����豸����������
	 * ����֧�� */
	irq_set_irq_wake(IRQ_EINT0  , 1);
	irq_set_irq_wake(IRQ_EINT2  , 1);
	irq_set_irq_wake(IRQ_EINT11, 1);
	
	return 0;
}

static void button_exit(void)
{
	int i;
	for(i=0; i< 4; i++)
	{
		free_irq(pin_irqs[i].irq, &pin_irq[i]);
	}

	del_timer(&button_timer);
	input_register_device(button_dev);
	input_free_device(button_dev);

	irq_set_irq_wake(IRQ_EINT0  , 0);
	irq_set_irq_wake(IRQ_EINT2  , 0);
	irq_set_irq_wake(IRQ_EINT11, 0);
}

module_init(button_init);
module_exit(button_exit);

MODULE_LICENSE("GPL");
