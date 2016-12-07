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

#define DIRECT 124
#define SINGLE_INDIRECT 252
#define DOUBLE_INDIRECT 16636

//void free_inode(struct inode_disk *disk_inode, off_t length);
//bool grow_inode(struct inode_disk *disk_inode, off_t length);

/* On-disk inode.
   Must be exactly DISK_SECTOR_SIZE bytes long. */
struct inode_disk {
//    disk_sector_t start;                /* First data sector. */
//    off_t length;                       /* File size in bytes. */
//    unsigned magic;                     /* Magic number. */
//    uint32_t unused[125];               /* Not used. */
	off_t length;
	unsigned magic;
	uint32_t list_sector[126]; // last two for single and double indirect sectors
};

struct indirect_sector{
	uint32_t list_sector[128];
};

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, DISK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    disk_sector_t sector;               /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };

/* Returns the disk sector that contains byte offset POS within
   INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static disk_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
	/*
	ASSERT(inode != NULL);
	if (pos < inode->data.length)
		return inode->data.start + pos / DISK_SECTOR_SIZE;
	else
		return -1;
	*/
	ASSERT(inode != NULL);
//	printf("byte_to_sector: inode data length %u, pos %u\n", inode->data.length, pos);
	if (pos < inode->data.length){
		int sector = pos / DISK_SECTOR_SIZE;
		/* direct sector */
		if(sector < DIRECT){
			return inode->data.list_sector[sector];
		}
		/* single indirect sector */
		struct indirect_sector *indirect;
		struct cache_entry *ce;
		if(sector < SINGLE_INDIRECT){
			indirect = malloc(sizeof(struct indirect_sector));
			ce = cache_read(inode->data.list_sector[124]);
			indirect = ce->sector;

			disk_sector_t ret_sector = indirect->list_sector[sector-124];
			free(indirect);
			return ret_sector;
		}
		/* double indirect sector */
		if(sector < DOUBLE_INDIRECT){
			indirect = malloc(sizeof(struct indirect_sector));
			ce = cache_read(inode->data.list_sector[125]);
			indirect = ce->sector;

			disk_sector_t index = indirect->list_sector[(sector-SINGLE_INDIRECT)/128];
			ce = cache_read(index);
			indirect = ce->sector;

			disk_sector_t ret_sector = indirect->list_sector[(sector-SINGLE_INDIRECT)%128];
			free(indirect);
			return ret_sector;
		}
		printf("sector greater than DOUBLE_INDIRECT\n");
		return -1;
	}
	else{
		printf("pos greater than inode data length\n");
		printf("byte_to_sector: inode data length %u, pos %u\n", inode->data.length, pos);
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
//  printf("inode_init.\n");
  list_init (&open_inodes);
}

void free_inode(struct inode_disk *disk_inode, off_t length){
	size_t num_sector = bytes_to_sectors(length);
	ASSERT(num_sector < DOUBLE_INDIRECT);

	struct indirect_sector *double_indirect;
	struct indirect_sector *indirect;
	int i, j;

	/* double indirect sector */
	if(disk_inode->list_sector[125] != -1){
		double_indirect = disk_inode->list_sector[125];
		for(i=0; i<128; i++){
			if(num_sector < DOUBLE_INDIRECT - 128*i
					&& double_indirect->list_sector[127-i] != -1){
				indirect = double_indirect->list_sector[127-i];
				for(j=0; j<128; j++){
					if(num_sector < DOUBLE_INDIRECT - 128*i - j
							&& indirect->list_sector[127-j] != -1){
						free_map_release(indirect->list_sector[127-j], 1);
						indirect->list_sector[127-j] = -1;
					}
				}
				if(num_sector < DOUBLE_INDIRECT - 128*i - 127){
					free_map_release(indirect, 1);
					double_indirect->list_sector[127-i] = -1;
				} else{
					disk_write(filesys_disk, double_indirect->list_sector[127-i], indirect);
				}
			}
		}
		if(num_sector < SINGLE_INDIRECT+1){
			free_map_release(double_indirect, 1);
			disk_inode->list_sector[125] = -1;
		} else{
			disk_write(filesys_disk, disk_inode->list_sector[125], double_indirect);
		}
	}
	/* single indirect sector */
	if(num_sector < SINGLE_INDIRECT && disk_inode->list_sector[124] != -1){
		indirect = disk_inode->list_sector[124];
		for(i=0; i<127; i++){
			if(num_sector < SINGLE_INDIRECT - i){
				free_map_release(indirect->list_sector[127-i], 1);
				indirect->list_sector[127-i] = -1;
			}
		}
		if (num_sector < DIRECT + 1) {
			free_map_release(indirect, 1);
			disk_inode->list_sector[124] = -1;
		} else {
			disk_write(filesys_disk, disk_inode->list_sector[124], indirect);
		}
	}
	/* direct sector */
	for(i=0; i<123; i++){
		if(num_sector < DIRECT - i && disk_inode->list_sector[123-i] != -1){
			free_map_release(disk_inode->list_sector[123-i], 1);
			disk_inode->list_sector[123-i] = -1;
		}
	}

	disk_inode->length = length;
}

