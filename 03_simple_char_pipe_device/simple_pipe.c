#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

MODULE_AUTHOR("eric");
MODULE_LICENSE("GPL");

#define SIMPLE_PIPE_NAME		"simple_pipe"
#define SIMPLE_PIPE_MAJOR		0
#define SIMPLE_PIPE_MINOR		0
#define SIMPLE_PIPE_SIZE		2048

/* simple pipe structure */
struct simple_pipe {
	wait_queue_head_t inq, outq;
	char *buffer, *end;
	int buffersize;
	char *rp, *wp;
	int nreaders, nwriters;
	struct semaphore sem;
	struct cdev cdev;
};

/* device */
int simple_pipe_major;
struct simple_pipe simple_pipe_dev;

/* prototypes */
int simple_pipe_open(struct inode *, struct file *);
int simple_pipe_release(struct inode *, struct file *);
ssize_t simple_pipe_read(struct file *, char __user *, size_t, loff_t *);
ssize_t simple_pipe_write(struct file *, const char __user *, size_t, loff_t *);
int simple_pipe_init_module(void);
void simple_pipe_exit_module(void);

/* file operations structure */
struct file_operations simple_pipe_fops = {
	.owner				= THIS_MODULE,
	.open				= simple_pipe_open,
	.release			= simple_pipe_release,
	.read				= simple_pipe_read,
	.write				= simple_pipe_write,
};

/*
 * Open the device.
 */
int simple_pipe_open(struct inode *inode, struct file *filp)
{
	if (down_interruptible(&simple_pipe_dev.sem))
		return -ERESTARTSYS;

	if (!simple_pipe_dev.buffer) {
		simple_pipe_dev.buffer = kmalloc(simple_pipe_dev.buffersize,
						 GFP_KERNEL);
		if (!simple_pipe_dev.buffer) {
			up(&simple_pipe_dev.sem);
			return -ENOMEM;
		}
	}
	simple_pipe_dev.end = simple_pipe_dev.buffer
		+ simple_pipe_dev.buffersize;
	simple_pipe_dev.rp = simple_pipe_dev.wp = simple_pipe_dev.buffer;

	if (filp->f_mode & FMODE_READ)
		simple_pipe_dev.nreaders++;
	if (filp->f_mode & FMODE_WRITE)
		simple_pipe_dev.nwriters++;
	up(&simple_pipe_dev.sem);

	return nonseekable_open(inode, filp);
}

/*
 * Close the device.
 */
int simple_pipe_release(struct inode *inode, struct file *filp)
{
	down(&simple_pipe_dev.sem);
	if (filp->f_mode & FMODE_READ)
		simple_pipe_dev.nreaders--;
	if (filp->f_mode & FMODE_WRITE)
		simple_pipe_dev.nwriters--;
	if (simple_pipe_dev.nreaders + simple_pipe_dev.nwriters == 0) {
		kfree(simple_pipe_dev.buffer);
		simple_pipe_dev.buffer = NULL;
	}

	up(&simple_pipe_dev.sem);
	return 0;
}

/*
 * Read the device.
 */
ssize_t simple_pipe_read(struct file *filp, char __user *buf, size_t count,
			 loff_t *f_pos)
{
	if (down_interruptible(&simple_pipe_dev.sem))
		return -ERESTARTSYS;

	while(simple_pipe_dev.rp == simple_pipe_dev.wp) {
		up(&simple_pipe_dev.sem);
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		if (wait_event_interruptible(simple_pipe_dev.inq,
					     (simple_pipe_dev.rp
					      != simple_pipe_dev.wp)))
			return -ERESTARTSYS;
		if (down_interruptible(&simple_pipe_dev.sem))
			return -ERESTARTSYS;
	}

	if (simple_pipe_dev.wp > simple_pipe_dev.rp)
		count = min(count,
			    (size_t) (simple_pipe_dev.wp - simple_pipe_dev.rp));
	else
		count = min(count, (size_t)
			    (simple_pipe_dev.end - simple_pipe_dev.rp));
	if (copy_to_user(buf, simple_pipe_dev.rp, count)) {
		up(&simple_pipe_dev.sem);
		return -EFAULT;
	}
	simple_pipe_dev.rp += count;
	if (simple_pipe_dev.rp == simple_pipe_dev.end)
		simple_pipe_dev.rp = simple_pipe_dev.buffer;
	up(&simple_pipe_dev.sem);

	wake_up_interruptible(&simple_pipe_dev.outq);

	return count;
}

