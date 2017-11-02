//input输入子系统来写USB鼠标的输入事件
//驱动与设备分离的思想
//drivers\hid\usbhid\usbmouse.c

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb/input.h>
#include <linux/hid.h>


static struct input_dev *uk_dev;
static char *usb_buf;
static dma_addr_t usb_buf_phys;
static int len;
static struct urb *uk_urb;
//基类，子类和协议 的信息表示的是（Vendor ID，供应商识别码）和PID（Product ID，产品识别码）
static struct usb_device_id usbmouse_key_id_table [] = {
	{ USB_INTERFACE_INFO (USB_INTERFACE_CLASS_HID, USB_INTERFACE_SUBCLASS_BOOT,
		USB_INTERFACE_PROTOCOL_MOUSE) },
	{ }
};

//urb usb_fill_int_urb()由初始化完成时被调用的完成处理函数
//usb_fill_int_urb(uk_urb, dev, pipe, usb_buf, len, usbmouse_key_irq, NULL, endpoint->bInterval);
//urb->complete函数为 usbmouse_key_irq
static void usbmouse_key_irq(struct urb *urb)
{
	static unsigned char pre_val;//存储当前键值
	if((pre_val & (1<<0)) != (usb_buf[0] & (1<<0)))
	{
		//左键发生变化
		input_event(uk_dev, EV_KEY, KEY_L, (usb_buf[0] & (1<<0)) ? 1: 0);
		input_sync(uk_dev);
	}

	if((pre_val & (1<<1)) != (usb_buf[0] & (1<<1)))
	{
		//右键发生变化
		input_event(uk_dev, EV_KEY, KEY_S, (usb_buf[0] & (1<<1)) ? 1 : 0);
		input_sync(uk_dev);
	}

	if((pre_val & (1<<2)) != (usb_buf[0] & (1<<2)) ? 1: 0)
	{
		//中键发生变化
		input_event(uk_dev, EV_KEY, KEY_S, (usb_buf[0] & (1<<2)) ? 1 : 0);
		input_sync(uk_dev);
	}

	pre_val = usb_buf[0];

	//重新提交urb
	usb_submit_urb(uk_urb, GPF_KERNEL);

}
//probe函数对USB设备进行识别
//usb描述符，主要有四种usb描述符，设备描述符，配置描述符，接口描述符和端点描述符(第五种是字符串描述符)
//协议里规定一个usb设备是必须支持这四大描述符的
static int usbmouse_key_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	//interface_to_usbdev()函数获取usb接口对应的usb设备
	struct usb_device *dev = interface_to_usbdev(intf);
	//usb接口 包含所有可用于该接口的可选设置的接口结构数组
	struct usb_host_interface  * interface;
	//真实端点信息
	struct usb_endpoint_descriptor *endpoint;
	//管道 端点与接口之间的纽带
	int pipe;

	//从USB设备中获得接口
	interface = intf->cur_altsetting;
	//得到端点
	endpoint = &interface->endpoint[0].desc;

	//分配一个input_dev结构体  input输入子系统有关
	uk_dev = input_allocate_device();
	//产生那些类事件
	set_bit(EV_KEY, uk_dev->evbit);
	set_bit(EV_REP, uk_dev->evbit);
	//产生哪些时间
	set_bit(KEY_L, uk_dev->keybit);
	set_bit(KEY_S, uk_dev->keybit);
	set_bit(KEY_ENTER, uk_dev->keybit);

	//注册
	input_register_device(uk_dev);

	//硬件操作
	//数据传输需要 源，目的， 长度
	//源：USB设备的某个端点获得数据 bEndpointAddress定义于 结构体usb_endpoint_descriptor
	pipe = usb_rcvintpipe(dev, endpoint->bEndpointAddress);
	//长度  wMaxPacketSize定义于结构体usb_endpoint_descriptor
	len  = endpoint->wMaxPacketSize;
	//目的
/* usb_buffer_alloc()函数 第一个参数就是 struct usb_device 结构体的指针, 第二个参数申请的 buffer 的大小
 * 第三个参数 ,GFP_KERNEL, 是一个内存申请的 flag, 通常内存申请都用这个 flag, 除非是中断上下文 , 不能睡眠 ,
 * 那就得用 GPF_ATOMIC, 这里没那么多要求 .而 usb_buffer_alloc() 的第四个参数 涉及到 dma 传输 .该函数不仅进行
 * 内存分配，还会进行DMA映射，这里第四个参数将被设置为DMA地址。这个地址用于传输DMA缓冲区数据的urb。
 * 用 usb_buffer_alloc 申请的内存空间需要用它的搭档 usb_buffer_free() 来释放 . */
	usb_buf = usb_buffer_alloc(dev, len, GPF_ATOMIC, &usb_buf_phys); 
	//URB的创建是由usb_alloc_urb()完成的.这个函数会完成URB内存的分配和基本成员的初始化工作.
	uk_urb = usb_alloc_urb(0, GPF_KERNEL);
	//初始化，被安排给一个特定USB设备的特定端点。
	/* urb参数指向要被初始化的urb的指针；dev指向这个urb要被发送到的USB设备；
	 * pipe是这个urb要被发送到的USB设备的特定端点 transfer_buffer 是指向发送数据或接收数据的缓冲区的指针，
	 * 和urb一样，它也不能是静态缓冲区，必须使用kmalloc()来分配；buffer_length是transfer_buffer指针所指向
	 * 缓冲区的大小；complete指针指向当这个urb完成时被调用的完成处理函数；context是完成处理函数的“上下文”；
	 * interval是这个urb应当被调度的间隔。 */
	usb_fill_int_urb(uk_urb, dev, pipe, usb_buf, len, usbmouse_key_irq, NULL, endpoint->bInterval);

	//用于传输DMA缓冲区数据的urb
	uk_urb->transfer_dma = usb_buf_phys;
	//当transfer_flags 使用 URB_NO_TRANSFER_DMA_MAP 表示不使用DMA，数据放到transfer_buffer，即usb_buf
	uk_urb->transfer_flags |= USB_NO_TRANSFER_DMA_MAP;
	//usb_submit_urb把urb提交给主控制器
	//urb: usb request block    urb结构来传递数据，urb是usb通信基础。
	usb_submit_urb(uk_urb, GPF_KERNEL);
	return 0;
}

static void usbmouse_key_disconnect(struct usb_interface *intf)
{
	struct usb_device *dev = interface_to_usbdev(init);
	//usb_kill_urb()函数取消urb
	usb_kill_urb(uk_urb);
	//释放 urb
	usb_free_urb(uk_urb);

	usb_buffer_free(dev, len, usb_buf, usb_buf_phys);
	input_unregister_device(uk_dev);
	input_free_device(uk_dev);
}

static struct usb_driver usbmouse_key_driver = {
	.name = "usbmouse_key",
	.probe = usbmouse_key_probe,
	.disconnect = usbmouse_key_disconnect,
	.id_table = usbmouse_key_id_table,  //USB 设备的类，子类，鼠标
};

static int usbmouse_key_init(void)
{
	//注册usb驱动
	usb_register(&usbmouse_key_driver);
	return 0;
}

static void usbmouse_ket_exit(void)
{
	//注销USB驱动
	usb_deregister(&usbmouse_key_driver);
}


module_init(usbmouse_key_init);
module_init(usbmouse_ket_exit);

MODULE_LICENSE("GPL");