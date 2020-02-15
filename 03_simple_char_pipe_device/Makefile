obj-m := simple_pipe.o

KERNELDIR ?= /home/eric/dev/kernel/kernels/linux-current

default:
	make -C $(KERNELDIR) M=$(PWD) modules

clean:
	make -C $(KERNELDIR) M=$(PWD) clean
