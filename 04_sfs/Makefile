DISK		= ./disk.img
DISK_SIZE	= 32
MKFS		= mkfs.sfs
C_INCLUDE_PATH	= ""

obj-m       := inode.o
KERNELDIR   ?= /lib/modules/$(shell uname -r)/build

all: $(DISK)
	make -C $(KERNELDIR) M=$(PWD) modules

$(MKFS): mkfs.sfs.c
	$(CC) -Wall -o $@ $<

$(DISK): $(MKFS)
	dd if=/dev/zero of=$(DISK) bs=1M count=$(DISK_SIZE)
	./$(MKFS) $(DISK)

clean:
	rm -f $(MKFS) *.o $(DISK)
	make -C $(KERNELDIR) M=$(PWD) clean
