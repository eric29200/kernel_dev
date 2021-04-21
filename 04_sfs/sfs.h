#ifndef _SFS_H_
#define _SFS_H_

#include <stdint.h>

#define SFS_MAGIC               0xABCD
#define SFS_BLOCK_SIZE          1024
#define SFS_BITS_PER_BLOCK      ((SFS_BLOCK_SIZE) << 3)
#define SFS_ROOT_INODE          1
#define SFS_FILENAME_LEN        30

#define SFS_INODES_PER_BLOCK    ((SFS_BLOCK_SIZE) / (sizeof(struct sfs_inode)))

/*
 * SFS on disk superblock.
 */
struct sfs_super_block {
  uint32_t s_ninodes;           /* total number of inodes */
  uint32_t s_nzones;            /* total number of zones */
  uint16_t s_imap_blocks;       /* number of blocks used by inodes bit map */
  uint16_t s_zmap_blocks;       /* number of blocks used by zones bit map */
  uint16_t s_firstdatazone;     /* first data zone */
  uint16_t s_blocksize;         /* block size in byte */
  uint32_t s_max_size;          /* maximum file size */
  uint16_t s_magic;             /* magic number */
};

/*
 * SFS on disk inode.
 */
struct sfs_inode {
  uint16_t i_mode;              /* file mode */
  uint16_t i_nlinks;            /* number of links to this file */
  uint16_t i_uid;               /* owner id */
  uint16_t i_gid;               /* group id */
  uint32_t i_size;              /* file size in bytes */
  uint32_t i_atime;             /* access time */
  uint32_t i_mtime;             /* modification time */
  uint32_t i_ctime;             /* creation time */
  uint32_t i_zone[10];          /* data zones */
};

/*
 * SFS dir entry.
 */
struct sfs_dir_entry {
  uint16_t inode;
  char name[SFS_FILENAME_LEN];
};

#endif
