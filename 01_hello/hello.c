#include <linux/init.h>
#include <linux/module.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eric");
MODULE_DESCRIPTION("A simple Hello world");
MODULE_VERSION("0.1");

static int __init hello_init(void)
{
	printk(KERN_ALERT "Hello\n");
	return 0;
}

static void __exit hello_exit(void)
{
	printk(KERN_ALERT "Goodbye\n");
}

module_init(hello_init);
module_exit(hello_exit);
