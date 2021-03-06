#include <linux/fb.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/suspend.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/div64.h>
#include <asm/mach/map.h>

struct lcd_regs {
unsigned long	lcdcon1;	
unsigned long	lcdcon2;	
unsigned long	lcdcon3;	
unsigned long	lcdcon4;	
unsigned long	lcdcon5;    
unsigned long	lcdsaddr1;    
unsigned long	lcdsaddr2;    
unsigned long	lcdsaddr3;    
unsigned long	redlut;    
unsigned long	greenlut;    
unsigned long	bluelut;    
unsigned long	reserved[9];    
unsigned long	dithmode;    
unsigned long	tpal;    
unsigned long	lcdintpnd;    
unsigned long	lcdsrcpnd;    
unsigned long	lcdintmsk;    
unsigned long	lpcsel;
};


static struct fb_info *s3c_lcd;
static volatile unsigned long *gpbcon;
static volatile unsigned long *gpbdat;
static volatile unsigned long *gpccon;
static volatile unsigned long *gpdcon;
static volatile unsigned long *gpgcon;
static volatile struct lcd_regs* lcd_regs;

static int s3c_lcdfb_setcolreg(unsigned int regno, unsigned int red,			     
								unsigned int green, unsigned int blue,			     
								unsigned int transp, struct fb_info *info)
{	
	unsigned int val;		
	if (regno > 16)		
		return 1;	/* 用red,green,blue三原色构造出val */	
	val  = chan_to_field(red,	&info->var.red);	
	val |= chan_to_field(green, &info->var.green);	
	val |= chan_to_field(blue,	&info->var.blue);		
	
	pseudo_palette[regno] = val;	
	return 0;
}

static int mylcd_open(struct fb_info *info, int user)
{
	pm_runtime_get_sync(&lcd_dev.dev);
	return 0;
}

static int mylcd_release(struct fb_info *info, int user)
{
	/* autosuspend: 如果不想让设备频繁地开关,使用autosuspend功能 */
	pm_runtime_mark_last_busy(&lcd_dev.dev);
	pm_runtime_put_sync_autosuspend(&lcd_dev.dev);
	return 0;
}

static struct fb_ops s3c_lcdfb_ops = {	
	.owner		= THIS_MODULE,	
	.fb_setcolreg	= s3c_lcdfb_setcolreg,	
	.fb_fillrect	= cfb_fillrect,	
	.fb_copyarea	= cfb_copyarea,	
	.fb_imageblit	= cfb_imageblit,
	.fb_open = mylcd_open,
	.fb_release = mylcd_release,
};

static int lcd_suspend_notifier(struct notifier_block *nb, unsigned long event, void *dummy)
{
	switch(event) 
	{
	case PM_SUSPEND_PREPARE:
		printk(" lcd suspend notifiler test : PM_SUSPEND_PREPARE\n ");
		return NOTIFY_OK;

	case PM_POST_SUSPEND:
		printk(" lcd suspend notifiler test : PM_POST_SUSPEND\n ");
		return NOTIFY_OK;

	default:
		return NOTIFY_DONE;
	}
}

struct notifier_block * lcd_pm_notif_block = {
	.notifier_call = lcd_suspend_notifier,
};

static void lcd_release(struct device *dev)
{
}

struct platform_device lcd_dev = {
	.name = "lcd_pm",
	.id = -1,
	.dev = {
		.release = lcd_release,
	},	
};

static struct lcd_regs lcd_regs_backup; 

static int lcd_suspend(struct device *dev)
{
	int i;	
	unsigned long *dest = &lcd_regs_backup;	
	unsigned long *src  = lcd_regs;		
	for (i = 0; i < sizeof(lcd_regs_backup)/sizeof(unsigned long); i++)	
	{		
		dest[i] = src[i];	
	}		
	lcd_regs->lcdcon1 &= ~(1<<0); /* 关闭LCD本身 */	
	*gpbdat &= ~1;     /* 关闭背光 */	
	return 0;
}