bool grow_inode(struct inode_disk *disk_inode, off_t length){
	size_t num_sector = bytes_to_sectors(length);
	size_t curr_num_sector = bytes_to_sectors(disk_inode->length);
	ASSERT(num_sector < DOUBLE_INDIRECT && curr_num_sector < DOUBLE_INDIRECT);
	int growth = num_sector - curr_num_sector;
//	printf("grow_inode: init growth %d\n", growth);

	if(growth <= 0){
		disk_inode->length = length;
		return true;
	}

	uint32_t i, j;
	disk_sector_t direct_sector = 0;
	static char zeros[DISK_SECTOR_SIZE];

	/* direct sector */
	if(curr_num_sector < DIRECT){
		for(i = curr_num_sector; i < num_sector && i < DIRECT; i++){
			if(free_map_allocate(1, &direct_sector)){
				disk_write(filesys_disk, direct_sector, zeros);
				disk_inode->list_sector[i] = direct_sector;
				growth--;
			} else{
				free_inode(disk_inode, curr_num_sector);
//				printf("grow_inode: a\n");
				return false;
			}
		}
	}
	if (growth <= 0) {
		disk_inode->length = length;
		return true;
	}
	curr_num_sector = DIRECT;

	/* single indirect sector */
	disk_sector_t indirect_sector = 0;
	struct indirect_sector *indirect;
	indirect = malloc(sizeof(struct indirect_sector));
	if (indirect == NULL) {
		free_inode(disk_inode, curr_num_sector);
		free(indirect);
//		printf("grow_inode: b\n");
		return false;
	}

	if(curr_num_sector < SINGLE_INDIRECT){
		if(disk_inode->list_sector[124] == -1){
			if(free_map_allocate(1, &indirect_sector)){
				for(i=0; i<128; i++){
					indirect->list_sector[i] = -1;
				}
			} else{
				free_inode(disk_inode, curr_num_sector);
				free(indirect);
//				printf("grow_inode: c\n");
				return false;
			}
		} else{
			disk_read(filesys_disk, disk_inode->list_sector[124], indirect);
		}

		for(i = curr_num_sector; i < (num_sector - DIRECT)&& i < SINGLE_INDIRECT; i++){
			if(free_map_allocate(1, &direct_sector)){
				disk_write(filesys_disk, direct_sector, zeros);
				indirect->list_sector[i] = direct_sector;
				growth--;
			} else{
				free_inode(disk_inode, curr_num_sector);
				free(indirect);
//				printf("grow_inode: d\n");
				return false;
			}
		}

		if(disk_inode->list_sector[124] == -1){
			disk_inode->list_sector[124] = indirect_sector;
			disk_write(filesys_disk, indirect_sector, indirect);
		} else{
			disk_write(filesys_disk, disk_inode->list_sector[124], indirect);
		}
	}
	if (growth <= 0) {
		disk_inode->length = length;
		free(indirect);
		return true;
	}
	curr_num_sector = SINGLE_INDIRECT;

	/* double indirect sector */
	disk_sector_t double_indirect_sector = 0;
	struct indirect_sector *double_indirect;
	double_indirect = malloc(sizeof(struct indirect_sector));
	if(double_indirect == NULL){
		free_inode(disk_inode, curr_num_sector);
		free(indirect);
		free(double_indirect);
//		printf("grow_inode: e\n");
		return false;
	}

	if(disk_inode->list_sector[125] == -1){
		if(free_map_allocate(1, &double_indirect_sector)){
			for(i=0; i<128; i++)
				double_indirect->list_sector[i] = -1;
		} else{
			free_inode(disk_inode, curr_num_sector);
			free(indirect);
			free(double_indirect);
//			printf("grow_inode: f\n");
			return false;
		}
	} else{
		disk_read(filesys_disk, disk_inode->list_sector[125], double_indirect);
	}

	for(i = (curr_num_sector - SINGLE_INDIRECT) / 128; i < (num_sector - SINGLE_INDIRECT) / 128 + 1 && i < 128; i++){
		if(double_indirect->list_sector[i] == -1){
			if(free_map_allocate(1, &indirect_sector)){
				for(j=0; j<128; j++)
					indirect->list_sector[j] = -1;
			} else{
				free_inode(disk_inode, curr_num_sector);
				free(indirect);
				free(double_indirect);
//				printf("grow_inode: g\n");
				return false;
			}
		} else{
			disk_read(filesys_disk, double_indirect->list_sector[i], indirect);
		}

		for(j = curr_num_sector - SINGLE_INDIRECT - (i*128); j < num_sector - SINGLE_INDIRECT - (i*128) + 1 && j < 128 ;j++){
			if(free_map_allocate(1, &direct_sector)){
				disk_write(filesys_disk, direct_sector, zeros);
				indirect->list_sector[j] = direct_sector;
				growth--;
			} else{
				free_inode(disk_inode, curr_num_sector);
				free(indirect);
				free(double_indirect);
//				printf("grow_inode: h\n");
				return false;
			}
		}

		if(double_indirect->list_sector[i] == -1){
			double_indirect->list_sector[i] = indirect_sector;
			disk_write(filesys_disk, indirect_sector, indirect);
		} else{
			disk_write(filesys_disk, double_indirect->list_sector[i], indirect);
		}
	}

	if(disk_inode->list_sector[125] == -1){
		disk_inode->list_sector[125] = double_indirect_sector;
		disk_write(filesys_disk, double_indirect_sector, double_indirect);
	} else{
		disk_write(filesys_disk, disk_inode->list_sector[125], double_indirect);
	}
//	printf("grow_inode: final growth %d\n", growth);
	if(growth <= 0){
		disk_inode->length = length;
		free(indirect);
		free(double_indirect);
		return true;
	}
//	printf("grow_inode: i\n");
//	printf("grow_inode: length %u, num_sector %u\n", length, num_sector);
	return false;
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   disk.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (disk_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == DISK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  /*
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      if (free_map_allocate (sectors, &disk_inode->start))
        {
          disk_write (filesys_disk, sector, disk_inode);
          if (sectors > 0) 
            {
              static char zeros[DISK_SECTOR_SIZE];
              size_t i;
              
              for (i = 0; i < sectors; i++) 
                disk_write (filesys_disk, disk_inode->start + i, zeros); 
            }
          success = true; 
        } 
      free (disk_inode);
    }
  return success;
  */
  if(disk_inode != NULL){
	  disk_inode->length = 0;
	  disk_inode->magic = INODE_MAGIC;

	  int i;
	  for(i=0; i<126; i++){
		  disk_inode->list_sector[i] = -1;
	  }

	  success = grow_inode(disk_inode, length);
	  if(success){
		  disk_write(filesys_disk, sector, disk_inode);
	  }
	  // debug
	  else{
		  printf("inode_create: grow_inode fail\n");
	  }

	  free(disk_inode);
  }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (disk_sector_t sector) 
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
  disk_read (filesys_disk, inode->sector, &inode->data);
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
disk_sector_t
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
          /*
          free_map_release (inode->data.start,
                            bytes_to_sectors (inode->data.length));
          */
          free_inode(&(inode->data), 0);
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

	while (size > 0) {
		/* Disk sector to read, starting byte offset within sector. */
		disk_sector_t sector_idx = byte_to_sector(inode, offset);
//		printf("inode_read_at: sector_idx %u, %d\n", sector_idx, sector_idx); ////
		if (sector_idx == -1){
			printf("inode_read_at: sector_idx -1, offset %u\n", offset);
			return bytes_read;
		}

		int sector_ofs = offset % DISK_SECTOR_SIZE;

		/* Bytes left in inode, bytes left in sector, lesser of the two. */
		off_t inode_left = inode_length(inode) - offset;
		int sector_left = DISK_SECTOR_SIZE - sector_ofs;
		int min_left = inode_left < sector_left ? inode_left : sector_left;

		/* Number of bytes to actually copy out of this sector. */
		int chunk_size = size < min_left ? size : min_left;
		if (chunk_size <= 0)
			break;

		if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) {
			/* Read full sector directly into caller's buffer. */
			/*
//			printf("inode_read_at: bytes_read %u\n", bytes_read);
			 disk_read (filesys_disk, sector_idx, buffer + bytes_read);
			*/
//			/*
			struct cache_entry *ce = cache_read(sector_idx);
			if (ce == NULL)
				break;
			memcpy(buffer + bytes_read, ce->sector, DISK_SECTOR_SIZE);
//			*/
		} else {
			/* Read sector into bounce buffer, then partially copy
			 into caller's buffer. */
			/*
			 if (bounce == NULL)
			 {
			 bounce = malloc (DISK_SECTOR_SIZE);
			 if (bounce == NULL)
			 break;
			 }
			 disk_read (filesys_disk, sector_idx, bounce);
			 memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
			 */
//			/*
			struct cache_entry *ce = cache_read(sector_idx);
			if (ce == NULL)
				break;
			memcpy(buffer + bytes_read, ce->sector + sector_ofs, chunk_size);
//			*/
		}

		/* Advance. */
		size -= chunk_size;
		offset += chunk_size;
		bytes_read += chunk_size;
	}
	free(bounce);

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
      disk_sector_t sector_idx = byte_to_sector (inode, offset);
      if(sector_idx == -1){
    	  printf("inode_read_at: sector_idx -1\n");
    	  return bytes_written;
      }

      int sector_ofs = offset % DISK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = DISK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) 
        {
          /* Write full sector directly to disk. */
    	  /*
          disk_write (filesys_disk, sector_idx, buffer + bytes_written);
          */
//          /*
    	  struct cache_entry *ce = cache_write(sector_idx);
    	  if(ce == NULL)
    		  break;
    	  memcpy(ce->sector, buffer + bytes_written, DISK_SECTOR_SIZE);
//    	  */
        }
      else 
        {
//          /* We need a bounce buffer.*/
//          if (bounce == NULL)
//            {
//              bounce = malloc (DISK_SECTOR_SIZE);
//              if (bounce == NULL)
//                break;
//            }
//
//          /* If the sector contains data before or after the chunk
//             we're writing, then we need to read in the sector
//             first.  Otherwise we start with a sector of all zeros.*/
//          if (sector_ofs > 0 || chunk_size < sector_left)
//            disk_read (filesys_disk, sector_idx, bounce);
//          else
//            memset (bounce, 0, DISK_SECTOR_SIZE);
//          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
//          disk_write (filesys_disk, sector_idx, bounce);
//          /*
			struct cache_entry *ce = cache_write(sector_idx);
			if (ce == NULL)
				break;

			if (sector_ofs == 0 && chunk_size == sector_left)
				memset(ce->sector, 0, DISK_SECTOR_SIZE);
			memcpy(ce->sector + sector_ofs, buffer + bytes_written, chunk_size);
//			*/
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
