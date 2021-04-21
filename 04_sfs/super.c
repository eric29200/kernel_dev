#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/buffer_head.h>

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
 * SFS superblock operations.
 */
static const struct super_operations sfs_sops = {
  .alloc_inode      = NULL,
  .free_inode       = NULL,
  .write_inode      = NULL,
  .evict_inode      = NULL,
  .put_super        = NULL,
  .statfs           = NULL,
  .remount_fs       = NULL,
};

/*
 * Fill SFS super block.
 */
static int sfs_fill_super(struct super_block *s, void *data, int silent)
{
  struct sfs_super_block *ssb;
  struct sfs_sb_info *sbi;
  struct inode *root_inode;
  struct buffer_head **map;
  struct buffer_head *bh;
  unsigned long i, block;
  int ret = -EINVAL;

  /* create a super block in memory */
  sbi = kzalloc(sizeof(struct sfs_sb_info), GFP_KERNEL);
  if (!sbi)
    return -ENOMEM;
  s->s_fs_info = sbi;

  /* set block size */
  if (!sb_set_blocksize(s, SFS_BLOCK_SIZE))
    goto out_bad_hblock;

  /* read super block sector */
  bh = sb_bread(s, 1);
  if (!bh)
    goto out_bad_sb;

  /* copy super block informations from disk to memory */
  ssb = (struct sfs_super_block *) bh->b_data;
  sbi->s_sbh = bh;
  sbi->s_ninodes = ssb->s_ninodes;
  sbi->s_nzones = ssb->s_nzones;
  sbi->s_imap_blocks = ssb->s_imap_blocks;
  sbi->s_zmap_blocks = ssb->s_zmap_blocks;
  sbi->s_firstdatazone = ssb->s_firstdatazone;
  s->s_maxbytes = ssb->s_max_size;
  s->s_magic = ssb->s_magic;
  s->s_op = &sfs_sops;
  s->s_time_min = 0;
  s->s_time_max = U32_MAX;

  /* check super block */
  if (s->s_magic != SFS_MAGIC || sbi->s_ninodes == 0 || sbi->s_nzones == 0)
    goto out_no_fs;

  /* allocate inode and zones bitmaps */
  map = kzalloc((sbi->s_imap_blocks + sbi->s_zmap_blocks) * sizeof(struct buffer_head *), GFP_KERNEL);
  if (!map)
    goto out_no_map;
  sbi->s_imap = &map[0];
  sbi->s_zmap = &map[sbi->s_imap_blocks];

  /* read inodes bitmap */
  block = 2;
  for (i = 0; i < sbi->s_imap_blocks; i++, block++) {
    sbi->s_imap[i] = sb_bread(s, block);
    if (!sbi->s_imap[i])
      goto out_no_bitmap;
  }

  /* read zones bitmap */
  for (i = 0; i < sbi->s_zmap_blocks; i++, block++) {
    sbi->s_zmap[i] = sb_bread(s, block);
    if (!sbi->s_zmap[i])
      goto out_no_bitmap;
  }

  /* get root inode */
  root_inode = sfs_iget(s, SFS_ROOT_INODE);
  if (IS_ERR(root_inode)) {
    ret = PTR_ERR(root_inode);
    goto out_no_root;
  }

  /* make root inode */
  s->s_root = d_make_root(root_inode);
  if (!s->s_root) {
    ret = -ENOMEM;
    goto out_no_root;
  }

  return 0;
out_no_root:
  if (!silent)
    printk("SFS: get root inode failed\n");
  goto out_freemap;
out_no_bitmap:
  if (!silent)
    printk("SFS: can't read bitmaps\n");
out_freemap:
  for (i = 0; i < sbi->s_imap_blocks; i++)
    brelse(sbi->s_imap[i]);
  for (i = 0; i < sbi->s_zmap_blocks; i++)
    brelse(sbi->s_zmap[i]);
  kfree(sbi->s_imap);
  goto out_release;
out_no_map:
  if (!silent)
    printk("SFS: can't allocate bitmaps\n");
  ret = -ENOMEM;
  goto out_release;
out_no_fs:
  printk("SFS: no SFS file system on disk\n");
out_release:
  brelse(bh);
  goto out;
out_bad_sb:
  printk("SFS: unable to read superblock\n");
  goto out;
out_bad_hblock:
  printk("SFS: blocksize too small\n");
out:
  s->s_fs_info = NULL;
  kfree(sbi);
  return ret;
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

  /* register file system */
  err = register_filesystem(&sfs_fs_type);
  if (err) {
    destroy_inodecache();
    return err;
  }

	return 0;
}

/*
 * Unload SFS module.
 */
static void __exit exit_sfs(void)
{
  /* unregister file system */
  unregister_filesystem(&sfs_fs_type);

  /* destroy inode cache */
  destroy_inodecache();
}

module_init(init_sfs);
module_exit(exit_sfs);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eric");