static int lcd_resume(struct device *dev)
{
	int i;
	unsigned long *dest = lcd_regs;	
	unsigned long *src  = &lcd_regs_backup;	
	struct clk *clk = clk_get(NULL, "lcd");	
	clk_enable(clk);	clk_put(clk);
	lcd_regs->lcdcon1 = lcd_regs_backup.lcdcon1 & ~1;	
	lcd_regs->lcdcon2 = lcd_regs_backup.lcdcon2;	
	lcd_regs->lcdcon3 = lcd_regs_backup.lcdcon3;	
	lcd_regs->lcdcon4 = lcd_regs_backup.lcdcon4;	
	lcd_regs->lcdcon5 = lcd_regs_backup.lcdcon5;	
	lcd_regs->lcdsaddr1 = lcd_regs_backup.lcdsaddr1;	
	lcd_regs->lcdsaddr2 = lcd_regs_backup.lcdsaddr2;	
	lcd_regs->lcdsaddr3 = lcd_regs_backup.lcdsaddr3;
	
	lcd_regs->lcdcon1 |= (1<<0); /* 使能LCD控制器 */	
	lcd_regs->lcdcon5 |= (1<<3); /* 使能LCD本身 */	
	*gpbdat |= 1;     /* 输出高电平, 使能背光 */			
	return 0;
}

static struct dev_pm_ops lcd_pm = {
	.suspend = lcd_suspend,
	.resume   = lcd_resume, 
	.runtime_suspend = lcd_suspend,
	.runtime_resume = lcd_resume,
}; 

static int lcd_probe(struct platform_device *pdev)
{
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_use_autosuspend(&pdev->dev); /* suspend + timer */
	pm_runtime_enable(&pdev->dev);
	return 0;
}

static int lcd_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);
	return 0;
}

struct platform_driver lcd_drv = {
	.probe = lcd_probe,
	.remove = lcd_remove,
	.driver = {
		.name = "lcd_pm",
		.pm     = &lcd_pm,
	},
};

