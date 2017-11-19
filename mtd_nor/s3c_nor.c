#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <asm/io.h>

/* 参考 drivers\mtd\maps\physmap.c */

/* nor flash器件的信息 */
static struct map_info *s3c_nor_map;
/* 块设备mtd输入子系统的信息 */
static struct mtd_info *s3c_nor_mtd;

/* nor 的分区信息 */
static struct mtd_partition s3c_nor_parts[] = {
	[0] = {
		.name 	 = "bootloader_nor",
		.size 	 = 0x00040000,
		.offfset = 0,
	},

	[1] = {
		.name	 = "root_nor",
		.size	 = MTDPART_SIZ_FULL,
		.offset  = MTDPART_OFS_APPEND,
	}
};

static int s3c2440_nor_init(void)
{
	/* 分配map_info结构体 */
	s3c_nor_map = kzalloc(sizeof(struct map_info), GFP_KERNEL);

	/* 设置: 物理基地址(phys), 大小(size), 位宽(bankwidth), 虚拟基地址(virt) */
	s3c_nor_map->name 		= "s3c_nor";
	s3c_nor_map->phys 		= 0;
	s3c_nor_map->size 		= 0x1000000;
	s3c_nor_map->bankwidth  = 2;
	s3c_nor_map->virt 		= ioremap(s3c_nor_map->phys, s3c_nor_map->size); 

	simple_map_init(s3c_nor_map);

	/* 调用NOR FLASH协议层提供的函数来识别是cfi接口还是jedec接口 */

	s3c_nor_mtd = do_map_probe("cfi_probe", s3c_nor_map);
	if(s3c_nor_mtd == NULL)
	{
		s3c_nor_mtd = do_map_probe("jedec_probe", s3c_nor_map);
	}	

	if(s3c_nor_mtd == NULL)
	{
		iounmap(s3c_nor_map->virt );
		kfree(s3c_nor_map);
		return -EIO;
	}	
	/* 添加分区 */
	add_mtd_partition(s3c_nor_mtd ,s3c_nor_parts, 2);
	return 0;
}

static void s3c2440_nor_exit(void)
{
	del_mtd_partition(s3c_nor_mtd);
	iounmap(s3c_nor_map->virt);
	kfree(s3c_nor_map);
}

module_init(s3c2440_nor_init);
module_exit(s3c2440_nor_exit);

MODULE_LICENSE("GPL");