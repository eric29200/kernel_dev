obj-m := simple.o

KERNELDIR ?= /home/eric/dev/kernel/kernels/linux-current

default:
	make -C $(KERNELDIR) M=$(PWD) modules
	gcc -o ioctl_test ioctl_test.c

clean:
	make -C $(KERNELDIR) M=$(PWD) clean
	rm -f ioctl_test
