#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb/input.h>
#include <linux/hid.h>

static const unsigned char usb_kbd_keycode[256] = {
	  0,  0,  0,  0, 30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38,
	 50, 49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44,  2,  3,
	  4,  5,  6,  7,  8,  9, 10, 11, 28,  1, 14, 15, 57, 12, 13, 26,
	 27, 43, 43, 39, 40, 41, 51, 52, 53, 58, 59, 60, 61, 62, 63, 64,
	 65, 66, 67, 68, 87, 88, 99, 70,119,110,102,104,111,107,109,106,
	105,108,103, 69, 98, 55, 74, 78, 96, 79, 80, 81, 75, 76, 77, 71,
	 72, 73, 82, 83, 86,127,116,117,183,184,185,186,187,188,189,190,
	191,192,193,194,134,138,130,132,128,129,131,137,133,135,136,113,
	115,114,  0,  0,  0,121,  0, 89, 93,124, 92, 94, 95,  0,  0,  0,
	122,123, 90, 91, 85,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 29, 42, 56,125, 97, 54,100,126,164,166,165,163,161,115,114,113,
	150,158,159,128,136,177,178,176,142,152,173,140
};

struct usb_kbd {
	struct input_dev *dev;
	struct usb_device *usbdev;
	unsigned char old[8];  /*按键离开时所用之数据缓冲区*/ 
	struct urb *irq, *led;
	unsigned char newleds; /*目标指定灯状态*/ 
	char name[128];
	char phys[64];

	unsigned char *new;	//按键按下时所用之数据缓冲区
	struct usb_ctrlrequest *cr;
	unsigned char *leds;   /*当前指示灯状态*/ 
	dma_addr_t cr_dma;
	dma_addr_t new_dma;
	dma_addr_t leds_dma;
};
//urb数据传输完成后触发这个中断
static void usb_kbd_irq(struct urb *urb)
{
	struct usb_kbd *kbd = urb->context;
	int i;

	switch (urb->status) 
	{
	case 0:			/* success */
		break;
	case -ECONNRESET:	/* unlink */
	case -ENOENT:
	case -ESHUTDOWN:
		return;
	/* -EPIPE:  should clear the halt */
	default:		/* error */
		goto resubmit;
	}

	for (i = 0; i < 8; i++)//不知为嘛写这个
		input_report_key(kbd->dev, usb_kbd_keycode[i + 224], (kbd->new[0] >> i) & 1);

	/*若同时只按下1个按键则在第[2]个字节,若同时有两个按键则第二个在第[3]字节，类推最多可有6个按键同时按下*/
	for (i = 2; i < 8; i++) 
	{

		/* 得到按键离开的中断，同时没有该key按下的状态 */
		if (kbd->old[i] > 3 && memscan(kbd->new + 2, kbd->old[i], 6) == kbd->new + 8) 
		{
			if (usb_kbd_keycode[kbd->old[i]])
				input_report_key(kbd->dev, usb_kbd_keycode[kbd->old[i]], 0);
			else
				dev_info(&urb->dev->dev,
						"Unknown key (scancode %#x) released.\n", kbd->old[i]);
		}
		/* 得到按键按下的中断，同时没有该key离开的状态 */
		if (kbd->new[i] > 3 && memscan(kbd->old + 2, kbd->new[i], 6) == kbd->old + 8) 
		{
			if (usb_kbd_keycode[kbd->new[i]]) //判断数组中是否有那个键值
				input_report_key(kbd->dev, usb_kbd_keycode[kbd->new[i]], 1);
			else
				dev_info(&urb->dev->dev,
						"Unknown key (scancode %#x) released.\n", kbd->new[i]);
		}
	}

	input_sync(kbd->dev);

	memcpy(kbd->old, kbd->new, 8);

resubmit:
	i = usb_submit_urb (urb, GFP_ATOMIC);
	if (i)
		err_hid ("can't resubmit intr, %s-%s/input0, status %d",
				kbd->usbdev->bus->bus_name,
				kbd->usbdev->devpath, i);
}
//事件处理函数
static int usb_kbd_event(struct input_dev *dev, unsigned int type,
			 unsigned int code, int value)
{
	struct usb_kbd *kbd = input_get_drvdata(dev);

	if (type != EV_LED)//不支持LED事件
		return -1;
	//获取指示灯的目标状态
	kbd->newleds = (!!test_bit(LED_KANA,    dev->led) << 3) | (!!test_bit(LED_COMPOSE, dev->led) << 3) |
		       (!!test_bit(LED_SCROLLL, dev->led) << 2) | (!!test_bit(LED_CAPSL,   dev->led) << 1) |
		       (!!test_bit(LED_NUML,    dev->led));

	if (kbd->led->status == -EINPROGRESS)
		return 0;
	//指示灯的状态已经是目标状态时，就什么都不做
	if (*(kbd->leds) == kbd->newleds)
		return 0;

	*(kbd->leds) = kbd->newleds;
	kbd->led->dev = kbd->usbdev;
	if (usb_submit_urb(kbd->led, GFP_ATOMIC))
		err_hid("usb_submit_urb(leds) failed");

	return 0;
}
//只有上报LED类等事件的时候才会触发dev->event  可是并没有上报LED类事件
/*接在event之后操作，该功能其实usb_kbd_event中已经有了，
 *该函数的作用可能是防止event的操作失败，一般注释掉该函数中的所有行都可以正常工作*/ 
