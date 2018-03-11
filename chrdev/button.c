#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>
#include <linux/poll.h>


static volatile unsigned long * gpfcon;
static volatile unsigned long * gpfdat;

static struct fasync_struct * button_async;

static DECLARE_WAIT_QUEUE_HEAD(button_waitq);

struct pin_desc{
	unsigned int pin;
	unsigned int key_val;
};


/* 键值: 按下时, 0x01, 0x02, 0x03, 0x04 */
/* 键值: 松开时, 0x81, 0x82, 0x83, 0x84 */
static unsigned char key_val;
static volatile int ev_press = 0;
struct pin_desc pin_desc = {
	.pin = S3C2410_GPF0, 
	.key_val = 0x01,
};

static atomic_t canopen = ATOMIC_INIT(1);
static DECLARE_MUTEX(button_lock);/* 信号量初始化为1 = 互斥量 */
static struct pin_desc *irq_pd;
static struct timer_list button_timer;
static int major; 
static struct class *button_class;

static irqreturn_t irq_function(int irq, void *dev_id)
{
	irq_pd = (struct pin_desc *)dev_id;//记录发生中断时的引脚描述
	mod_timer(&button_timer, jiffies+HZ/100);//设置超时时间10ms jiffies为当前系统时间
	return IRQ_RETVAL(IRQ_HANDLED);
}

/* 引入原子变量或信号量进行互斥访问 */
static int button_open (struct inode *inode, struct file *file)
{
#if 0
/* 该函数对原子类型的变量v原子地增加1，并判断结果是否为0，如果为0，返回真，否则返回假。  */
	if(!atomic_dec_and_test(&canopen))
	{
		atomic_inc(&canopen);
		return -EBUSY;
	}
#endif	

	if(file->f_flags & O_NONBLOCK)
	{	
		/* 成功得到信号量则返回0 */
		if(down_trylock(&button_lock))
			return -EBUSY;
	}
	else
	{
		down(&button_lock);
	}

	request_irq(IRQ_EINT0, irq_function, IRQF_SHARED, "S1" , &pin_desc)
	
	return 0;
}

ssize_t button_read (struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	if(size != 1)
		return -EINVAL;
	if(file->f_flags & O_NONBLOCK)
	{
		if(!ev_press)
			return -EAGAIN;
	}

	else
	{
		/* 没有按键动作就会休眠在这里 */
		wait_event_interruptible(button_waitq, ev_press);
	}

	copy_to_user(buf, &key_val, sizeof(key_val));
	ev_press = 0;
}

static int button_close(struct inode *inode, struct file *file)
{
	free_irq(IRQ_EINT0, &pin_desc);
	//atomic_inc(&canopen);
	up(&button_lock);
}

static unsigned int  button_poll(struct file *file, struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	poll_wait(file, &button_wait, wait);/* 将进程file 挂入到button_wait中 */

	if(ev_press)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static int button_fasync (int fd, struct file *file, int on)
{
	return fasync_helper(fd, file, on,&button_async);
}

static struct file_operations  button_fops  = {
	.owner = THIS_MODULE,
	.open = button_open,
	.read = button_read,
	.close = button_close,
	.poll = button_poll,
	.fasync = button_fasync,
};

static void button_timer_function(unsigned long data)
{
	struct pin_desc * pindesc = irq_pd;
	unsigned int pinval;

	if (!pindesc)
		return;
	pinval = s3c2410_gpio_getpin(pindesc->pin);
	if (pinval)
	{
		/* 松开 */
		key_val = 0x80 | pindesc->key_val;
	}
	else
	{
		/* 按下 */
		key_val = pindesc->key_val;
	}

    	ev_press = 1;                  /* 表示中断发生了 */

	wake_up_interruptible(&button_waitq);
	//kill_fasync (&button_async, SIGIO, POLL_IN);
}

static int button_init(void)
{
	init_timer(&button_timer);
	button_timer.function = button_timer_function;
	add_timer(&button_timer);

	major = register_chrdev(0, "button", &button_fops);
	button_class = class_create(THIS_MODULE, "button");
	device_create(button_class, NULL , MKDEV(major, 0), NULL, "button");

	gpfcon = (unsigned long *)ioremap(0X56000050, 16);
	gpfdat = gpfcon + 1;
	
	return 0;
}

static void button_exit(void)
{
	unregister_chrdev(major, "button");
	device_destroy(button_class, MKDEV(major, 0));
	class_destroy(button_class);
	del_timer(&button_timer);
	iounmap(gpfcon);
	iounmap(gpfdat);
}

module_init(button_init);
module_exit(button_exit);
MODULE_LICENSE("GPL");
