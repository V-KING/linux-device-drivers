//用input输入子系统来写
//事件触发过程：按下触摸屏，进入触摸屏中断》》开始AD转换》》进入AD转换完成中断
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/serio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <asm/plat-s3c24xx/ts.h>

#include <asm/arch/regs-adc.h>
#include <asm/arch/regs-gpio.h>

struct s3c_ts_regs {
	unsigned long adccon;
	unsigned long adctsc;
	unsigned long adcdly;
	unsigned long adcdat0;
	unsigned long adcdat1;
	unsigned long adcupdn;
};

static volatile struct s3c_ts_regs * s3c_ts_regs;
static struct input_dev * s3c_ts_dev;
static struct timer_list ts_timer;

static void enter_wait_pen_down_mode (void)
{
	//检测手是否按下触摸屏
	s3c_ts_regs->adctsc = 0xd3; 
}

static void enter_wait_pen_up_mode(void)
{
	//检测手是否离开触摸屏
	s3c_ts_regs->adctsc = 0x1d3;
}

static void enter_measure_xy_mode (void)
{
	//测量x.y坐标
	s3c_ts_regs->adctsc = (1<<3) | (1<<2);
}

static void start_adc(void)
{
	//ADC 转换开始
	//A/D conversion starts and this bit is cleared after the startup
	s3c_ts_regs->adccon |= (1<<0);
}

static int s3c_filter_ts (int x[], int y[])
{
#define ERR_LIMIT 10

	int avr_x, avr_y;
	int det_x, det_y;

	avr_x = (x[0] + x[1])/2;
	avr_y = (y[0] + y[1])/2;

	det_x = (x[2] > avr_x) ? (x[2] - avr_x) : (avr_x - x[2]);
	det_y = (y[2] > avr_y) ? (y[2] - avr_y) : (avr_y - y[2]);

	if ((det_x > ERR_LIMIT) || (det_y > ERR_LIMIT))
		return 0;

	avr_x = (x[1] + x[2])/2;
	avr_y = (y[1] + y[2])/2;

	det_x = (x[3] > avr_x) ? (x[3] - avr_x) : (avr_x - x[3]);
	det_y = (y[3] > avr_y) ? (y[3] - avr_y) : (avr_y - y[3]);

	if ((det_x > ERR_LIMIT) || (det_y > ERR_LIMIT))
		return 0;

	return 1;
}

static void s3c_ts_timer_function (unsigned long data)
{
	if(s3c_ts_regs->adcdat0 & (1<<15))
	{
		input_report_abs(s3c_ts_dev, ABS_PRESSURE, 0);
		input_report_key(s3c_ts_dev, BIN_TOUCH, 0);
		input_sync(s3c_ts_dev);		//同步 告诉上层，本次的事件已经完成了
		enter_wait_pen_down_mode();//进入等待按下的模式
	}

	else
	{
		enter_measure_xy_mode();//进入测量坐标X Y 模式
		start_adc();//ADC采集开始
	}

}

static irqreturn_t pen_down_up_irq(int irq, void *dev_id)
{
	//0 = Stylus down state
	//1 = Stylus up state
	if(s3c_ts_regs->adcdat0 & (1<<15))
	{
		//up
		input_report_abs(s3c_ts_dev, ABS_PRESSURE, 0);
		input_report_key(s3c_ts_dev, BIN_TOUCH, 0);
		input_sync(s3c_ts_dev);		//同步，即通知上层本次事件结束
		enter_wait_pen_down_mode();
	}
	else
	{
		//down
		enter_measure_xy_mode();
		start_adc();
	}
	return IRQ_HANDLED;
}