static void usb_kbd_led(struct urb *urb)
{
	struct usb_kbd *kbd = urb->context;

	if (urb->status)
		dev_warn(&urb->dev->dev, "led urb status %d received\n",
			 urb->status);

	if (*(kbd->leds) == kbd->newleds)
		return;

	*(kbd->leds) = kbd->newleds;
	kbd->led->dev = kbd->usbdev;
	if (usb_submit_urb(kbd->led, GFP_ATOMIC))
		err_hid("usb_submit_urb(leds) failed");
}

/*打开键盘设备时，开始提交在 probe 函数中构建的 urb，进入 urb 周期。 */ 
static int usb_kbd_open(struct input_dev *dev)
{
	struct usb_kbd *kbd = input_get_drvdata(dev);

	kbd->irq->dev = kbd->usbdev;
	if (usb_submit_urb(kbd->irq, GFP_KERNEL))
		return -EIO;

	return 0;
}
/*关闭键盘设备时，结束 urb 生命周期。 */ 
static void usb_kbd_close(struct input_dev *dev)
{
	struct usb_kbd *kbd = input_get_drvdata(dev);

	usb_kill_urb(kbd->irq);
}

//封装一个函数去申请内存
/*创建URB 分配URB内存空间即创建URB*/ 
static int usb_kbd_alloc_mem(struct usb_device *dev, struct usb_kbd *kbd)
{
	if (!(kbd->irq = usb_alloc_urb(0, GFP_KERNEL)))
		return -1;
	if (!(kbd->led = usb_alloc_urb(0, GFP_KERNEL)))
		return -1;
	if (!(kbd->new = usb_buffer_alloc(dev, 8, GFP_ATOMIC, &kbd->new_dma)))
		return -1;
	if (!(kbd->cr = usb_buffer_alloc(dev, sizeof(struct usb_ctrlrequest), GFP_ATOMIC, &kbd->cr_dma)))
		return -1;
	if (!(kbd->leds = usb_buffer_alloc(dev, 1, GFP_ATOMIC, &kbd->leds_dma)))
		return -1;

	return 0;
}

//封装一个函数去释放申请的内存
/*销毁URB 释放URB内存空间即销毁URB*/ 
static void usb_kbd_free_mem(struct usb_device *dev, struct usb_kbd *kbd)
{
	usb_free_urb(kbd->irq);
	usb_free_urb(kbd->led);
	usb_buffer_free(dev, 8, kbd->new, kbd->new_dma);
	usb_buffer_free(dev, sizeof(struct usb_ctrlrequest), kbd->cr, kbd->cr_dma);
	usb_buffer_free(dev, 1, kbd->leds, kbd->leds_dma);
}


