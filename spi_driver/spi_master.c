#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_bitbang.h>
#include <linux/spi/s3c24xx.h>
#include <linux/module.h>
#include <plat/regs-spi.h>
#include <mach/regs-gpio.h>


static struct *spi_master  spi_control0;
static struct *spi_master  spi_control1; 

static struct s3c_spi_info {
	int irq;    
	unsigned int reg_base;        
	struct completion	 done;    
	struct spi_transfer *cur_t;    
	int cur_cnt;    
	struct platform_device *pdev;
};

static irqreturn_t s3c2440_spi_irq(int irq, void *dev_id)
{
	    struct spi_master *master = (struct spi_master *)dev_id;    
	    struct s3c_spi_info *info = spi_master_get_devdata(master);    
	    struct spi_transfer *t = info->cur_t;    
	    if (!t)    
	    {        
	    	/* 误触发 */    	
		return IRQ_HANDLED;                
	    }   

	    /* 处理 spi_transfer */    
	    if (t->tx_buf) /* 是发送 */    
	    {        

		info->cur_cnt++;                
		if (info->cur_cnt < t->len)/* 没发完? */            
		writeb(((unsigned char *)t->tx_buf)[info->cur_cnt], info->reg_base + S3C2410_SPTDAT);                
		else            
		complete(&info->done); /* 唤醒 */    
	     } 

	     /* 接收 */   	
	    else  
	    {        /* 读/存数据 */        
	    		((unsigned char *)t->rx_buf)[info->cur_cnt] = readb(info->reg_base + S3C2410_SPRDAT);        
			info->cur_cnt++;                
			if (info->cur_cnt < t->len)/* 没收完? */            
				writeb(0xff, info->reg_base + S3C2410_SPTDAT);                
			else            
				complete(&info->done); /* 唤醒 */    
	    }
		
	    return IRQ_HANDLED;	
}

static void s3c2440_spi_controler_init(int bus_num)
{
	struct clk *clk = clk_get(NULL, "spi");    /* 使能spi controller 0/1的时钟 */    
	clk_enable(clk);    clk_put(clk);    
	/* GPIO */    
	if (bus_num == 0)    
	{        
	/* SPI controller 0 */        
	/*         * GPE11 SPIMISO            * GPE12 SPIMOSI            * GPE13 SPICLK             */        
	s3c2410_gpio_cfgpin(S3C2410_GPE(11), S3C2410_GPE11_SPIMISO0);        
	s3c2410_gpio_cfgpin(S3C2410_GPE(12), S3C2410_GPE12_SPIMOSI0);        
	s3c2410_gpio_cfgpin(S3C2410_GPE(13), S3C2410_GPE13_SPICLK0);            
	}    

	else if (bus_num == 1)    
	{        
	/* SPI controller 1 */        
	/*         * GPG5 SPIMISO            * GPG6 SPIMOSI            * GPG7 SPICLK             */        
	s3c2410_gpio_cfgpin(S3C2410_GPG(5), S3C2410_GPG5_SPIMISO1);        
	s3c2410_gpio_cfgpin(S3C2410_GPG(6), S3C2410_GPG6_SPIMOSI1);       
	s3c2410_gpio_cfgpin(S3C2410_GPG(7), S3C2410_GPG7_SPICLK1);                
	/* 使能spi controller 1的时钟 */    
	}
}

static int s3c2440_spi_setup(struct spi_device *spi)
{
	struct s3c_spi_info *info;    
	struct clk *clk;        
	int cpol = 0;   
	int cpha = 0;    
	int div;
	
	info = spi_master_get_devdata(spi->master);    
	clk = clk_get(NULL, "plck");        
      /* 设置传输模式 : mode     
	* 传输频率 : max_speed_hz     
	* bits_per_word : 不用管     
	*/    

	/* spi_mode: CPOL,CPHA组成一个值,0,1,2,3     */    
	if (spi->mode & 1)    
	        cpha = 1;        
	if (spi->mode & 2)    
	        cpol = 1;            

	/* 写入SPCON0,1 */    
      /* [6:5] : 01, polling mode     
	* [4]   : 1 = enable      
	* [3]   : 1 = master     
	* [2]   : CPOL     
	* [1]   : CPHA     
	* [0]   : 0 = normal mode     
	*/	

	writeb((1<<5) | (1<<4) | (1<<3) | (cpol << 2) | (cpha << 1), info->reg_base + S3C2410_SPCON);    
	/*     
	* Baud rate = PCLK / 2 / (Prescaler value + 1)     
	* Prescaler value = PCLK/(2*Baud rate) - 1     
	* Prescaler Value : 0,1,...,255     
	*/	
	div = DIV_ROUND_UP(clk_get_rate(clk), spi->max_speed_hz * 2) - 1;    
	clk_put(clk);	
	if (div > 255)		
		div = 255;	
	writeb(div, info->reg_base + S3C2410_SPPRE);            

	return 0;
}

