obj-m := simple.o

KERNELDIR ?= /lib/modules/$(shell uname -r)/build

default:
	make -C $(KERNELDIR) M=$(PWD) modules
	gcc -o ioctl_test ioctl_test.c

clean:
	make -C $(KERNELDIR) M=$(PWD) clean
	rm -f ioctl_test
