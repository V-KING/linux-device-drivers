


#define DEVICE_NAME "top_button"

static DECLARE_WAIT_QUEUE_HEAD(button_waitq);
//wait_queue_head_t *target_wait;
static volatile int ev_press=0;
static int key_value;
/* home back sleep vol- vol+ */
/* UART_RING  SIM_DET   GYRO_INT  KP_ROW1 KP_ROW0*/
/* GPX1_1	GPX1_2	GPX3_3 GPX2_1 GPX2_0 */
/* WAKEUP_INT1[1]    WAKEUP_INT1[2]  WAKEUP_INT3[3]  XEINT_17  XEINT_16 */

/* WAKEUP_INT1[1] --- EXT_INT41[1] --- EINT[9] --- SPI[25]/ID[57] */

/* GPX1_1 ---XEINT[9]---SPI[25]/ID[57] */
/* GPX1_1 ---WAKEUP_INT1[1]--- */
struct button_irq_desc{
	int irq;
	int pin;
	int number;
	char *name;
};

static struct button_irq_desc button_irq[] = {
	{IRQ_EINT(9),   EXYNOS4_GPX1(1), 0, "HOME"},
	{IRQ_EINT(10), EXYNOS4_GPX1(2), 1, "BACK"},
	{IRQ_EINT(27), EXYNOS4_GPX3(3), 2, "SLEEP"},
	{IRQ_EINT(17), EXYNOS4_GPX2(1), 3, "VOL-"},
	{IRQ_EINT(16), EXYNOS4_GPX2(0), 4, "VOL+"},
};

static irq_handler_t button_irq(int irq, void *dev_id)
{
	struct button_irq_desc *button_desc = (struct button_irq_desc *)dev_id;
	if(!gpio_get_value(button_desc->pin))
	{
		/* which key is down */
		key_value = button_desc->number;
	}
	else
		return IRQ_RETVAL(IRQ_HANDLED);

	ev_press = 1;
	wake_up_interruptible(&button_waitq);

	return IRQ_RETVAL(IRQ_HANDLED);
}

static int button_open(struct inode *inode, struct file *file)
{
	int i=0;
	int err = 0;

	for(i=0; i<sizeof(button_irq)/sizeof(button_irq[0]); i++)
	{
		if(button_irq[i].irq >= 0)
			err = request_irq(button_irq[i].irq , button_irq, IRQ_TYPE_EDGE_FALLING , 
								button_irq[i].name , (void *)&button_irq[i]);
		if(err)
			break;
		
	}
	/* release all malloced resource */
	if(err)
	{
		i--;
		for(; i>=0; i--)
		{
			if(button_irq[i].irq < 0)
				continue;
			disable_irq(button_irq[i].irq );
			free_irq(button_irq[i].irq , (void *)&button_irq[i])
		}
		return -EBUSY;
	}

	/* all request_irq is OK */
	ev_press = 0;
	return 0;
}

static int button_release(struct inode *inode , struct file *file)
{
	int i=0;
	for(i=0; i< sizeof(button_irq)/sizeof(button_irq[0]); i++)
	{
		if(button_irq[i].irq<0)
    			continue;
   		free_irq(button_irq[i].irq,(void *)&button_irq[i]);
	}
}

static ssize_t button_read (struct file *file, char __user *buff, size_t count, loff_t *offp)
{
	unsigned int err;
	if(!ev_press)
	{
		if(filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		else
			wait_event_interruptible(button_waitq , ev_press);
	}

	ev_press = 0;
	err = copy_to_user(buff, &key_value , min(sizeof(key_value), count));

	return err?-EFAULT:min(sizeof(key_value), count);
}

static unsigned int button_poll (struct file *file, struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	poll_wait(file, &button_waitq, wait);
	if(ev_press)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static struct file_operations button_fops = {
	.owner = THIS_MODULE,
	.open = button_open,
	.read = button_read,
	.release = button_release,
	.poll = button_poll,
};

static struct miscdevice  button_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEVICE_NAME,
	.fops = &button_fops,
};
static struct class *button_class;
static int __init button_init(void)
{
	int ret = 0;
	ret = misc_register(&button_misc);
	button_class = class_create(THIS_MODULE, DEVICE_NAME);
	device_create(button_class , NULL, MKDEV(10, 0),  NULL, DEVICE_NAME);
	
	if(ret == 0)
		printk(" misc_register success \n ");
	else
		printk(" misc_register failed \n ");
	
	return ret;
}

static void __exit button_exit(void)
{
	misc_deregister(&button_misc);
}


module_init(button_init);
module_exit(button_exit);
MODULE_LICENSE("GPL");
