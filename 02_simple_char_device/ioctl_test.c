#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdlib.h>

#define SIMPLE_IOC_MAGIC    'k'
#define SIMPLE_IOCSSIZE     _IOW(SIMPLE_IOC_MAGIC,  1, int)

int main(int argc, char **argv)
{
	int fd;
	int ret = 0;
	int new_size = atoi(argv[1]);

	fd = open("/dev/simple", 0);
	if (fd == -1) {
		perror("open");
		ret = -1;
		goto out;
	}

	if (ioctl(fd, SIMPLE_IOCSSIZE, &new_size) == -1) {
		perror("ioctl");
		ret = -1;
		goto release;
	}

release:
	close(fd);
out:
	return ret;
}
