#include <linux/module.h>
#include <linux/fs.h>

#include "sfs.h"

/*
 * Set inode operations.
 */
void sfs_set_inode(struct inode *inode)
{
  if (S_ISREG(inode->i_mode)) {
    inode->i_fop = &sfs_file_operations;
  } else if (S_ISDIR(inode->i_mode)) {
    inode->i_op = &sfs_dir_inode_operations;
    inode->i_fop = &sfs_dir_operations;
  }
}
