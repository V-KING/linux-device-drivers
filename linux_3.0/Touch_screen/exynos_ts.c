/* 分析: cpu 由IIC 控制器取touch screen 坐标数据
  * 由input 输入子系统上报数据 */
/* GPL0_2 TP1_EN 1.8V--->3.3V 转换芯片 */
/* GPX0_3 TP_IOCTL  */
/* topeet: cpu ---->IIC (TSC2007)touch screen control---->touch sreen */
/* I2C_SCL7 GPD0_3    
  * I2C_SDA7 GPD0_2 */
/* GPLX0_0 数据可用中断 */

#define TOUCH_MAX_X	480
#define TOUCH_MAX_Y	272
#define TSC2007_NAME	"tsc2007"

static struct tsc2007_i2c_platform_data {
		uint32_t gpio_irq;			// IRQ port
		uint32_t irq_cfg;
	
		uint32_t gpio_wakeup;		// Wakeup support
		uint32_t wakeup_cfg;

		uint32_t gpio_reset;		// Reset support
		uint32_t reset_cfg;

		int screen_max_x;
		int screen_max_y;
		int pressure_max;
};

static struct tsc2007_ts_data {
		struct input_dev *input_dev;
		struct ft5x0x_event event;

		uint32_t gpio_irq;
		uint32_t gpio_wakeup;
		uint32_t gpio_reset;

		int screen_max_x;
		int screen_max_y;
		int pressure_max;

		struct work_struct work;
		struct workqueue_struct *queue;

#ifdef CONFIG_HAS_EARLYSUSPEND
		struct early_suspend early_suspend;
#endif
};

/* static struct i2c_board_info __initdata bfin_i2c_board_info[] = {
#if defined(CONFIG_TOUCHSCREEN_AD7160) || defined(CONFIG_TOUCHSCREEN_AD7160_MODULE)
	{
		I2C_BOARD_INFO("ad7160", 0x33),
		.irq = IRQ_PH1,
		.platform_data = (void *)&bfin_ad7160_ts_info,
	},
#endif
}; */

static int TSC2007_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tsc2007_i2c_platform_data *pdata;
	struct tsc2007_ts_data *ts;
	struct input_dev *input_dev;
	unsigned char val;
	int err = -EINVAL;

	/* Return 1 if adapter supports everything we need, 0 if not. */
	/* 确定IIC  what functionality is present  */
	/* 是否支持IIC 方式通信 */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) 
	{
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}

	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (!ts) 
	{
		err = -ENOMEM;
		goto exit_alloc_data_failed;
	}

	pdata = client->dev.platform_data;	/* 得到设备的平台数据 */
	if (!pdata) 
	{
		dev_err(&client->dev, "failed to get platform data\n");
		goto exit_no_pdata;
	}

	ts->screen_max_x = pdata->screen_max_x;
	ts->screen_max_y = pdata->screen_max_y;
	ts->pressure_max = pdata->pressure_max;

	ts->gpio_irq = pdata->gpio_irq;
	if (ts->gpio_irq != -EINVAL) {
		client->irq = gpio_to_irq(ts->gpio_irq);
	} else {
		goto exit_no_pdata;
	}
	if (pdata->irq_cfg) {
		s3c_gpio_cfgpin(ts->gpio_irq, pdata->irq_cfg);
		s3c_gpio_setpull(ts->gpio_irq, S3C_GPIO_PULL_NONE);
	}

	ts->gpio_wakeup = pdata->gpio_wakeup;
	ts->gpio_reset = pdata->gpio_reset;

	INIT_WORK(&ts->work, ft5x0x_ts_pen_irq_work);
	this_client = client;
	i2c_set_clientdata(client, ts);

	ts->queue = create_singlethread_workqueue(dev_name(&client->dev));
	if (!ts->queue) {
		err = -ESRCH;
		goto exit_create_singlethread;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		err = -ENOMEM;
		dev_err(&client->dev, "failed to allocate input device\n");
		goto exit_input_dev_alloc_failed;
	}

	ts->input_dev = input_dev;

	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_ABS, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);

#ifdef CONFIG_FT5X0X_MULTITOUCH
	set_bit(ABS_MT_TRACKING_ID, input_dev->absbit);
	set_bit(ABS_MT_TOUCH_MAJOR, input_dev->absbit);
	set_bit(ABS_MT_WIDTH_MAJOR, input_dev->absbit);
	set_bit(ABS_MT_POSITION_X, input_dev->absbit);
	set_bit(ABS_MT_POSITION_Y, input_dev->absbit);

	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, ts->screen_max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, ts->screen_max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, ts->pressure_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, 0, 200, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, FT5X0X_PT_MAX, 0, 0);
