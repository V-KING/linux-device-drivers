#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb/input.h>
#include <linux/hid.h>
/*urb 请求块就像一个装东西的“袋子”，USB 驱动程序把“空袋子”提交给 USB core，然后再交给主控制器，主控制器
 *把数据放入这个“袋子”后再将装满数据的“袋子”通过 USB core 交还给 USB 驱动程序，这样一次数据传输就完成了
 *驱动程序得到数据后进行处理并向上上报事件usb_mouse_irq */

//为USB鼠标创建一个私有的数据结构
struct usb_mouse {
	char name[128];
	char phys[64];
	struct usb_device *usbdev;
	struct input_dev *dev;
	struct urb *irq;

	signed char *data;
	dma_addr_t data_dma;
};

static void usb_mouse_irq(struct urb *urb)
{
	struct usb_mouse *mouse = urb->context;
	signed char *data = mouse->data;
	struct input_dev *dev = mouse->dev;
	int status;
	//status=0 表示USB数据传输成功
	switch (urb->status) {
	case 0:			/* success 跳出switch 并上报事件 */
		break;
	case -ECONNRESET:	//-ECONNRESET 表示被usb_unlink_urb()杀死
	case -ENOENT:		//-ENOENT 表示被usb_kill_urb()杀死
	case -ESHUTDOWN:
		return;
	/* -EPIPE:  should clear the halt */
	default:		/* error */
		goto resubmit;
	}
	//上报鼠标按键事件
	input_report_key(dev, BTN_LEFT,   data[0] & 0x01);
	input_report_key(dev, BTN_RIGHT,  data[0] & 0x02);
	input_report_key(dev, BTN_MIDDLE, data[0] & 0x04);
	input_report_key(dev, BTN_SIDE,   data[0] & 0x08);
	input_report_key(dev, BTN_EXTRA,  data[0] & 0x10);
	//上报鼠标的位移和滚轮
	input_report_rel(dev, REL_X,     data[1]);
	input_report_rel(dev, REL_Y,     data[2]);
	input_report_rel(dev, REL_WHEEL, data[3]);

	input_sync(dev);//事件上报结束
resubmit:
	status = usb_submit_urb (urb, GFP_ATOMIC);
	if (status)
		err ("can't resubmit intr, %s-%s/input0, status %d",
				mouse->usbdev->bus->bus_name,
				mouse->usbdev->devpath, status);
}

static int usb_mouse_open(struct input_dev *dev)
{
	//得到probe中定义的局部变量dev结构体
	//input_set_drvdata(input_dev, mouse); probe通过这个函数保存，用get获取
	struct usb_mouse *mouse = input_get_drvdata(dev);

	mouse->irq->dev = mouse->usbdev;
	if(usb_submit_urb(mouse->irq, GFP_KERNEL))
		return -EIO;

	return 0;
}

static void usb_mouse_close(struct input_dev *dev)
{
	struct usb_mouse *mouse = input_get_drvdata(dev);
	usb_kill_urb(mouse->irq);
}


/* 比较设备中的接口信息和USB 驱动程序中的 id_table，来初步决定该 USB 驱动程序是不是跟相应接口相匹配。
 * 通过这一道关卡后，USB core 会认为这个设备应该由这个驱动程序负责。然而，仅仅这一步是不够的
 * 接着，将会调用 USB 驱动程序中的 probe 函数对相应接口进行进一步检查。验证通过后 probe进行初始化*/
