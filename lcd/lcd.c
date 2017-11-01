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

/* LCD的驱动程序分为两层， fb_mem.c 调用file_operatios结构体里的函数 
 *  -> 函数调用lcd.c 里的register_framebuffer()注册的fb_info结构体
 *  -> fb_info结构体记录 帧缓冲设备的信息 包括LCD设备的设置参数、状态以及对底层硬件操作的函数指针 */

/* 设置LCD相关的寄存器 */
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

static volatile struct lcd_regs  * lcd_regs;

/* 帧缓冲为显示设备提供一个接口 */
/* fb_info结构体记录 帧缓冲设备的信息 包括设备的设置参数、状态以及对底层硬件操作的函数指针*/
static struct fb_info *s3c_lcd;
static volatile unsigned long *gpbcon;
static volatile unsigned long *gpbdat;
static volatile unsigned long *gpccon;

static volatile unsigned long *gpdcon;
static volatile unsigned long *gpddat;

//用于16位调色板的数组
static u32 pseudo_palette [16];

static inline unsigned int chan_to_field(unsigned int chan, struct fb_bitfield *bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf->length;
	return chan << bf->offset;
}
           
static int s3c_lcdfb_setcolreg( unsigned int regno, unsigned int red,
								unsigned int green, unsigned int blue,
								unsigned int transp, struct fb_info *info )
{
	unsigned int val;
	if(regno > 16)
		return 1;
	/* 用red,green,biue三原色构造出val */
	/* 顺序不可以改变，容易出错 */
	val =  chan_to_field(red, &info->var.red);
	val |= chan_to_field(green, &info->var.green);
	val |= chan_to_field(blue, &info->var.blue);
	/* 使用调色板 */
	/* 我们可能想要节省内存空间，在显存里只给了每个象素一个字节，这时的象素取出来就不能直接给lcd了,
	 * 这时就需要调色板。所谓调色板就相当于一个数组，每一项都代表一个16位的象素（当然根据具体的lcd不同
	 * 可以自己配置调色板），可以根据从显存中取出的数据来选择数组中的一项，这个16位象素就可以传给lcd了*/
	pseudo_palette[regno]  = val;
	return 0;
}

static struct fb_ops s3c_lcdfb_ops = {
	.owner = THIS_MODULE,

	.fb_setcolreg = s3c_lcdfb_setcolreg,
	/* 下面三个函数在/drivers/video下实现 */
	.fb_copyarea  = s3c_copyarea,
	.fb_fillrect  = cfb_fillrect,
	.fb_imageblit  = cfb_imageblit, 
}

