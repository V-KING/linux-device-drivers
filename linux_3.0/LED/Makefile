KERNEL_DIR = /home/kernel/iTop4412_Kernel_3.0

all:	
	make -C $(KERNEL_DIR) M=`pwd` modules
clean:
	make -C $(KERNEL_DIR) M=`pwd` modules clean
	rm -rf modules.order

obj-m += led_drv.o	
obj-m += led_dev.o		
