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

#define SIMPLE_NAME		"simple"
#define SIMPLE_MAJOR		0
#define SIMPLE_MINOR		0
#define SIMPLE_SIZE		2048
#define SIMPLE_MAX_SIZE		2048 * 1024
#define SIMPLE_PROC_SIZE	128

/* simple device */
int simple_major;
char *simple_data;
int simple_data_size;
char simple_proc_data[SIMPLE_PROC_SIZE];
struct semaphore simple_sem;
struct cdev simple_cdev;

/* prototypes */
int simple_open(struct inode *, struct file *);
int simple_release(struct inode *, struct file *);
ssize_t simple_read(struct file *, char __user *, size_t, loff_t *);
ssize_t simple_write(struct file *, const char __user *, size_t, loff_t *);
loff_t simple_llseek(struct file *, loff_t, int);
long simple_ioctl(struct file *, unsigned int, unsigned long);
ssize_t simple_read_proc(struct file *, char __user *, size_t, loff_t *);
int simple_init_module(void);
void simple_exit_module(void);

/* file operations structure */
struct file_operations simple_fops = {
	.owner				= THIS_MODULE,
	.open				= simple_open,
	.release			= simple_release,
	.read				= simple_read,
	.write				= simple_write,
	.llseek				= simple_llseek,
	.unlocked_ioctl		= simple_ioctl,
};

/* proc file operations */
struct proc_ops simple_proc_ops = {
	.proc_read	= simple_read_proc,
};

/* ioctl definition */
#define SIMPLE_IOC_MAGIC	'k'
#define SIMPLE_IOCSSIZE		_IOW(SIMPLE_IOC_MAGIC,  1, int)

/*
 * Open the device.
 */
int simple_open(struct inode *inode, struct file *filp)
{
	/* trim if write only */
	if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
		if (down_interruptible(&simple_sem))
			return -ERESTARTSYS;

		memset(simple_data, 0, simple_data_size);
		up(&simple_sem);
	}

	return 0;
}

/*
 * Close the device.
 */
int simple_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/*
 * Read the device.
 */
ssize_t simple_read(struct file *filp, char __user *buf, size_t count,
		    loff_t *f_pos)
{
	int retval = 0;

	if (down_interruptible(&simple_sem))
		return -ERESTARTSYS;

	if (*f_pos >= simple_data_size)
		goto out;

	if (*f_pos + count > simple_data_size)
		count = simple_data_size - *f_pos;

	if (copy_to_user(buf, simple_data + *f_pos, count)) {
		retval = -EFAULT;
		goto out;
	}

	*f_pos += count;
	retval = count;

  out:
	up(&simple_sem);
	return retval;
}

/*
 * Write the device.
 */
ssize_t simple_write(struct file *filp, const char __user *buf, size_t count,
		     loff_t *f_pos)
{
	int retval = 0;

	if (down_interruptible(&simple_sem))
		return -ERESTARTSYS;

	if (*f_pos >= simple_data_size)
		goto out;

	if (*f_pos + count > simple_data_size)
		count = simple_data_size - *f_pos;

	if (copy_from_user(simple_data + *f_pos, buf, count)) {
		retval = -EFAULT;
		goto out;
	}

	*f_pos += count;
	retval = count;
  out:
	up(&simple_sem);
	return retval;
}

/*
 * Llsek the device.
 */
loff_t simple_llseek(struct file *filp, loff_t off, int whence)
{
	loff_t new_pos;

	if (down_interruptible(&simple_sem))
		return -ERESTARTSYS;

	switch(whence) {
		case 0:
			new_pos = off;
			break;
		case 1:
			new_pos = filp->f_pos + off;
			break;
		case 2:
			new_pos = simple_data_size + off;
			break;
		default:
			new_pos = -EINVAL;
			break;
	}

	if (new_pos < 0)
		new_pos = -EINVAL;
	else
		filp->f_pos = new_pos;

	up(&simple_sem);
	return new_pos;
}

/*
 * Ioctl on the device.
 */
