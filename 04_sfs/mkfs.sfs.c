#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <stdint.h>

#include "sfs.h"

#define div_ceil(a, b)      (((a) / (b)) + (((a) % (b)) > 0 ? 1 : 0))

#define setbit(a, i)        ((a)[(i) / CHAR_BIT] |= 1 << ((i) % CHAR_BIT))
#define clrbit(a, i)        ((a)[(i) / CHAR_BIT] &= ~(1 << ((i) % CHAR_BIT)))
#define isset(a, i)         ((a)[(i) / CHAR_BIT] & (1 << ((i) % CHAR_BIT)))

#define mark_inode(x)       (setbit(inode_map, (x)))
#define unmark_inode(x)     (clrbit(inode_map, (x)))

#define mark_zone(x)        (setbit(zone_map, (x)))
#define unmark_zone(x)      (clrbit(zone_map, (x)))

/* inode and zone map */
static struct sfs_super_block *sb = NULL;
static char *inode_map = NULL;
static char *zone_map = NULL;
static char *inode_table = NULL;

/*
 * Malloc or exit.
 */
static void *xmalloc(size_t size)
{
  void *ret = malloc(size);
  if (!ret)
    exit(1);

  return ret;
}

/*
 * Write super block.
 */
static int write_super_block(int fd, off_t file_size)
{
  uint32_t nb_blocks, nb_inodes;
  uint32_t nb_blocks_itable;
  char buffer[SFS_BLOCK_SIZE];
  int i;

  /* allocate a new super block */
  sb = xmalloc(sizeof(struct sfs_super_block));

  /* compute number of blocks and inodes */
  nb_blocks = file_size / SFS_BLOCK_SIZE;
  nb_inodes = nb_blocks / 3;

  /* round number of inodes to fill a block */
  nb_inodes = (nb_inodes + SFS_INODES_PER_BLOCK - 1) & ~(SFS_INODES_PER_BLOCK - 1);

  /* set super block number of inodes and blocks */
  sb->s_ninodes = nb_inodes;
  sb->s_nzones = nb_blocks;

  /* compute number of blocks needed for inodes bit map */
  sb->s_imap_blocks = div_ceil(nb_inodes + 1, SFS_BITS_PER_BLOCK);

  /* compute number of blocks needed for zones bit map */
  nb_blocks_itable = div_ceil(nb_inodes, SFS_INODES_PER_BLOCK);
  sb->s_zmap_blocks = div_ceil(nb_blocks - (2 + sb->s_imap_blocks + nb_blocks_itable), SFS_BITS_PER_BLOCK - 1);

  /* compute first data zone (skip super block, imap, zmap and inodes table */
  sb->s_firstdatazone = 2 + sb->s_imap_blocks + sb->s_zmap_blocks + nb_blocks_itable;

  /* set max file size */
  sb->s_max_size = INT_MAX;
  sb->s_blocksize = SFS_BLOCK_SIZE;
  sb->s_magic = SFS_MAGIC;

  /* write first block (empty, reserved) */
  memset(buffer, 0, SFS_BLOCK_SIZE);
  if (write(fd, buffer, SFS_BLOCK_SIZE) != SFS_BLOCK_SIZE) {
    perror("write");
    return -1;
  }

  /* write second block with super block */ 
  memcpy(buffer, sb, sizeof(struct sfs_super_block));
  if (write(fd, buffer, SFS_BLOCK_SIZE) != SFS_BLOCK_SIZE) {
    perror("write");
    return -1;
  }

  /* allocate inodes and zones bit maps and inode table */
  inode_map = xmalloc(sb->s_imap_blocks * SFS_BLOCK_SIZE);
  zone_map = xmalloc(sb->s_zmap_blocks * SFS_BLOCK_SIZE);
  inode_table = xmalloc(div_ceil(sb->s_ninodes, SFS_INODES_PER_BLOCK) * SFS_BLOCK_SIZE);

  /* set all inodes and zones used */
  memset(inode_map, 0xFF, sb->s_imap_blocks * SFS_BLOCK_SIZE);
  memset(zone_map, 0xFF, sb->s_zmap_blocks * SFS_BLOCK_SIZE);
  memset(inode_table, 0, div_ceil(sb->s_ninodes, SFS_INODES_PER_BLOCK) * SFS_BLOCK_SIZE);

  /* unmark available inodes */
  for (i = SFS_ROOT_INODE; i <= sb->s_ninodes; i++)
    unmark_inode(i);

  /* unmark available data zones */
  for (i = sb->s_firstdatazone; i < sb->s_nzones; i++)
    unmark_zone(i);

  /* print stats */
  printf("%d inodes\n", nb_inodes);
  printf("%d blocks\n", nb_blocks);
  printf("first data zone = %d\n", sb->s_firstdatazone);

  return 0;
}

/*
 * Write root inode.
 */