static int usb_kbd_probe(struct usb_interface *iface, const struct usb_device_id *id)
{
	//获取usb接口对应的usb设备
	struct usb_device *dev = interface_to_usbdev(iface);
	//用于该接口的可选设置的接口结构数组
	struct usb_host_interface *interface;
	//真实端点信息
	struct usb_endpoint_descriptor *endpoint;
	//键盘这个设备的私有结构体
	struct usb_kbd *kbd;
	struct input_dev *input_dev;
	int i, pipe, maxp;
	int error = -ENOMEM;
	//获取接口的当前设置
	interface = iface->cur_altsetting;
	//判断接口的端点数是不是1
	if(interface->desc.bNumEndpoints != 1)
		return -ENODEV;
	//获取端点
	endpoint = &interface->endpoint[0].desc;
	//usb_endpoint_is_int_in() 判断端点是否为中断输入端点
	if(!usb_endpoint_is_int_in(endpoint))
		return -ENODEV;

	//建立接口与端点之间的管道
	pipe = usb_rcvintpipe(dev, endpoint->bEndpointAddress);

	//返回该端点能够传输的最大的包长度  字节
	maxp = usb_maxpacket(dev, pipe, usb_pipeout(pipe));
	//为键盘的私有结构体分配内存
	kbd = kzalloc(sizeof(struct usb_kbd), GFP_KERNEL);
	//申请input_dev内存空间
	input_dev = input_allocate_device();
	if(!kbd || !input_dev)
		goto fail1;
	//usb_kbd_alloc_mem()分配urb空间和其他缓冲空间
	if(usb_kbd_alloc_mem(dev, kbd))
		goto fail2;
	//都赋值给 kbd 这个私有结构体
	kbd->usbdev = dev;
	kbd->dev = input_dev;

	if(dev->manufacturer)
		strlcpy(kbd->name, dev->product, sizeof(kbd->name));

	if(dev->product)
	{
		if(dev->manufacturer)
			strlcat(kbd->name, " ", sizeof(kbd->name));
		strlcat(kbd->name, dev->product, sizeof(kbd->name));
	}


	if(!strlen(kbd->name))
		snprintf(kbd->name, sizeof(kbd->name), "USB HIDBP Keyboard %04x:%04x",
			 le16_to_cpu(dev->descriptor.idVendor),le16_to_cpu(dev->descriptor.idProduct));

	usb_make_patch(dev, kbd->phys, sizeof(kbd->phys));
	strlcpy (kbd->phys, "/input0", sizeof(kbd->phys));

	input_dev->name = kbd->name;	//设备名
	input_dev->phys = kbd->phys;  //设备节点名称
	usb_to_input_id(dev, &input_dev->id);

	input_dev->dev.parent = &iface->dev;
	//得到probe中定义的局部变量dev结构体
	//input_set_drvdata(input_dev, mouse); probe通过这个函数保存，用get获取
	input_set_drvdata(input_dev, kbd);
	//按键类， LED灯， 重复上报
	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_LED) | BIT_MASK(EV_REP);
	input_dev->ledbit[0] = BIT_MASK(LED_NUML) | BIT_MASK(LED_CAPSL) | BIT_MASK(LED_SCROLLL) | 
						   BIT_MASK(LED_COMPOSE) | BIT_MASK(LED_KANA);

	for(i=0; i<255; i++)
	{	//按键类型的键值定义在usb_kbd_keycode[]数组中
		set_bit(usb_kbd_keycode[i], input_dev->keybit);
	}	

	clear_bit(0, input_dev->keybit);
	/* 对于LED类型的事件，首先会调用到dev->event 然后再调用事件处理层的event */  
	input_dev->event = usb_kbd_event;//注册事件处理函数
	input_dev->open = usb_kbd_open;//注册设备打开函数入口
	input_dev->close = usb_kbd_close;//注册设备关闭函数出口
	//urb 中断传输
	usb_fill_int_urb(kbd->irq, dev, pipe, kbd->new, (maxp > 8 ? 8 : maxp),
						 usb_kbd_irq, kbd, endpoint->bInterval);

	kbd->irq->transfer_dma = kbd->new_dma;
	//不使用DMA传输数据
	kbd->irq->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	kbd->cr->bRequestType = USB_TYPE_CLASS | USB_RECIP_INTERFACE;
	kbd->cr->bRequest = 0x09;
	kbd->cr->wValue = cpu_to_le16(0x200);
	kbd->cr->wIndex = cpu_to_le16(interface->desc.bInterfaceNumber);
	kbd->cr->wLength = cpu_to_le16(1);
	//urb的控制传输方式 控制urb
	usb_fill_control_urb(kbd->led, dev, usb_sndctrlpipe(dev, 0),
			     (void *) kbd->cr, kbd->leds, 1,
			     usb_kbd_led, kbd);
	kbd->led->setup_dma = kbd->cr_dma;
	kbd->led->transfer_dma = kbd->leds_dma;
	kbd->led->transfer_flags |= (URB_NO_TRANSFER_DMA_MAP | URB_NO_SETUP_DMA_MAP);

	error = input_register_device(kbd->dev);
	if (error)
		goto fail2;
//usb接口的数据与kbd的私有数据关联
	usb_set_intfdata(iface, kbd);
	return 0;

fail2:	
	usb_kbd_free_mem(dev, kbd);
fail1:	
	input_free_device(input_dev);
	kfree(kbd);
	return error;
}

static void usb_kbd_disconnect(struct usb_interface *intf)
{
	struct usb_kbd *kbd = usb_get_intfdata (intf);
	//取消与kbd私有数据的联系
	usb_set_intfdata(intf, NULL);
	if (kbd) 
	{
		usb_kill_urb(kbd->irq);
		input_unregister_device(kbd->dev);
		usb_kbd_free_mem(interface_to_usbdev(intf), kbd);
		kfree(kbd);
	}
}
//USB键盘设备信息
static struct usb_device_id usb_kbd_id_table [] = {
	{ USB_INTERFACE_INFO(USB_INTERFACE_CLASS_HID, USB_INTERFACE_SUBCLASS_BOOT,
		USB_INTERFACE_PROTOCOL_KEYBOARD ) },
	{ } 
};

MODULE_DEVICE_TABLE (usb, usb_kbd_id_table);

static struct usb_driver usb_kbd_driver = {
	.name = "usbkbd",
	.probe = usb_kbd_probe,
	.disconnect = usb_kbd_disconnect,
	.id_table = usb_kbd_id_table, 
};

static int __init usbkbd_init(void)
{
	usb_register(&usb_kbd_driver);
	return 0;
}

static void __exit usbkbd_exit(void)
{
	usb_deregister(&usb_kbd_driver);
}


module_init(usbkbd_init);
module_exit(usbkbd_exit);

MODULE_LICENSE("GPL");	