static int s3c2440_lcd_init(void)
{
	/* 1. 分配一个fb_info */	
	s3c_lcd = framebuffer_alloc(0, NULL);
	
	/* 2. 设置 */	
	/* 2.1 设置固定的参数 */	
	strcpy(s3c_lcd->fix.id, "mylcd");	
	s3c_lcd->fix.smem_len = 480*272*16/8;	
	s3c_lcd->fix.type     = FB_TYPE_PACKED_PIXELS;	
	s3c_lcd->fix.visual   = FB_VISUAL_TRUECOLOR; /* TFT */	
	s3c_lcd->fix.line_length = 480*2;		
	/* 2.2 设置可变的参数 */	
	s3c_lcd->var.xres           = 480;	
	s3c_lcd->var.yres           = 272;	
	s3c_lcd->var.xres_virtual   = 480;	
	s3c_lcd->var.yres_virtual   = 272;	
	s3c_lcd->var.bits_per_pixel = 16;	/* RGB:565 */	
	s3c_lcd->var.red.offset     = 11;	
	s3c_lcd->var.red.length     = 5;		
	s3c_lcd->var.green.offset   = 5;	
	s3c_lcd->var.green.length   = 6;	
	s3c_lcd->var.blue.offset    = 0;	
	s3c_lcd->var.blue.length    = 5;	
	s3c_lcd->var.activate       = FB_ACTIVATE_NOW;			
	/* 2.3 设置操作函数 */	
	s3c_lcd->fbops              = &s3c_lcdfb_ops;		
	/* 2.4 其他的设置 */	
	s3c_lcd->pseudo_palette = pseudo_palette;	//s3c_lcd->screen_base  = ;  
	/* 显存的虚拟地址 */ 	
	s3c_lcd->screen_size   = 480*272*16/8;
	
	/* 3. 硬件相关的操作 */	
	/* 3.1 配置GPIO用于LCD */	
	gpbcon = ioremap(0x56000010, 8);	
	gpbdat = gpbcon+1;	
	gpccon = ioremap(0x56000020, 4);	
	gpdcon = ioremap(0x56000030, 4);	
	gpgcon = ioremap(0x56000060, 4);    
	*gpccon  = 0xaaaaaaaa;   
	*gpdcon  = 0xaaaaaaaa;   
	*gpbcon &= ~(3); 
	*gpbcon |= 1;	
	*gpbdat &= ~1;   
	*gpgcon |= (3<<8); 	
	/* 3.2 根据LCD手册设置LCD控制器, 比如VCLK的频率等 */	
	lcd_regs = ioremap(0x4D000000, sizeof(struct lcd_regs));		
	lcd_regs->lcdcon1  = (4<<8) | (3<<5) | (0x0c<<1);
#if 1	

	lcd_regs->lcdcon2  = (1<<24) | (271<<14) | (1<<6) | (9);	
      	lcd_regs->lcdcon3 = (1<<19) | (479<<8) | (1);	
      	lcd_regs->lcdcon4 = 40;
#else
	lcd_regs->lcdcon2 =	S3C2410_LCDCON2_VBPD(5) | \		
			S3C2410_LCDCON2_LINEVAL(319) | \		
			S3C2410_LCDCON2_VFPD(3) | \		
			S3C2410_LCDCON2_VSPW(1);

	lcd_regs->lcdcon3 =	S3C2410_LCDCON3_HBPD(10) | \		
			S3C2410_LCDCON3_HOZVAL(239) | \		
			S3C2410_LCDCON3_HFPD(1);
	
	lcd_regs->lcdcon4 =	S3C2410_LCDCON4_MVAL(13) | \		
			S3C2410_LCDCON4_HSPW(0);
#endif		
	lcd_regs->lcdcon5 = (1<<11) | (0<<10) | (1<<9) | (1<<8) | (1<<0);		
	/* 3.3 分配显存(framebuffer), 并把地址告诉LCD控制器 */	
	s3c_lcd->screen_base = dma_alloc_writecombine(NULL, s3c_lcd->fix.smem_len, &s3c_lcd->fix.smem_start, GFP_KERNEL);		
	lcd_regs->lcdsaddr1  = (s3c_lcd->fix.smem_start >> 1) & ~(3<<30);	
	lcd_regs->lcdsaddr2  = ((s3c_lcd->fix.smem_start + s3c_lcd->fix.smem_len) >> 1) & 0x1fffff;	
	lcd_regs->lcdsaddr3  = (480*16/16);  /* 一行的长度(单位: 2字节) */			
	lcd_regs->lcdcon5 |= (1<<3);
	/* 4. 注册 */	
	register_framebuffer(s3c_lcd);	

	/* 添加休眠与唤醒 */
	register_pm_notifier(&lcd_pm_notif_block);
	platform_device_register(&lcd_dev);
	platform_driver_register(&lcd_drv);
	return 0;
}

static void s3c2440_lcd_exit(void)
{
	unregister_framebuffer(s3c_lcd);	
	lcd_regs->lcdcon1 &= ~(1<<0); /* 关闭LCD本身 */	
	*gpbdat &= ~1;     /* 关闭背光 */	
	dma_free_writecombine(NULL, s3c_lcd->fix.smem_len, s3c_lcd->screen_base, s3c_lcd->fix.smem_start);	
	iounmap(lcd_regs);	
	iounmap(gpbcon);	
	iounmap(gpccon);	
	iounmap(gpdcon);	
	iounmap(gpgcon);	
	framebuffer_release(s3c_lcd);

	unregister_pm_notifier(&lcd_pm_notif_block); /* 通知内核关闭设备 */
	platform_device_unregister(&lcd_dev);
	platform_driver_unregister(&lcd_drv);
}

module_init(s3c2440_lcd_init);
module_exit(s3c2440_lcd_exit);
MODULE_LICENSE("GPL");
