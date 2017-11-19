#include <linux/init.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/version.h>
#include <asm/dma.h>
#include <linux/spinlock.h>
#include <linux/crc32.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <asm/uaccess.h>


//声明一个网络设备结构体

struct net_device *dev = NULL;

//启动数据包的传输,传递参数skb,使得驱动程序能获取从上层传递下来的数据包

static int virtnet_start_xmit(struct sk_buf *skb, struct net_device *dev)

{

        printf("send %d bytes\n", skb->len);

        // 更改统计数据

        dev->stats.tx_packets++;

        dev->stats.tx_bytes += skb->len;

        return 0;

}

//入口函数

static int __init virtnet_init(void)
{
       // 1、分配一个维护网卡的结构

        dev = alloc_netdev(0, "virtnet%d", ether_setup);

        2、配置网卡特性

        dev->hard_start_xmit = &virtnet_start_xmit;

        /*MAC address*/

        dev->dev_addr[0] = 0x00;

        dev->dev_addr[0] = 0x12;

        dev->dev_addr[0] = 0x34;

        dev->dev_addr[0] = 0x56;

        dev->dev_addr[0] = 0x78;

        dev->dev_addr[0] = 0x9a;

        3、注册网卡设备

        register_netdev(dev);

        return 0;

}

static void __exit virtnet_exit(void)

{
        unregister_netdev(dev);
        free_netdev(dev);
}


module_init(virtnet_init);
module_exit(virtnet_exit);
MODULE_LICENSE("GPL");