long simple_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	int old_size = simple_data_size;

	if (_IOC_TYPE(cmd) != SIMPLE_IOC_MAGIC)
		return -ENOTTY;

	if (cmd != SIMPLE_IOCSSIZE)
		return -ENOTTY;

	if (!access_ok((void __user *) arg, _IOC_SIZE(cmd)))
		return -EFAULT;

	if (down_interruptible(&simple_sem))
		return -ERESTARTSYS;

	retval = __get_user(simple_data_size, (int __user *) arg);
	if (retval)
		goto out;

	if (simple_data_size < 0)
		simple_data_size = 0;

	if (simple_data_size > SIMPLE_MAX_SIZE)
		simple_data_size = SIMPLE_MAX_SIZE;

	/* realloc buffer */
	simple_data = (char *) krealloc(simple_data, simple_data_size,
					GFP_KERNEL);
	if (!simple_data) {
		simple_data_size = 0;
		retval = -ENOMEM;
		goto out;
	}

	/* memzero */
	if (old_size <= 0 && simple_data_size > 0)
		memset(simple_data, 0, simple_data_size);

	/* update proc entry */
	memset(simple_proc_data, 0, SIMPLE_PROC_SIZE);
	snprintf(simple_proc_data, SIMPLE_PROC_SIZE, "Device %s : size %d.\n",
		 SIMPLE_NAME, simple_data_size);
  out:
	up(&simple_sem);
	return retval;
}

/*
 * Read proc entry.
 */
ssize_t simple_read_proc(struct file *filp, char __user *buf, size_t count,
			 loff_t *f_pos)
{
	int retval = 0;

	if (down_interruptible(&simple_sem))
		return -ERESTARTSYS;

	if (*f_pos >= SIMPLE_PROC_SIZE)
		goto out;

	if (*f_pos + count > SIMPLE_PROC_SIZE)
		count = SIMPLE_PROC_SIZE - *f_pos;

	if (copy_to_user(buf, simple_proc_data + *f_pos, count)) {
		retval = -EFAULT;
		goto out;
	}

	*f_pos += count;
	retval = count;
  out:
	up(&simple_sem);
	return retval;
}

/*
 * Init module.
 */
int simple_init_module(void)
{
	int result, err = 0;
	int devno;
	dev_t dev = 0;

	/* get a major number */
	result = alloc_chrdev_region(&dev, SIMPLE_MINOR, 1, SIMPLE_NAME);
	simple_major = MAJOR(dev);
	if (result < 0) {
		printk(KERN_WARNING "simple: can't get major %d.\n",
		       simple_major);
		return result;
	}

	/* init device */
	simple_data_size = SIMPLE_SIZE;
	simple_data = (char *) kmalloc(simple_data_size, GFP_KERNEL);
	if (!simple_data) {
		err = -ENOMEM;
		goto fail;
	}

	memset(simple_data, 0, simple_data_size);
	sema_init(&simple_sem, 1);
	cdev_init(&simple_cdev, &simple_fops);
	simple_cdev.owner = THIS_MODULE;
	devno = MKDEV(simple_major, SIMPLE_MINOR);
	err = cdev_add(&simple_cdev, devno, 1);

	if (err) {
		printk(KERN_NOTICE "Error %d adding simple device.\n", err);
		goto fail;
	}

	/* create proc entry */
	memset(simple_proc_data, 0, SIMPLE_PROC_SIZE);
	snprintf(simple_proc_data, SIMPLE_PROC_SIZE, "Device %s : size %d.\n",
		 SIMPLE_NAME, simple_data_size);
	proc_create(SIMPLE_NAME, 0, NULL, &simple_proc_ops);

	return 0;
  fail:
    simple_exit_module();
	return err;
}

/*
 * Exit module.
 */
void simple_exit_module(void)
{
	dev_t devno = MKDEV(simple_major, SIMPLE_MINOR);
	cdev_del(&simple_cdev);
	unregister_chrdev_region(devno, 1);
	remove_proc_entry(SIMPLE_NAME, NULL);
	if (simple_data)
		kfree(simple_data);
}

/* register module */
module_init(simple_init_module);
module_exit(simple_exit_module);
