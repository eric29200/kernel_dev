#include <linux/module.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>

#include "sfs.h"

/*
 * Get raw SFS inode.
 */
static struct sfs_inode *sfs_raw_inode(struct super_block *sb, ino_t ino, struct buffer_head **bh)
{
  struct sfs_sb_info *sbi = sfs_sb_info(sb);
  struct sfs_inode *p;
  uint32_t block;

  /* check inode number */
  if (!ino || ino > sbi->s_ninodes) {
    printk("SFS: Bad inode number : %ld is out of range\n", (long) ino);
    return NULL;
  }

  /* decrement inode number (first sfs inode = 1) */
  ino--;

  /* read inode block */
  block = 2 + sbi->s_imap_blocks + sbi->s_zmap_blocks + ino / SFS_INODES_PER_BLOCK;
  *bh = sb_bread(sb, block);
  if (!*bh) {
    printk("SFS: Unable to read inode block\n");
    return NULL;
  }

  /* get inode */
  p = (struct sfs_inode *) (*bh)->b_data;
  return &p[ino % SFS_INODES_PER_BLOCK];
}

/*
 * Read an inode.
 */
struct inode *sfs_iget(struct super_block *sb, unsigned long ino)
{
  struct sfs_inode_info *sfs_inode;
  struct sfs_inode *raw_inode;
  struct buffer_head *bh;
  struct inode *inode;
  int i;

  /* try to get inode from cache or allocate a new one */
  inode = iget_locked(sb, ino);
  if (!inode)
    return ERR_PTR(-ENOMEM);

  /* inode was already present in the cache : return it */
  if (!(inode->i_state & I_NEW))
    return inode;

  /* get raw sfs inode */
  raw_inode = sfs_raw_inode(inode->i_sb, inode->i_ino, &bh);
  if (!raw_inode) {
    iget_failed(inode);
    return ERR_PTR(-EIO);
  }

  /* check if inode is still referenced */
  if (raw_inode->i_nlinks == 0) {
    printk("SFS: deleted inode referenced: %lu\n", inode->i_ino);
    brelse(bh);
    iget_failed(inode);
    return ERR_PTR(-ESTALE);
  }

  /* set generic inode */
  inode->i_mode = raw_inode->i_mode;
  i_uid_write(inode, raw_inode->i_uid);
  i_gid_write(inode, raw_inode->i_gid);
  set_nlink(inode, raw_inode->i_nlinks);
  inode->i_size = raw_inode->i_size;
  inode->i_mtime.tv_sec = raw_inode->i_mtime;
  inode->i_atime.tv_sec = raw_inode->i_atime;
  inode->i_ctime.tv_sec = raw_inode->i_ctime;
  inode->i_mtime.tv_nsec = 0;
  inode->i_atime.tv_nsec = 0;
  inode->i_ctime.tv_nsec = 0;
  inode->i_blocks = 0;

  /* set data blocks */
  sfs_inode = sfs_inode_info(inode);
  for (i = 0; i < 10; i++)
    sfs_inode->i_data[i] = raw_inode->i_zone[i];

  /* set inode operations */
  sfs_set_inode(inode);

  /* release buffer and unlock inode */
  brelse(bh);
  unlock_new_inode(inode);

  return inode;
}