#else
	set_bit(ABS_X, input_dev->absbit);
	set_bit(ABS_Y, input_dev->absbit);
	set_bit(ABS_PRESSURE, input_dev->absbit);
	set_bit(BTN_TOUCH, input_dev->keybit);

	input_set_abs_params(input_dev, ABS_X, 0, ts->screen_max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, ts->screen_max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_PRESSURE, 0, ts->pressure_max, 0 , 0);
#endif

	input_dev->name = FT5X0X_NAME;
	input_dev->id.bustype = BUS_I2C;
	input_dev->id.vendor = 0x12FA;
	input_dev->id.product = 0x2143;
	input_dev->id.version = 0x0100;

	err = input_register_device(input_dev);
	if (err) {
		input_free_device(input_dev);
		dev_err(&client->dev, "failed to register input device %s, %d\n",
				dev_name(&client->dev), err);
		goto exit_input_dev_alloc_failed;
	}

	msleep(3);
	err = ft5x0x_read_fw_ver(&val);
	if (err < 0) {
		dev_err(&client->dev, "chip not found\n");
		goto exit_irq_request_failed;
	}

	err = request_irq(client->irq, ft5x0x_ts_interrupt,
			IRQ_TYPE_EDGE_FALLING /*IRQF_TRIGGER_FALLING*/, "ft5x0x_ts", ts);
	if (err < 0) {
		dev_err(&client->dev, "Request IRQ %d failed, %d\n", client->irq, err);
		goto exit_irq_request_failed;
	}

	disable_irq(client->irq);

	dev_info(&client->dev, "Firmware version 0x%02x\n", val);

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;//EARLY_SUSPEND_LEVEL_DISABLE_FB + 1;
	ts->early_suspend.suspend = ft5x0x_ts_suspend;
	ts->early_suspend.resume = ft5x0x_ts_resume;
	register_early_suspend(&ts->early_suspend);
#endif

	enable_irq(client->irq);

	//cym 4412_set_ctp(CTP_FT5X06);
	dev_info(&client->dev, "FocalTech ft5x0x TouchScreen initialized\n");
	return 0;

exit_irq_request_failed:
	input_unregister_device(input_dev);

exit_input_dev_alloc_failed:
	cancel_work_sync(&ts->work);
	destroy_workqueue(ts->queue);

exit_create_singlethread:
	i2c_set_clientdata(client, NULL);

exit_no_pdata:
	kfree(ts);

exit_alloc_data_failed:
exit_check_functionality_failed:
	dev_err(&client->dev, "probe ft5x0x TouchScreen failed, %d\n", err);

	return err;


static const struct i2c_device_id TSC2007_ts_id[] = {
	{ TSC2007_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, TSC2007_ts_id);

static struct i2c_driver TSC2007_ts_driver = {
	.probe		= TSC2007_ts_probe,
	.remove		= __devexit_p(TSC2007_ts_remove),
	.id_table	= TSC2007_ts_id,
	.driver	= {
		.name	= TSC2007_NAME,
		.owner	= THIS_MODULE,
	},
};


static int ts_init(void)
{
	int ret;
	ret = gpio_request(EXYNOS4_GPL0(2), "TP1_EN");/* 1.8V--->3.3V 转换芯片使能 */
	if(ret != 0)
		printk(KERN_ERR, "request gpio GPL0_2 filed\n");
	gpio_direction_output(EXYNOS4_GPL0(2), 1);
	s3c_gpio_cfgpin(EXYNOS4_GPL0(2), S3C_GPIO_OUTPUT);
	gpio_free(EXYNOS4_GPL0(2));
	mdelay(5);
	
	ret = gpio_request(EXYNOS4_GPX0(3), "GPX0_3");
	gpio_direction_output(EXYNOS4_GPX0(3), 1);	
       s3c_gpio_cfgpin(EXYNOS4_GPX0(3), S3C_GPIO_OUTPUT);
	gpio_free(EXYNOS4_GPX0(3));
	
	return i2c_add_driver(&TSC2007_ts_driver);	
}

static void ts_exit(void)
{
	
}

module_init(ts_init);
module_exit(ts_exit);
MODULE_LICENSE("GPL");
