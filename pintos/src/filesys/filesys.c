#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"
#include "threads/synch.h"

/* The disk that contains the file system. */
struct disk *filesys_disk;

struct lock lock_filesys;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  filesys_disk = disk_get (0, 1);
  if (filesys_disk == NULL)
    PANIC ("hd0:1 (hdb) not present, file system initialization failed");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();

  lock_init(&lock_filesys);
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, bool is_dir)
{
  disk_sector_t inode_sector = 0;
  int length = strlen(name);
  if(length == 0)
	  return NULL;

  char path[length+1];
  char filename[length+1];
  memset(path, 0, length);
  memset(filename, 0, length);
  parse_dir(name, path, filename);

  lock_acquire(&lock_filesys);
  struct dir *dir = dir_open_path(path);
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, is_dir)
                  && dir_add (dir, filename, inode_sector, is_dir));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);
  lock_release(&lock_filesys);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open(const char *name) {
	int length = strlen(name);
	if(length == 0)
		return NULL;

	char path[length+1];
	char filename[length+1];
	memset(path, 0, length);
	memset(filename, 0, length);
	parse_dir(name, path, filename);
	lock_acquire(&lock_filesys);
	struct dir *dir = dir_open_path(path);
	struct inode *inode = NULL;

	if(dir == NULL){
		lock_release(&lock_filesys);
		return NULL;
	}

	if(strlen(filename) > 0){
		dir_lookup(dir, filename, &inode);
		dir_close(dir);
	} else{
		inode = dir_get_inode (dir);
	}

	if(inode == NULL || inode_is_removed(inode)){
		lock_release(&lock_filesys);
		return NULL;
	}

	lock_release(&lock_filesys);
	return file_open(inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
	int length = strlen(name);
	if (length == 0)
		return NULL;

	char path[length + 1];
	char filename[length + 1];
	memset(path, 0, length);
	memset(filename, 0, length);
	parse_dir(name, path, filename);

	lock_acquire(&lock_filesys);
	struct dir *dir = dir_open_path(path);
	if(dir == NULL){
		lock_release(&lock_filesys);
		return false;
	}

	bool success = dir_remove(dir, filename);
	dir_close (dir);

	lock_release(&lock_filesys);
  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