static int s3c2440_spi_transfer(struct spi_device *spi, struct spi_message *msg)
{
    	struct spi_master *master = spi->master;    
    	struct s3c_spi_info *info;	
    	struct spi_transfer	*t = NULL;    
    	info = spi_master_get_devdata(master);        

    	/* 1. 选中芯片 */    
	s3c2410_gpio_setpin(spi->chip_select, 0);  /* 默认为低电平选中 */    

	/* 2. 发数据 */        
	/* 2.1 发送第1个spi_transfer之前setup */    
	master->setup(spi);        

	/* 2.2 从spi_message中逐个取出spi_transfer,执行它 */	
	list_for_each_entry (t, &msg->transfers, transfer_list) 
	{	    /* 处理这个spi_transfer */        
		info->cur_t = t;        
		info->cur_cnt = 0;    	
		init_completion(&info->done);        
		/* 如果该spi_transfer的speed_hz或bits_per_word         
		* 不是0, 则调用setup来设置         
		*/        

		if (t->tx_buf)        
		{            
			/* 发送 */            
			writeb(((unsigned char *)t->tx_buf)[0], info->reg_base + S3C2410_SPTDAT);            
			/* 它会触发中断 */            /* 休眠 */            
			wait_for_completion(&info->done);        
		}        

		else if(t->rx_buf)        
		{            /* 接收 */            
			writeb(0xff, info->reg_base + S3C2410_SPTDAT);            
			/* 它会触发中断 */            /* 休眠 */            
			wait_for_completion(&info->done);        
		}        

		/* 如果该spi_transfer的cs_change为1         * 则取消片选         */    
	} 
	
	/* 2.3 唤醒等待的进程 */	
	msg->status = 0;	
	msg->complete(msg->context);        

	/* 3. 取消片选 */    
	s3c2410_gpio_setpin(spi->chip_select, 1);  /* 默认为低电平选中 */   
	
	return 0;	
}

static struct *spi_master create_spi_master_s3c2440(int bus_num, unsigned int reg_base, int irq)
{
	int ret;    
	struct spi_master *master;    
	struct s3c_spi_info *info;    
	struct platform_device *pdev;
	
	pdev = platform_device_alloc("s3c2440_spi", bus_num);    
	platform_device_add(pdev);    
	master = spi_alloc_master(&pdev->dev, sizeof(struct s3c_spi_info));  
	
	master->bus_num = bus_num;    
	master->num_chipselect = 0xffff;	
	master->mode_bits = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH;    
	master->setup    = s3c2440_spi_setup;    
	master->transfer = s3c2440_spi_transfer; 
	
	info = spi_master_get_devdata(master);    
	info->reg_base = (unsigned int)ioremap(reg_base, 0x18);    
	info->irq = irq;    info->pdev = pdev;    /* 硬件初始化 */ 
	
	s3c2440_spi_controler_init(bus_num);    
	ret = request_irq(irq, s3c2440_spi_irq, 0, "s3c2440_spi", master);    
	spi_register_master(master);    
	return master;
}

static void destory_spi_master_s3c2440(struct spi_master *master)
{
	struct s3c_spi_info *info = spi_master_get_devdata(master);      
	spi_unregister_master(master);    
	platform_device_del(info->pdev);    
	platform_device_put(info->pdev);    
	free_irq(info->irq, master);    
	iounmap((void *)info->reg_base);
}

static int spi_master_init(void)
{
	spi_control0 = create_spi_master_s3c2440(0, 0x59000000, SPI_IRQ0);
	spi_control1 = create_spi_master_s3c2440(1, 0x59000020, SPI_IRQ1);
	return 0;
}

static int spi_master_exit(void)
{
	destory_spi_master_s3c2440(spi_control0);
	destory_spi_master_s3c2440(spi_control1);
	return 0;
}

MODULE_LICENSE("GPL");
module_init(spi_master_init);
module_exit(spi_master_exit);
