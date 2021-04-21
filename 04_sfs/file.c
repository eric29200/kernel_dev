#include <linux/module.h>
#include <linux/fs.h>

#include "sfs.h"

/*
 * SFS file operations.
 */
const struct file_operations sfs_file_operations = {
  .llseek         = generic_file_llseek,
  .read_iter      = generic_file_read_iter,
  .write_iter     = generic_file_write_iter,
  .mmap           = generic_file_mmap,
  .fsync          = generic_file_fsync,
  .splice_read    = generic_file_splice_read,
};
