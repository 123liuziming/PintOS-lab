#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

// 要保证一个inode_disk正好是一个扇区大小,即对齐后512字节
#define DIRECT_BLOCK_COUNT 123
#define INDIRECT_BLOCK_PER_SECTOR 128

#define MIN(a, b) a < b ? a : b

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    //block_sector_t start;               /* First data sector. */
    // 类似于linux 0.11 直接映射
    block_sector_t direct_blocks[DIRECT_BLOCK_COUNT];
    // 一级间接映射
    block_sector_t indirect_blocks;
    // 二级间接映射
    block_sector_t doubly_indirect_blocks;
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    uint32_t unused[125];               /* Not used. */
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };

// 将返回的在文件中的sector号转化为实际的盘块号
static block_sector_t index_to_sector(const struct inode_disk* inode, off_t index) {
  // 直接映射块,那么直接返回
  if (index < DIRECT_BLOCK_COUNT) {
    return inode->direct_blocks[index];
  }
  // 一级间接映射
  index -= DIRECT_BLOCK_COUNT;
  if (index < INDIRECT_BLOCK_PER_SECTOR) {
    uint8_t* buffer;
    block_read(fs_device, inode->indirect_blocks, buffer);
    return *(buffer + index);
  }
  // 二级间接映射
  index -= INDIRECT_BLOCK_PER_SECTOR;
  if (index < INDIRECT_BLOCK_PER_SECTOR * INDIRECT_BLOCK_PER_SECTOR) {
    uint8_t* buffer;
    // 在第一级时一个index代表128个sector
    int m = index / INDIRECT_BLOCK_PER_SECTOR;
    int n = index % INDIRECT_BLOCK_PER_SECTOR;
    // 读第一级,取第m个
    block_read(fs_device, inode->doubly_indirect_blocks, buffer);
    block_read(fs_device, *(buffer + m), buffer);
    return *(buffer + n);
  }
  // 文件太大了,没法搞
  return -1;
}


/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  // 从之前的顺序存储改为现在的索引存储
  ASSERT (inode != NULL);
  if (pos < inode->data.length) {
    int index = pos / BLOCK_SECTOR_SIZE;
    return index_to_sector(&inode->data, index);
  }
  else {
    return -1;
  }
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

static bool indirect_inode_alloc(struct inode_disk* node, int sector_num, int level) {
  // 一级间接映射
  int doubly_mapping = level == 1 ? 1 : DIV_ROUND_UP(sector_num, BLOCK_SECTOR_SIZE);
  if (level == 1 && !free_map_allocate(1, &node->indirect_blocks)) {
    return false;
  }
  if (level == 2 && !free_map_allocate(1, &node->doubly_indirect_blocks)) {
    return false;
  }
  int i = 0, m;
  int l = MIN(sector_num, BLOCK_SECTOR_SIZE);
  int sectors[BLOCK_SECTOR_SIZE];
  int doubly_sectors[BLOCK_SECTOR_SIZE];
  memset(doubly_sectors, 0, sizeof(doubly_sectors));
  for (m = 0; m < doubly_mapping; ++m) {
    int double_tmp;
    if (level == 2 && !free_map_allocate(1, &double_tmp)) {
      return false;
    }
    memset(sectors, 0, sizeof(sectors));
    for (; i < l; ++i) {
      int tmp;
      if (!free_map_allocate(1, &tmp)) {
        return false;
      }
      sectors[i] = tmp;
    }
    sector_num -= l;
    l = MIN(sector_num, BLOCK_SECTOR_SIZE);
    if (level == 1) {
      cache_write(node->indirect_blocks, sectors);
    } 
    else if (level == 2) {
      doubly_sectors[m] = double_tmp;
      cache_write(double_tmp, sectors);
    }
  }
  if (level == 2) {
    cache_write(node->doubly_indirect_blocks, doubly_sectors);
  }
  return true;
}

// 在磁盘上申请一个inode
static bool inode_alloc(struct inode_disk* node) {
  int len = node->length;
  char* zeros = (char*) malloc(BLOCK_SECTOR_SIZE * sizeof(char)); 
  // 一级映射,直接申请对应块就可以
  int sector_num = DIV_ROUND_UP(len, BLOCK_SECTOR_SIZE);
  int l = MIN(sector_num, DIRECT_BLOCK_COUNT);
  
  int i = 0;
  for (; i < l; ++i) {
    if (!free_map_allocate(1, &node->direct_blocks[i])) {
      return false;
    }
    // 申请到了inode
    cache_write(node->direct_blocks[i], zeros);
  }
  sector_num -= l;
  if (sector_num == 0) {
    return true;
  }

  l = MIN(sector_num, INDIRECT_BLOCK_PER_SECTOR);
  if (!indirect_inode_alloc(node, sector_num, 1)) {
    return false;
  }

  sector_num -= l;
  if (sector_num == 0) {
    return true;
  }

  l = MIN(sector_num, INDIRECT_BLOCK_PER_SECTOR * INDIRECT_BLOCK_PER_SECTOR);
  if (!indirect_inode_alloc(node, sector_num, 2)) {
    return false;
  }

  return sector_num == l;

}


/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      if (free_map_allocate (sectors, &disk_inode->start)) 
        {
          block_write (fs_device, sector, disk_inode);
          if (sectors > 0) 
            {
              static char zeros[BLOCK_SECTOR_SIZE];
              size_t i;
              
              for (i = 0; i < sectors; i++) 
                block_write (fs_device, disk_inode->start + i, zeros);
            }
          success = true; 
        } 
      free (disk_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  block_read (fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          free_map_release (inode->data.start,
                            bytes_to_sectors (inode->data.length)); 
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}