static int usb_mouse_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	//获取usb接口对应的usb设备
	struct usb_device *dev = interface_to_usbdev(intf);
	struct usb_host_interface *interface;
	struct usb_endpoint_descriptor * endpoint;
	//存放usb鼠标的设备信息， 自定义的一个结构体
	struct usb_mouse *mouse;
	struct input_dev *input_dev;
	//pipe表示接口与端点间的管道
	//maxp用于存放鼠标的数据
	int pipe, maxp;
	int error = -ENOMEM;
	////获取usb接口结构体中的usb host接口结构体
	interface = intf->cur_altsetting;

	if(interface->desc.bNumEndpoints != 1)
		return -ENOMEM;
	//获取usb host接口结构体中的端点描述结构体
	endpoint = &interface->endpoint[0].desc;
	if(!usb_endpoint_is_int_in(endpoint))
		return -ENOMEM;
	//建立中断输入端点
	pipe = usb_rcvintpipe(dev, endpoint->bEndpointAddress);
	//获取返回字节大小
	maxp = usb_maxpacket(dev, pipe, usb_pipeout(pipe));
	//分配usb_mouse结构体的私有数据空间
	mouse = kzalloc(sizeof(struct usb_mouse), GFP_KERNEL);

	input_dev = input_allocate_device();
	if(!mouse || !input_dev)
		goto fail1;
	//申请USB数据传输的目的空间
	mouse->data = usb_buffer_alloc(dev, 8, GFP_AYOMIC, &mouse->data_dma);
	if(!mouse->data)
		goto fail1;
	//urb(usb request block)的创建是由usb_alloc_urb()完成的.这个函数会完成URB内存的分配和基本成员的初始化工作.
	mouse->irq = usb_alloc_urb(0, GFP_KERNEL);
	if(!mouse->irq)
		goto fail2;

	mouse->usbdev = dev;
	mouse->dev = input_dev;

	if(dev->manufacturer)
		strlcpy(mouse->name, dev->manufacturer, sizeof(mouse->name));

	if (dev->product) 
	{
		if (dev->manufacturer)
			//第二个参数连接到第一个参数结尾处
			strlcat(mouse->name, " ", sizeof(mouse->name));
		strlcat(mouse->name, dev->product, sizeof(mouse->name));
	}

	if (!strlen(mouse->name))
		// 按照USB HIDBP Mouse %04x:%04x格式格式化其后面的可变参数
		//le16_to_cpu(dev->descriptor.idVendor),le16_to_cpu(dev->descriptor.idProduct)
		//并将其复制到mouse->name中
		snprintf(mouse->name, sizeof(mouse->name),"USB HIDBP Mouse %04x:%04x",
			     le16_to_cpu(dev->descriptor.idVendor),le16_to_cpu(dev->descriptor.idProduct));

	usb_make_path(dev, mouse->phys, sizeof(mouse->phys));
	strlcat(mouse->phys, "/input0", sizeof(mouse->phys));

	//输入设备的名字设置成usb鼠标的名字
	input_dev->name = mouse->name;
	//输入设备的路径设置成usb鼠标的路径
	input_dev->phys = mouse->phys;
	//设置输入设备的信息bustype,vendor,product,version
	usb_to_input_id(dev, &input_dev->id);
	input_dev->dev.parent = &intf->dev;
	//支持的动作类型为按键动作和相对坐标事件
	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REL);
	//按键类型是左， 中， 右
	input_dev->keybit[BIT_WORD(BTN_MOUSE)] = BIT_MASK(BTN_LEFT) | BIT_MASK(BTN_RIGHT) | BIT_MASK(BTN_MIDDLE);
	//鼠标相对坐标位图
	input_dev->relbit[0] = BIT_MASK(REL_X) | BIT_MASK(REL_Y);
	//支持鼠标侧键和额外的按键
	input_dev->keybit[BIT_WORD(BTN_MOUSE)] |= BIT_MASK(BTN_SIDE) | BIT_MASK(BTN_EXTRA);
	//支持鼠标的滚轮
	input_dev->relbit[0] |= BIT_MASK(REL_WHEEL);

	input_set_drvdata(input_dev, mouse);

	input_dev->open = usb_mouse_open;
	input_dev->close = usb_mouse_close;
	//初始化，被安排给一个特定USB设备的特定端点
	usb_fill_int_urb(mouse->irq, dev, pipe, mouse->data, (maxp > 8 ? 8 : maxp),
		        	 usb_mouse_irq, mouse, endpoint->bInterval);
	mouse->irq->transfer_dma = mouse->data_dma;
	//不使用DMA数据传输
	mouse->irq->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
	//注册input输入设备
	error = input_register_device(mouse->dev);
	if (error)
		goto fail3;
	/*usb_set_intfdata(struct usb_interface *intf, void *data)函数,
	 *该函数中的第一个参数就是的驱动要支持的那个设备接口数据结构的指针,
	 *第二个参数是该驱动为了实现接口正常运行而分配的自己的数据结构。*/
	//usb_set_intfdata()的作用就是把接口intf和它的驱动要用到的数据结构mouse关联起来
	usb_set_intfdata(intf, mouse);
	return 0;

fail3:	
	usb_free_urb(mouse->irq);
fail2:	
	usb_buffer_free(dev, 8, mouse->data, mouse->data_dma);
fail1:	
	input_free_device(input_dev);
	kfree(mouse);
	return error;

}

static void usbmouse_disconnect (struct usb_interface *intf)
{
	struct usbmouse *mouse = usb_get_intfdata(intf);
	//将关联的私有数据结构取消
	usb_set_intfdata(intf, NULL);
	if(mouse)
	{
		usb_kill_urb(mouse->irq);
		input_unregister_device(mouse->dev);
		usb_free_urb(mouse->irq);
		usb_buffer_free(interface_to_usbdev(intf), 8, mouse->data, mouse->data_dma);
		kfree(mouse);
	}
}
//设备表存放设备信息
static struct usb_device_id usbmouse_id_table [] = {
	{ USB_INTERFACE_INFO(USB_INTERFACE_CLASS_HID, USB_INTERFACE_SUBCLASS_BOOT,
		USB_INTERFACE_PROTOCOL_MOUSE) },
	{ }	//设备表最后一个元素为空，用于标识结束
};

//一般用于支持热插拔的设备驱动程序中
/* 该宏生成一个名为__mod_pci_device_table的局部变量，该变量指向第二个参数。内核构建时，
 * depmod程序会在所有模块中搜索符号__mod_pci_device_table，把数据（设备列表）从模块中抽出，
 * 添加到映射文件/lib/modules/KERNEL_VERSION/modules.pcimap中，当depmod结束之后，
 * 所有的PCI设备连同他们的模块名字都被该文件列出。
 * 当内核告知热插拔系统一个新的PCI设备被发现时，热插拔系统使用modules.pcimap文件来找寻恰当的驱动程序。  */
MODULE_DEVICE_TABLE(usb, usbmouse_id_table);

static struct usb_driver usbmouse_driver = {
	.name = "usbmouse",
	.probe = usb_mouse_probe,
	.disconnect = usbmouse_disconnect,
	.id_table   = usbmouse_id_table, 	
};


static int __init usbmouse_init(void)
{
	//注册一个usb驱动
	usb_register(&usbmouse_driver);
	return 0;
}


static void usbmouse_exit(void)
{
	//注销一个usb驱动
	usb_deregister(&usbmouse_driver);
}

module_init(usbmouse_init);
module_exit(usbmouse_exit);

MODULE_LICENSE("GPL");