static int simple_pipe_spacefree(void)
{
	if (simple_pipe_dev.rp == simple_pipe_dev.wp)
		return simple_pipe_dev.buffersize - 1;
	return ((simple_pipe_dev.rp + simple_pipe_dev.buffersize
		 - simple_pipe_dev.wp) % simple_pipe_dev.buffersize) - 1;
}

/*
 * Write the device.
 */
ssize_t simple_pipe_write(struct file *filp, const char __user *buf,
			  size_t count, loff_t *f_pos)
{
	if (down_interruptible(&simple_pipe_dev.sem))
		return -ERESTARTSYS;

	while(simple_pipe_spacefree() == 0) {
		up(&simple_pipe_dev.sem);
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		if (wait_event_interruptible(simple_pipe_dev.outq,
					     (simple_pipe_spacefree() != 0)))
			return -ERESTARTSYS;
		if (down_interruptible(&simple_pipe_dev.sem))
			return -ERESTARTSYS;
	}

	count = min(count, (size_t) simple_pipe_spacefree());
	if (simple_pipe_dev.wp >= simple_pipe_dev.rp)
		count = min(count, (size_t)
			    (simple_pipe_dev.end - simple_pipe_dev.wp));
	else
		count = min(count, (size_t)
			    (simple_pipe_dev.rp - simple_pipe_dev.wp - 1));

	if (copy_from_user(simple_pipe_dev.wp, buf, count)) {
		up(&simple_pipe_dev.sem);
		return -EFAULT;
	}
	simple_pipe_dev.wp += count;
	if (simple_pipe_dev.wp == simple_pipe_dev.end)
		simple_pipe_dev.wp = simple_pipe_dev.buffer;
	up(&simple_pipe_dev.sem);

	wake_up_interruptible(&simple_pipe_dev.inq);

	return count;
}

/*
 * Init module.
 */
int simple_pipe_init_module(void)
{
    int result, err = 0;
    int devno;
    dev_t dev = 0;

    /* get a major number */
    result = alloc_chrdev_region(&dev, SIMPLE_PIPE_MINOR, 1, SIMPLE_PIPE_NAME);
    simple_pipe_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "simple: can't get major %d.\n", simple_pipe_major);
        return result;
    }

    /* init device */
	simple_pipe_dev.buffersize = SIMPLE_PIPE_SIZE;
    simple_pipe_dev.buffer = NULL;
    sema_init(&simple_pipe_dev.sem, 1);
	init_waitqueue_head(&simple_pipe_dev.inq);
	init_waitqueue_head(&simple_pipe_dev.outq);
    cdev_init(&simple_pipe_dev.cdev, &simple_pipe_fops);
    simple_pipe_dev.cdev.owner = THIS_MODULE;
    devno = MKDEV(simple_pipe_major, SIMPLE_PIPE_MINOR);
    err = cdev_add(&simple_pipe_dev.cdev, devno, 1);

    if (err) {
        printk(KERN_NOTICE "Error %d adding simple device.\n", err);
        goto fail;
    }

    return 0;
  fail:
    simple_pipe_exit_module();
    return err;
}

/*
 * Exit module.
 */
void simple_pipe_exit_module(void)
{
    dev_t devno = MKDEV(simple_pipe_major, SIMPLE_PIPE_MINOR);
    cdev_del(&simple_pipe_dev.cdev);
    unregister_chrdev_region(devno, 1);
    remove_proc_entry(SIMPLE_PIPE_NAME, NULL);
}

/* register module */
module_init(simple_pipe_init_module);
module_exit(simple_pipe_exit_module);
