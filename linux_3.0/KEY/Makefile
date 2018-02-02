KERN_DIR = /home/kernel/iTop4412_Kernel_3.0

all:
	make -C $(KERN_DIR) M=`pwd` modules
clean:
	make -C $(KERN_DIR) M=`pwd` modules clean
	rm -rf modules.order	

obj-m += key_drv.o