static int write_root_inode(int fd)
{
  struct sfs_inode *root_inode = (struct sfs_inode *) inode_table;
  char root_block[SFS_BLOCK_SIZE];
  struct sfs_dir_entry dir_entry;

  /* mark root inode */
  mark_inode(SFS_ROOT_INODE);

  /* set inode */
  root_inode->i_nlinks = 2;
  root_inode->i_atime = root_inode->i_mtime = root_inode->i_ctime = time(NULL);
  root_inode->i_mode = S_IFDIR + 0755;
  root_inode->i_uid = getuid();
  root_inode->i_gid = getgid();
  root_inode->i_zone[0] = sb->s_firstdatazone;
  root_inode->i_size = sizeof(struct sfs_dir_entry) * 2;

  /* zero root block */
  memset(root_block, 0, SFS_BLOCK_SIZE);

  /* create entry '.' in root block */
  dir_entry.inode = SFS_ROOT_INODE;
  memset(dir_entry.name, 0, SFS_FILENAME_LEN);
  strcpy(dir_entry.name, ".");
  memcpy(root_block, &dir_entry, sizeof(struct sfs_dir_entry));

  /* create entry '..' in root block */
  dir_entry.inode = SFS_ROOT_INODE;
  memset(dir_entry.name, 0, SFS_FILENAME_LEN);
  strcpy(dir_entry.name, "..");
  memcpy(root_block + sizeof(struct sfs_dir_entry), &dir_entry, sizeof(struct sfs_dir_entry));

  /* mark root inode zone */
  mark_zone(root_inode->i_zone[0]);

  /* seek to root block */
  if (lseek(fd, root_inode->i_zone[0] * SFS_BLOCK_SIZE, SEEK_SET) == -1) {
    perror("lseek");
    return -1;
  }

  /* write root block */
  if (write(fd, root_block, SFS_BLOCK_SIZE) != SFS_BLOCK_SIZE) {
    perror("write");
    return -1;
  }

  /* seek to root block */
  if (lseek(fd, root_inode->i_zone[0] * SFS_BLOCK_SIZE, SEEK_SET) == -1) {
    perror("lseek");
    return -1;
  }

  return 0;
}

/*
 * Write inodes map.
 */
static int write_imap(int fd)
{
  /* seek to imap */
  if (lseek(fd, 2 * SFS_BLOCK_SIZE, SEEK_SET) == -1) {
    perror("lseek");
    return -1;
  }

  /* write imap */
  if (write(fd, inode_map, sb->s_imap_blocks * SFS_BLOCK_SIZE) != sb->s_imap_blocks * SFS_BLOCK_SIZE) {
    perror("write");
    return -1;
  }

  return 0;
}

/*
 * Write zones map.
 */
static int write_zmap(int fd)
{
  /* seek to zmap */
  if (lseek(fd, (2 + sb->s_imap_blocks) * SFS_BLOCK_SIZE, SEEK_SET) == -1) {
    perror("lseek");
    return -1;
  }

  /* write zmap */
  if (write(fd, zone_map, sb->s_zmap_blocks * SFS_BLOCK_SIZE) != sb->s_zmap_blocks * SFS_BLOCK_SIZE) {
    perror("write");
    return -1;
  }

  return 0;
}

/*
 * Write inode table.
 */
static int write_inode_table(int fd)
{
  int count;

  /* seek to inode table */
  if (lseek(fd, (2 + sb->s_imap_blocks + sb->s_zmap_blocks) * SFS_BLOCK_SIZE, SEEK_SET) == -1) {
    perror("lseek");
    return -1;
  }

  /* write inode table */
  count = div_ceil(sb->s_ninodes, SFS_INODES_PER_BLOCK) * SFS_BLOCK_SIZE;
  if (write(fd, inode_table, count) != count) {
    perror("write");
    return -1;
  }

  return 0;
}

/*
 * Main function.
 */
int main(int argc, char **argv)
{
  struct stat statbuf;
  char *path;
  int fd, ret = 0;

  /* check parameters */
  if (argc != 2) {
    fprintf(stderr, "Usage: %s disk.img\n", argv[0]);
    return -1;
  }

  /* get file size */
  path = argv[1];
  if (stat(path, &statbuf) == -1) {
    perror("stat");
    return -1;
  }

  /* check file size */
  if (statbuf.st_size < 10 * SFS_BLOCK_SIZE) {
    fprintf(stderr, "Disk image too small : at least 1 MB is required\n");
    return -1;
  }

  /* open file */
  fd = open(path, O_RDWR);
  if (fd == -1) {
    perror("open");
    return -1;
  }

  /* write super block */
  ret = write_super_block(fd, statbuf.st_size);
  if (ret == -1)
    goto out;

  /* write root inode */
  ret = write_root_inode(fd);
  if (ret == -1)
    goto out;

  /* write imap */
  ret = write_imap(fd);
  if (ret == -1)
    goto out;

  /* write zmap */
  ret = write_zmap(fd);
  if (ret == -1)
    goto out;

  /* write inode table */
  write_inode_table(fd);
  if (ret == -1)
    goto out;

out:
  if (inode_table)
    free(inode_table);
  if (inode_map)
    free(inode_map);
  if (zone_map)
    free(zone_map);
  if (sb)
    free(sb);
  close(fd);
  return ret;
}
