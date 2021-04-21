#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>

#include "sfs.h"

/* SFS inode cache. */
static struct kmem_cache *sfs_inode_cache;

/*
 * Init a SFS inode.
 */
static void init_once(void *foo)
{
  struct sfs_inode_info *ei = (struct sfs_inode_info *) foo;
  inode_init_once(&ei->vfs_inode);
}

/*
 * Create inode cache.
 */
static int __init init_inodecache(void)
{
  sfs_inode_cache = kmem_cache_create("sfs_inode_cache", sizeof(struct sfs_inode_info), 0,
                                      (SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD | SLAB_ACCOUNT), init_once);

  if (!sfs_inode_cache)
    return -ENOMEM;

  return 0;
}

/*
 * Destroy inode cache.
 */
static void destroy_inodecache(void)
{
  /* make sure all delayed rcu free inodes are flushed */
  rcu_barrier();

  /* destroy cache */
  kmem_cache_destroy(sfs_inode_cache);
}

/*
 * Fill SFS super block.
 */
static int sfs_fill_super(struct super_block *s, void *data, int silent)
{
  return 0;
}

/*
 * Mount SFS file system.
 */
static struct dentry *sfs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data)
{
  return mount_bdev(fs_type, flags, dev_name, data, sfs_fill_super);
}

/* SFS file system */
static struct file_system_type sfs_fs_type = {
  .owner        = THIS_MODULE,
  .name         = "sfs",
  .mount        = sfs_mount,
  .kill_sb      = kill_block_super,
  .fs_flags     = FS_REQUIRES_DEV,
};

/*
 * Load SFS module.
 */
static int __init init_sfs(void)
{
  /* init inode cache */
  int err = init_inodecache();
  if (err)
    return err;

	return 0;
}

/*
 * Unload SFS module.
 */
static void __exit exit_sfs(void)
{
  destroy_inodecache();
}

module_init(init_sfs);
module_exit(exit_sfs);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eric");
