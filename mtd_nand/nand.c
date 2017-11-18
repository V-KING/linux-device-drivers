#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linuc/delay.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/clk.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>
 
#include <asm/io.h>
 
#include <asm/arch/regs-nand.h>
#include <asm/arch/nand.h>

struct s3c_nand_regs {
	unsigned long nfconf  ;
	unsigned long nfcont  ;
	unsigned long nfcmd   ;
	unsigned long nfaddr  ;
	unsigned long nfdata  ;
	unsigned long nfeccd0 ;
	unsigned long nfeccd1 ;
	unsigned long nfeccd  ;
	unsigned long nfstat  ;
	unsigned long nfestat0;
	unsigned long nfestat1;
	unsigned long nfmecc0 ;
	unsigned long nfmecc1 ;
	unsigned long nfsecc  ;
	unsigned long nfsblk  ;
	unsigned long nfeblk  ;
};

static struct s3c_nand_regs *s3c_nand_regs;
/* mtd_info 结构体表示块设备的共有抽象 */
static struct mtd_info *s3c_mtd;
/* nand_chip 表示具体一个芯片的特征 */
static struct nand_chip *s3c_nand_chip;

static struct mtd_partition s3c_nand_parts[] = {
	[0] = {
		.name 	= "bootloader",
		.size 	= 0x00040000,
		.offset = 0,
	},

	[1] = {
		.name	= "params",
		.size   = 0x00020000,
		.offset = MTDPART_OFS_APPEND,
	}

	[2] = {
		.name	= "kernel",
		.size	= 0x00200000,
		.offset = MTDPART_OFS_APPEND,
	}

	[3] = {
		.name	= "root",
		.size	= MTDPART_SIZ_FULL,
		.offset = MTDPART_OFS_APPEND,
	}
};

static void s3c2440_select_chip(struct mtd_info *mtd, int chipnr)
{
	if(chipnr == -1)	
	{
		/* 取消选中:  */
		s3c_nand_regs->nfcont |= (1<<1);
	}	

	else
	{
		/* 选中 */
		s3c_nand_regs->nfcont |= (1<<0);
	}	
}


/* 发送的是命令还是地址 */
static void s3c2440_cmd_ctrl(struct mtd_info *mtd, int dat, unsigned int ctrl)
{
	if(ctrl & NAND_CLE)
	{
		/* 发送命令 */
		s3c_nand_regs->nfcmd = dat;
	}	

	else 
	{
		/* 发送地址 */
		s3c_nand_regs->nfaddr = dat;
	}	
}

static int s3c2440_dev_ready(struct mtd_info *mtd)
{
	return (s3c_nand_regs->nfstat & (1<<0));
}

static int s3c_nand_init(void)
{
	/* 时钟结构体用于设置时钟 */
	struct clk *clk;
	/* 分配nand_chip 结构体 */
	s3c_nand = kzalloc(sizeof(struct nand_chip), GFP_KERNEL);

	s3c_nand_regs = ioremap(0x4E000000, sizeof(struct s3c_nand_regs));

	/* 设置nand_chip */
	s3c_nand->select_chip = s3c2440_select_chip;
	s3c_nand->cmd_ctrl 	  = s3c2440_cmd_ctrl;
	s3c_nand->IO_ADDR_R   = &s3c_nand_regs->nfdata;
	s3c_nand->IO_ADDR_W   = &s3c_nand_regs->nfdata;
	s3c_nand->dev_ready   = s3c2440_dev_ready;
	s3c_nand->ecc.mode    = NAND_ECC_SOFT;

	/* NAND时钟设置 */
	clk = clk_get(NULL, "nand");
	clk_enable(clk);

	/*根据nand flash 与 s3c2440控制器手册设置时序参数*/
#define TACLS	0
#define TWRPH0	1
#define TWRPH1  0

	s3c_nand_regs->nfconf = (TACLS << 12) | (TWRPH0 << 8) | (TWRPH1 << 4);	

	/* NFCONT: 
	 * BIT1-设为1, 取消片选 
	 * BIT0-设为1, 使能NAND FLASH控制器
	 */
	s3c_nand_regs->nfcont = (1<<1) | (1<<0);

	s3c_mtd = kzalloc(sizeof(struct mtd_info), GFP_KERNEL);
	s3c_mtd->owner = THIS_MODULE;
	/* s3c_nand 是nand_chip 结构体
	 * s3c_mtd  是nand_info 结构体	*/
	s3c_mtd->priv  = s3c_nand; 
	/* 扫描设备是否存在 */
	nand_scan(s3c_mtd, 1);

	/* 3.4.2 内核中使用mtd_device_parse_register()设置分区
	 * Register the partitions 
	 * mtd_device_parse_register(s3c_mtd, NULL, NULL, s3c_nand_parts, 4);
	 */

	add_mtd_partition(s3c_mtd, s3c_nand_parts, 4);

	return 0;
}

static void s3c_nand_exit(void)
{
	del_mtd_partition(s3c_mtd, s3c_nand_parts, 4);
	kfree(s3c_mtd);
	iounmap(s3c_nand_regs);
	kfree(s3c_nand);
}

module_init(s3c_nand_init);
module_exit(s3c_nand_exit);

MODULE_LICENSE("GPL");