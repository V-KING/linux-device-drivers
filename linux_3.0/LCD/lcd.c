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

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/div64.h>

#include <asm/mach/map.h>
#include <asm/arch/regs-lcd.h>
#include <asm/arch/regs-gpio.h>
#include <asm/arch/fb.h>




static struct fb_info *exy4412_lcd;
static volatile unsigned long *gpf0con;
static volatile unsigned long *gpf1con;
static volatile unsigned long *gpf2con;
static volatile unsigned long *gpf3con;

static volatile unsigned char *gpl1con;
static volatile unsigned char *gpl1dat;
static u32 pseudo_palette[16];
/* RGB interface  LCDBLKC_CFG (0x1001_0210)*/
static struct lcd_regs = {
	unsigned long VIDCON0;/0x11C0_0000/
	unsigned long VIDCON1;
	unsigned long VIDCON2;
	

};

static struct lcd_regs *lcd_regs;

static inline unsigned int chan_to_field(unsigned int chan, struct fb_bitfield *bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf->length;
	return chan << bf->offset;
}

static int exy4412_lcdfb_setcolreg(unsigned int regno, unsigned int red,
			     unsigned int green, unsigned int blue,
			     unsigned int transp, struct fb_info *info)
{
	unsigned int val;
	
	if (regno > 16)
		return 1;

	/* 用red,green,blue三原色构造出val */
	val  = chan_to_field(red,	&info->var.red);
	val |= chan_to_field(green, &info->var.green);
	val |= chan_to_field(blue,	&info->var.blue);
	
	//((u32 *)(info->pseudo_palette))[regno] = val;
	pseudo_palette[regno] = val;
	return 0;
}

static struct fb_ops exy4412_lcd_fop = {
	.owner		= THIS_MODULE,
	.fb_setcolreg	= exy4412_lcdfb_setcolreg,
	.fb_fillrect		= exy4412_fillrect,
	.fb_copyarea	= exy4412_copyarea,
	.fb_imageblit	= exy4412_imageblit,
};


static int lcd_init(void)
{
	/* 结构体分配内存 */
	exy4412_lcd = framebuffer_alloc(0,NULL);  
	/* 不需要额外的空间存放私有数据 */

	/* 构造结构体数据 */
	strcpy(exy4412_lcd->fix.id, "4.3_lcd");
	
	exy4412_lcd->fix.smem_len = 480*272*24/8;  	/* WXCAT43-TG6#001_V1.0.pdf  */
	exy4412_lcd->fix.line_length = 480*3;
	exy4412_lcd->fix.visual   = FB_VISUAL_TRUECOLOR; /* TFT 真彩色*/

	exy4412_lcd->var.xres = 480;
	exy4412_lcd->var.yres = 272;
	exy4412_lcd->var.xres_virtual = 480;
	exy4412_lcd->var.yres_virtual = 272;
	exy4412_lcd->var.bits_per_pixel = 24;	/* 每个像素24位 */

	exy4412_lcd->var.red.offset = 16;
	exy4412_lcd->var.red.length = 8;
	
	exy4412_lcd->var.green.offset = 8;
	exy4412_lcd->var.green.length = 8;
	
	exy4412_lcd->var.blue.offset = 0;
	exy4412_lcd->var.blue.length = 8;
	
	exy4412_lcd->var.activate = FB_ACTIVATE_NOW;	/* set values immediately (or vbl)*/
		
	exy4412_lcd->fbops = &exy4412_lcd_fop;
	exy4412_lcd->pseudo_palette = pseudo_palette;	/* 调色板 */
	//exy4412_lcd->screen_base = 
	exy4412_lcd->screen_size = 480*272*24/8;

	/* exynos4412 display control */
	gpf0con = ioremap(0x11400180, 4);
	gpf1con = ioremap(0x114001A0, 4);
	gpf2con = ioremap(0x114001C0, 4);
	gpf3con = ioremap(0x114001E0, 4);
										
	gpl1con = ioremap(0x110000E0, 1);
	gpl1dat = ioremap(0x110000E4, 1);
	/* GPL1_0 LCD_PWDN	 */
	/* lcd 用的引脚设置为输出 */
	*gpf0con = 0x11111111;/* HSYNC VSYNC DEN VOTCLK VD0-3 */
	*gpf1con = 0x11111111;/* VD4-11 */
	*gpf2con = 0x11111111;/* VD12-19 */
	*gpf3con &= 0xffff0000;
	*gpf3con |= 0x00001111;/* VD20-23 */
	
	/* 电源引脚设置输出并输出为1 */
	*gpl1con &= 0xf0;
	*gpl1con |= 0x01;
	*gpl1dat |= 0x01;  /* GPL1_0 输出高电平 */

	/* 设置display_control */
	lcd_regs = ioremap(0x11C0_0000 , sizeof(struct lcd_regs));

	/* VIDCON0 
	  * [31] 	0
	  * [30]	1	enable
	  * [29]	0
	  * [28:26]	000 RGB
	  * [18]	1 = RGB serial format
	  * [0:1]	11 display on*/

	

	exy4412_lcd->fix.smem_start = 		/* 显存的地址 */
	register_framebuffer(exy4412_lcd);
}

static void lcd_exit(void)
{

}

module_init(lcd_init);
module_exit(lcd_exit);
MODULE_LICENSE("GPL");