static irqreturn_t adc_irq(int irq, void *dev_id)
{
	static int cnt = 0;
	static int x[4], y[4];
	int adcdat0, adcdat1;

	adcdat0 = s3c_ts_regs->adcdat0;
	adcdat1 = s3c_ts_regs->adcdat1;

	if(s3c_ts_regs->adcdat0  & (1<<15))
	{
		cnt = 0;
		input_report_abs(s3c_ts_dev, ABS_PRESSURE, 0);
		input_report_key(s3c_ts_dev, BIN_TOUCH, 0);
		input_sync(s3c_ts_dev);
		enter_wait_pen_down_mode();
	}

	else
	{	//循环AD测量四次，存入一个数组，将数组传给s3c_filter_ts()进行数据排错处理
		x[cnt] = adcdat0 & 0x3ff;
		y[cnt] = adcdat1 & 0x3ff;
		++cnt;
		if(cnt == 4)
		{
			if(s3c_filter_ts(x, y))
			{	//上报事件（X Y坐标）
				input_report_abs(s3c_ts_dev, ABS_X, (x[0]+x[1]+x[2]+x[3])/4);
				input_report_abs(s3c_ts_dev, ABS_Y, (y[0]+y[1]+y[2]+y[3])/4);
				input_report_abs(s3c_ts_dev, ABS_PRESSURE, 1);
				input_report_key(s3c_ts_dev, BTN_TOUCH, 1);
				input_sync(s3c_ts_dev);//事件同步,它告知事件的接收者:驱动已经发出了一个完整的报告
			}

			cnt = 0;
			enter_wait_pen_up_mode();//进入等待中断模式
			//使用定时器，时间上报完毕后，再次进入enter_measure_xy_mode()函数
			//这样就可以处理长按和滑动触摸屏的事件了
			mod_timer(&ts_timer, jiffies + HZ/100);
		}
		else
		{
			//这里实现多次测量
			enter_measure_xy_mode();
			start_adc();
		}
	}
	return IRQ_HANDLED;
}

static int s3c_ts_init(void)
{
	struct clk * clk;
	//分配一个input_dev 输入设备结构体
	s3c_ts_dev = input_allocate_device();
	
	//设置能产生哪类事件
	set_bit(EV_KEY, s3c_ts_dev->evbit);
	set_bit(EV_ABS, s3c_ts_dev->evbit);

	//设置这类事件里的哪些事件
	set_bit(BTN_TOUCH, s3c_ts_dev->keybit);

	input_set_abs_params(s3c_ts_dev, ABS_X, 0, 0x3FF, 0, 0);
	input_set_abs_params(s3c_ts_dev, ABS_Y, 0, 0x3FF, 0, 0);
	input_set_abs_params(s3c_ts_dev, ABS_PRESSURE, 0, 1, 0, 0);

	//注册
	input_register_device(s3c_ts_dev);

	//硬件操作
	// linux时钟管理 获取adc的时钟clock 结构体 struct clk * clk
	// 参数adc 定义在arch\arm\plat-samsung\Clock.c
	clk = clk_get(NULL, "adc");
	// 使能adc 时钟
	clk_enable(clk);
	//触摸屏相关寄存器地址映射
	s3c_ts_regs = ioremap(0x58000000, sizeof(struct s3c_ts_regs));
	// (1<<14) AD转换器分频器使
	// (49<<6) 设置ADC时钟大小
	s3c_ts_regs->adccon = (1<<14) | (49<<6);

	// adc与触摸屏时钟
	request_irq(IRQ_TC, pen_down_up_irq, TRQF_SAMPLE_RANDOM, "ts_pen", NULL);
	request_irq(IRQ_ADC, adc_irq, IRQF_SAMPLE_RANDOM, "adc", NULL);

	//正常转换模式，XY位置模式，自动定位模式  使用ADC转换启动延迟值。
	s3c_ts_regs->adcdly = 0xffff;

	//初始化定时器
	init_timer(&ts_timer);
	ts_timer.function = s3c_ts_timer_function;
	add_timer(&ts_timer);

	//enter_wait_pen_down_mode() 函数里是 s3c_ts_regs->adctsc = 0xd3
	enter_wait_pen_down_mode();
	return 0;
}

static void s3c_ts_exit(void)
{
	free_irq(IRQ_TC, NULL);
	free_irq(IRQ_ADC, NULL);
	iounmap(s3c_ts_regs);
	input_unregister_device(s3c_ts_dev);
	input_free_device(s3c_ts_dev);
	del_timer(&ts_timer);
}

module_init(s3c_ts_init);
module_exit(s3c_ts_exit);

MODULE_LICENSE("GPL");