static int lcd_init(void)
{
	/* 分配一个fb_info结构体 */
	s3c_lcd = framebuffer_alloc(0, NULL);

	/* 设置LCD设备固定的参数 */
	strcpy(s3c_lcd->fix.id , "mylcd");
	s3c_lcd->fix.smem_len = 480*272*16/8;////显存的长度=分辨率*每象素字节数(RGB 565 所以是两个字节)
	s3c_lcd->fix.type     = FB_TYPE_PACKED_PIXELS;//填充像素
	s3c_lcd->fix.visual   = FB_VISUAL_TRUECOLOR; // TFT为真彩色 
	s3c_lcd->fix.line_length = 480*2;//每行的长度 = 一行的像素个数 * 每象素字节数

	/* 设置LCD设备可变的参数 */
	s3c_lcd->var.xres     = 480;//x方向分辨率
	s3c_lcd->var.yres     = 272;//y方向分辨率
	s3c_lcd->var.xres_virtual = 480;//x方向虚拟分辨率
	s3c_lcd->var.yres_virtual = 272;//y方向虚拟分辨率
	s3c_lcd->var.bits_per_pixel = 16;//每像素字节数

	/* 设置RGB 565 (两个字节来设置RGB)*/
	s3c_lcd->var.red.offset     = 11;
	s3c_lcd->var.red.length     = 5;
	
	s3c_lcd->var.green.offset   = 5;
	s3c_lcd->var.green.length   = 6;

	s3c_lcd->var.blue.offset    = 0;
	s3c_lcd->var.blue.length    = 5;

	s3c_lcd->var.activate       = FB_ACTIVATE_NOW;//设置的值立即生效

	/* 设置操作函数 */
	s3c_lcd->fbops              = &s3c_lcdfb_ops;

	/* 其余参数设置 */
	s3c_lcd->pseudo_palette = pseudo_palette;//存放调色板所调颜色的数组
	s3c_lcd->screen_size    = 480*272*16/8;//显存的大小

	/* 硬件相关的操作 */
	gpbcon = ioremap(0x56000010, 8);//寄存器的映射
	gpbdat = gpbcon + 1;
	gpccon = ioremap(0x56000020, 4);
	gpdcon = ioremap(0x56000030, 4);
	gpgcon = ioremap(0x56000060, 4);

	*gpccon  = 0xaaaaaaaa;/* 配置寄存器GPCCON为LCD专用的寄存器 */
	*gpdcon  = 0xaaaaaaaa;/* 配置寄存器GPDCON为LCD的数据总线　*/
	
	*gpbcon  &= ~(3);
	*gpbcon  |= 1;        /* GPB0 被设置为输出引脚 */
	*gpbdat  &= ~1;       /* GPB0输出0  */

	*gpgcon  |= (3<<8);   /* GPG4设置为LCD_PWREN */

	/* LCD控制器包含的所有寄存器进行内存映射 */

	lcd_regs = ioremap(0x4D000000, sizeof(struct lcd_regs));/* 0x4D000000 为lcdcon1 的地址 */
	/* 结合LCD手册 和 s3c2440的lcd控制器进行对lcd控制器的设置 */
	/* bit[17:8]: VCLK = HCLK / [(CLKVAL+1) x 2]  ,(CLKVAL > 2), LCD手册P14 时间最少是100ns,即LCD时钟为10MHZ
	 *            10MHz(100ns) = 100MHz / [(CLKVAL+1) x 2]  10MHZ < 12MHZ
	 *            CLKVAL = 4  设置时钟
	 * bit[6:5]: 0b11, TFT LCD  (11 = TFT LCD panel)
	 * bit[4:1]: 0b1100, 16 bpp for TFT    1100 = 16 bpp for TFT
	 * bit[0]  : 0 = Disable the video output and the LCD control signal.
	 */
	lcd_regs->lcdcon1 = (4<<8) | (3<<5) | (0x0c<<1);
	
#if 1

	lcd_regs->lcdcon2 = (1<<24) | (271<<14) | (1<<6) | (9);

	lcd_regs->lcdcon3 = (1<<19) | (479<<8)  | (1);

	lcd_regs->lcdcon4 = 40;

#endif	
	/* 信号的极性 
	 * bit[11]: 1=565 format
	 * bit[10]: 0 = The video data is fetched at VCLK falling edge
	 * bit[9] : 1 = HSYNC信号要反转,即低电平有效 通过比较s3c2440的时序图和lcd的时序图可知需要反转
	 * bit[8] : 1 = VSYNC信号要反转,即低电平有效 
	 * bit[6] : 0 = VDEN不用反转
	 * bit[3] : 0 = PWREN输出0
	 * bit[1] : 0 = BSWP   //BSWP HWSWP 这两位决定显存的存放方式
	 * bit[0] : 1 = HWSWP 2440手册P413
	 */
	lcd_regs->lcdcon5 = (1<<11) | (0<<10) | (1<<9) | (1<<8) | (1<<0);
	
	/* 3.3 分配显存(framebuffer), 并把地址告诉LCD控制器 */
	//s3c_lcd->fix.smem_len 显存的长度
	//&s3c_lcd->fix.smem_start 显存的起始地址
	//PS: 开发板没有专用的显存，使用内存分配去代替显存的
	s3c_lcd->screen_base = dma_alloc_wirtecombile(NULL, s3c_lcd->fix.smem_len, &s3c_lcd->fix.smem_start, GPF_KERNEL);
	//& ~(3<<30) 表示[31:32]不进行操作
	lcd_regs->lcdsaddr1 = (s3c_lcd->fix.smem_start >> 1) & ~(3<<30);
	//& 0x1fffff 表示[21:32]不进行操作
	//问题：LCDBASEL = ((the frame end address) >>1) + 1 手册要求加一操作，这里没有？？
	lcd_regs->lcdsaddr2 = ((s3c_lcd->fix.smem_start + s3c_lcd->fix.smem_len) >> 1) & 0x1fffff;
	lcd_regs->lcdsaddr3 = (480 * 16 / 16);  /* 一行的长度即宽度设置（单位：两个字节） */

	lcd_regs->lcdcon1   |= (1<<0);//一开始失能，都配置完毕后再使能LCD控制器
	lcd_regs->lcdcon5   |= (1<<3);//Enable PWREN signal LCD屛开关的电源信号
	*gpbdat   |= 1;		//使能背光


	/* 注册 */
	register_framebuffer(s3c_lcd);

	return 0;
}

static void lcd_exit(void)
{
	unregister_framebuffer(s3c_lcd);
	lcd_regs->lcdconl &= ~(1<<0);  /* 关闭LCD */
	*gpbdat &= ~1;//失能背光
	//释放分配的显存
	dma_free_writecombine(NULL, s3c_lcd->fix.sem_len, s3c_lcd->screen_base, s3c_lcd->fix.smem_start);

	iounmap(lcd_regs);
	iounmap(gpbcon);
	iounmap(gpccon);
	iounmap(gpdcon);
	iounmap(gpgcon);
	//释放一个fb_info结构体
	framebuffer_release(s3c_lcd);

}

module_init(lcd_init);
module_exit(lcd_exit);
MODULE_LICENSE("GPL");