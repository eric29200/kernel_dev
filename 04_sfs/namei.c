#include <linux/module.h>
#include <linux/fs.h>

#include "sfs.h"

/*
 * SFS dir operations.
 */
const struct file_operations sfs_dir_operations = {
  .llseek           = generic_file_llseek,
  .read             = generic_read_dir,
  .iterate_shared   = NULL,
  .fsync            = generic_file_fsync,
};

/*
 * SFS dir inode operations.
 */
const struct inode_operations sfs_dir_inode_operations = {
  .create     = NULL,
  .lookup     = NULL,
  .mkdir      = NULL,
};
