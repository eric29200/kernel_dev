#include <linux/init.h>
#include <linux/module.h>

static int __init init_sfs(void)
{
	return 0;
}

static void __exit exit_sfs(void)
{
}

module_init(init_sfs);
module_exit(exit_sfs);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eric");
