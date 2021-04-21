DISK				= ./disk.img
DISK_SIZE	 = 32
MKFS				= mkfs.sfs

all: $(DISK)

$(MKFS): mkfs.sfs.c
	$(CC) -Wall -o $@ $<

$(DISK): $(MKFS)
	dd if=/dev/zero of=$(DISK) bs=1M count=$(DISK_SIZE)
	./$(MKFS) $(DISK)

clean:
	rm -f $(MKFS) *.o $(DISK)
