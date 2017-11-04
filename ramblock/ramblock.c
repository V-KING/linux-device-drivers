#include <linux/module.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/genhd.h>
#include <linux/hdreg.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/blkdev.h>
#include <linux/blkpg.h>
#include <linux/delay.h>
#include <linux/io.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/dma.h>


#define RAMBLOCK_SIZE (1024*1024)
static struct gendisk *ramblock_disk;
static request_queue_t *ramblock_queue;

static int major;

static DEFINE_SPINLOCK(ramblock_lock);
static unsigned char *ramblock_buf;



static int ramblock_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	// 容量=heads*cylinders*sectors*512 
	geo->heads = 2;//磁头
	geo->cylinders = 32;//柱面
	geo->sectors = RAMBLOCK_SIZE/2/32/512;//扇区 一个扇区512字节
	return 0;
}
//块设备操作函数结构体
static struct block_device_operation ramblock_fops = {
	.owner = THIS_MODULE,
	.getgeo = ramblock_getgeo,
};
/*I/O调度器（也称电梯）的工作是以最优性能的方式向驱动提交I/O请求。大部分I/O 调度器累积批量的 I/O 请求，
 *并将它们排列为递增（或递减）的块索引顺序后提交给驱动。(I/O调度算法)进行这些工作的原因在于，对于磁头而言，
 *当给定顺序排列的请求时，可以使得磁盘顺序地从一头到另一头工作，非常像一个满载的电梯，
 *在一个方向移动直到所有它的“请求”已被满足。另外，I/O调度器还负责合并邻近的请求，
 *当一个新 I/O 请求被提交给调度器后，它会在队列里搜寻包含邻近扇区的请求；如果找到一个，
 *并且如果结果的请求不是太大，调度器将合并这2个请求。对磁盘等块设备进行I/O操作顺序的调度类似于电梯的原理，
 *先服务完上楼的乘客，再服务下楼的乘客效率会更高，而“上蹿下跳”，顺序响应用户的请求则会导致电梯无序地忙乱。*/
//处理IO请求的函数
static void do_ramblock_request(request_queue_t * q)
{
	static int r_cnt = 0;
	static int w_cnt = 0;
	/*在Linux块设备驱动中，使用 request 结构体来表征等待进行的I/O请求*/
	struct request *req;

	while((req = elv_next_request(q)) != NULL)
	{
		/* 数据传输的三要素 源，目的， 长度*/
		unsigned long offset = req->sector * 512;

		unsigned long len = req->current_nr_sectors * 512;
		if(rq_data_dir(req) == READ)
		{
			memcpy(req->buffer, ramblock_buf + offset, len);
		}
		else
		{
			memcpy(ramblock_buf+offset, req->buff, len);
		}

		end_request(req, 1);//结束这个请求 表示请求成功
	}
}
/*gendisk结构描述一个磁盘，包括主从设备号、设备操作函数、容量等信息，
 *它通过gendisk->queue和request_queue联系起来*/
static int ramblock_init(void)
{
	/* 分配一个gendisk结构体 */
	ramblock_disk = alloc_disk(16);
	/* 设置队列，提供对设备的读写能力 */
	//第1个参数是请求处理函数的指针，第2个参数是控制访问队列权限的自旋锁
	ramblock_quene = blk_init_quene(do_ramblock_request, &ramblock_lock);
	//建立磁盘设备与读写函数队列的联系
	ramblock_disk->quene = ramblock_quene;
	//注册块设备，自动分配主设备号，设备名为ramblock
	major = register_blkdev(0, "ramblock");
	//磁盘设备的主次设备号为major , 0
	ramblock_disk->major = major;
	ramblock_disk->minor = 0;
	//ramblock_disk->disk_name = ramblock
	sprintf(ramblock_disk->disk_name, "ramblock");
	//设置块设备操作函数
	ramblock_disk->fops = &ramblock_fops;
	//设置磁盘扇区为512字节
	set_capacity(ramblock_disk, RAMBLOCK_SIZE/512);
	//分配内存 这里使用内存模拟磁盘
	ramblock_buf = kzalloc(RAMBLOCK_SIZE, GFP_KERNEL);
	//将gendisk结构体注册到内核中
	add_disk(ramblock_disk);
	return 0;
}

static void ramblock_exit(void)
{
	unregister(major, "ramblock");
	//卸载磁盘
	del_gendisk(ramblock_disk);
	put_disk(ramblock_disk);
	//清除请求队列
	blk_cleanup_queue(ramblock_quene);
	kfree(ramblock_buf);
}

module_init(ramblock_init);
module_exit(ramblock_exit);

MODULE_LICENSE("GPL");