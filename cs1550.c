/*
	FUSE: Filesystem in Userspace
	Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

	This program can be distributed under the terms of the GNU GPL.
	See the file COPYING.
*/

#define	FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

//size of a disk block
#define	BLOCK_SIZE 512

//we'll use 8.3 filenames
#define	MAX_FILENAME 8
#define	MAX_EXTENSION 3

//How many files can there be in one directory?
#define MAX_FILES_IN_DIR (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long))

//The attribute packed means to not align these things
struct cs1550_directory_entry
{
	int nFiles;	//How many files are in this directory.
				//Needs to be less than MAX_FILES_IN_DIR

	struct cs1550_file_directory
	{
		char fname[MAX_FILENAME + 1];	//filename (plus space for nul)
		char fext[MAX_EXTENSION + 1];	//extension (plus space for nul)
		size_t fsize;					//file size
		long nStartBlock;				//where the first block is on disk
	} __attribute__((packed)) files[MAX_FILES_IN_DIR];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.  
	char padding[BLOCK_SIZE - MAX_FILES_IN_DIR * sizeof(struct cs1550_file_directory) - sizeof(int)];
} ;

typedef struct cs1550_root_directory cs1550_root_directory;

#define MAX_DIRS_IN_ROOT (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + sizeof(long))

struct cs1550_root_directory
{
	int nDirectories;	//How many subdirectories are in the root
						//Needs to be less than MAX_DIRS_IN_ROOT
	struct cs1550_directory
	{
		char dname[MAX_FILENAME + 1];	//directory name (plus space for nul)
		long nStartBlock;				//where the directory block is on disk
	} __attribute__((packed)) directories[MAX_DIRS_IN_ROOT];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.  
	char padding[BLOCK_SIZE - MAX_DIRS_IN_ROOT * sizeof(struct cs1550_directory) - sizeof(int)];
} ;


typedef struct cs1550_directory_entry cs1550_directory_entry;

//How much data can one block hold?
#define	MAX_DATA_IN_BLOCK (BLOCK_SIZE - sizeof(long))

struct cs1550_disk_block
{
	//The next disk block, if needed. This is the next pointer in the linked 
	//allocation list
	long nNextBlock;

	//And all the rest of the space in the block can be used for actual data
	//storage.
	char data[MAX_DATA_IN_BLOCK];
};

typedef struct cs1550_disk_block cs1550_disk_block;

//Ensures dname, fname, and fext are empty. Returns the result of separating the path.
//Will return 0 if invalid, 1 if a directory, and 3 if a file.
static int parse_path(const char* path, char* dname, char* fname, char* fext)
{
	memset(dname, 0, MAX_FILENAME + 1);
	memset(fname, 0, MAX_FILENAME + 1);
	memset(fext, 0, MAX_EXTENSION + 1);
	
	return sscanf(path, "/%[^/]/%[^.].%s", dname, fname, fext);
}

//Originally had this function doing more, but figured it wasn't needed anymore. Seems somewhat pointless now.
static FILE* get_directories() 
{
	FILE* fp = fopen(".directories", "rb");
	
	return fp;
}

static int get_directory(FILE* fp, int index, cs1550_root_directory* dir)
{
	int res = 0;
	
	if (fp == NULL)
	{
		res = -ENOENT;
	}
	else
	{
		fseek(fp, index*sizeof(cs1550_root_directory), SEEK_SET);
		res = fread(dir, sizeof(cs1550_root_directory), 1, fp);
	}
	
	return res;
}

static int dir_exists(FILE* fp, char* dname)
{
	int res = 0;
	
	if (fp != NULL)
	{
		int is_found = 0;
		int index = 0;
		
		cs1550_root_directory* dir = malloc(sizeof(cs1550_root_directory));		
		for (get_directory(fp, index, &dir); get_directory(fp, index, &dir) == 0; index++)
		{
			if (strcmp(dir->directories[index].dname, dname) == 0)
			{
				is_found = 1;
				break;
			}
		}
		
		free(dir);
		
		if (is_found == 1)
		{
			res = 0;
		}
		else
		{
			res = -ENOENT;
		}
	}
	else
	{
		res = -ENOENT;
	}

	return res;
}

static int get_file(cs1550_directory_entry* entry, char* fname, char* fext)
{
	int file_count = 0;
	
	for (file_count = 0; file_count < entry->nFiles; file_count++)
	{
		if (strcmp(fname, entry->files[file_count].fname) == 0)
		{
			if (strcmp(fext, entry->files[file_count].fext == 0))
			{
				return file_count;
			}
		}
		
	}
	
	return -ENOENT;
	
}

/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not. 
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int cs1550_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));
   
	//is path the root dir?
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else {

		char dname[MAX_FILENAME + 1];	//directory name (plus space for nul)
		char fname[MAX_FILENAME + 1];	//filename (plus space for nul)
		char fext[MAX_EXTENSION + 1];	//file extension (plus space for nul)
		
		
		//Should be 1 if a directory. > 1 if a file.
		int parsed = parse_path(path, dname, fname, fext);
		FILE* fp = get_directories();
		
		//If directory
		if (parsed == 1)
		{
			
			
			if (fp == NULL)
			{
				printf(".directories doesn't exist");
				res = -ENOENT;
			}
			else
			{
				printf(".directories exists");
				
				//Need to find the directory now if it exists
				int exists = dir_exists(fp, dname);
				
				if (exists == 0)
				{
					stbuf->st_mode = S_IFDIR | 0755;
					stbuf->st_nlink = 2;
					res = 0;
				}
				else
				{
					res = -ENOENT;
				}
			}
	
		} 
		//if File
		else if (parsed > 1)
		{
			int index = 0;
			int found = 0;
			cs1550_root_directory* dir = malloc(sizeof(cs1550_root_directory)); 
			for (get_directory(fp, index, &dir); get_directory(fp, index, &dir) == 0; index++)
			{
				if (strcmp(dir->directories[index].dname, dname) == 0)
				{
					found = 1;
					break;
				}
			}
			
			if (found = 0)
			{
				res = -ENOENT;
			}
			else
			{
				cs1550_directory_entry* entry = malloc(sizeof(cs1550_directory_entry));
				int file_index = get_file(entry, fname, fext);
				
				if (file_index == -ENOENT)
				{
					res = -ENOENT;
				}
				else
				{
					stbuf->st_mode = S_IFREG | 0666; 
					stbuf->st_nlink = 1; //file links
					stbuf->st_size = entry->files[file_index].fsize; //file size - make sure you replace with real size!
					res = 0;
				}
				free(entry);
			}
		}
		fclose(fp);
		
	//Check if name is subdirectory
	/* 
		//Might want to return a structure with these fields
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		res = 0; //no error
	*/

	//Check if name is a regular file
	/*
		//regular file, probably want to be read and write
		stbuf->st_mode = S_IFREG | 0666; 
		stbuf->st_nlink = 1; //file links
		stbuf->st_size = 0; //file size - make sure you replace with real size!
		res = 0; // no error
	*/
	}
	
	return res;
}

/* 
 * Called whenever the contents of a directory are desired. Could be from an 'ls'
 * or could even be when a user hits TAB to do autocompletion
 */
static int cs1550_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	//Since we're building with -Wall (all warnings reported) we need
	//to "use" every parameter, so let's just cast them to void to
	//satisfy the compiler
	(void) offset;
	(void) fi;
	
	char dname[MAX_FILENAME + 1];	//directory name (plus space for nul)
	char fname[MAX_FILENAME + 1];	//filename (plus space for nul)
	char fext[MAX_EXTENSION + 1];	//file extension (plus space for nul)
	
	//Should be 1 if a directory. > 1 if a file.
	int parsed = parse_path(path, dname, fname, fext);
	FILE* fp = get_directories();
	

	//This line assumes we have no subdirectories, need to change
	if (strcmp(path, "/") == 0)
	{
		//the filler function allows us to add entries to the listing
		//read the fuse.h file for a description (in the ../include dir)
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);
		
		if (fp != NULL)
		{
			cs1550_directory_entry* entry = malloc(sizeof(cs1550_directory_entry));
			
			FILE* disk = fopen(".disk", "rb+");
			
			fread(entry, sizeof(cs1550_directory_entry), 1, disk);
			
			int index = 0;
			for (index = 0; index < MAX_FILES_IN_DIR; index++)
			{
				filler(buf, entry->files[index].fname, NULL, 0);
			}
			
			free(entry);
			free(disk);
		}
	}
	else
	{
		if (fp != NULL)
		{	
			int dir_index = 0;
			int is_found = 0;
			cs1550_root_directory* dir = malloc(sizeof(cs1550_root_directory)); 
			for (get_directory(fp, dir_index, &dir); get_directory(fp, dir_index, &dir) == 0; dir_index++)
			{
				if (strcmp(dir->directories[dir_index].dname, dname) == 0)
				{
					is_found = 1;
					break;
				}
			}
			
			
			if (is_found == 1)
			{
				cs1550_directory_entry* entry = malloc(sizeof(cs1550_directory_entry));
				int index = 0;
				for (get_directory(fp, index, &dir); get_directory(fp, index, &dir) == 0; index++)
				{
					for (index = 0; index < entry->nFiles; index++)
					{
						strcpy(fname, entry->files[index].fname);
						strcat(fname, ".");
						strcat(fname, entry->files[index].fext);
						
						filler(buf, fname, NULL, 0);
					}
				}
				
				fclose(fp);
				free(dir);
				free(entry);
			}
			else
			{
				fclose(fp);
				free(dir);
				return -ENOENT;
			}
		}
	}
	
	
	

	/*
	//add the user stuff (subdirs or files)
	//the +1 skips the leading '/' on the filenames
	filler(buf, newpath + 1, NULL, 0);
	*/
	return 0;
}

/* 
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int cs1550_mkdir(const char *path, mode_t mode)
{
	//(void) path;
	(void) mode;
	
	char dname[MAX_FILENAME + 1];	//directory name (plus space for nul)
	char fname[MAX_FILENAME + 1];	//filename (plus space for nul)
	char fext[MAX_EXTENSION + 1];	//file extension (plus space for nul)
		
		

	int parsed = parse_path(path, dname, fname, fext);
	
	if (strlen(dname) >= 9)
	{
		return -ENAMETOOLONG;
	}
	else if (parsed > 1)
	{
		return -EPERM;
	}
	else
	{
		FILE* fp = fopen(".directories", "ab+");
		
		int exists = dir_exists(fp, dname);
		
		if (exists != 0)
		{
			int index = 0;
			cs1550_root_directory* dir = malloc(sizeof(cs1550_root_directory));
			for (index = 0; index < MAX_FILES_IN_DIR; index++)
			{
				fread(dir, sizeof(cs1550_root_directory), 1, fp);
				if (dir->directories[index].dname == NULL)
				{
					FILE* disk = fopen(".disk", "rb+");
					
					strcpy(dir->directories[index].dname, dname);
					dir->nDirectories += 1;
					fwrite(dir, sizeof(cs1550_root_directory), 1, disk);
					fclose(fp);
					fclose(disk);
					free(dir);
					break;
				}
				
			}
			
		}
		else
		{
			return -EEXIST;
		}
		
	}
	

	return 0;
}

/* 
 * Removes a directory.
 */
static int cs1550_rmdir(const char *path)
{
	(void) path;
    return 0;
}

/* 
 * Does the actual creation of a file. Mode and dev can be ignored.
 *
 */
static int cs1550_mknod(const char *path, mode_t mode, dev_t dev)
{	
	(void) mode;
	(void) dev;
	
	char dname[MAX_FILENAME + 1];	//directory name (plus space for nul)
	char fname[MAX_FILENAME + 1];	//filename (plus space for nul)
	char fext[MAX_EXTENSION + 1];	//file extension (plus space for nul)
		
		

	int parsed = parse_path(path, dname, fname, fext);
	
	if (parsed < 2)
	{
		return -EPERM;
	}
	else
	{
		if (strlen(fname) >= 9 || strlen(fext) >= 4)
		{
			return -ENAMETOOLONG;
		}
		else
		{
			FILE* fp = fopen(".directories", "ab+");
			
			if (fp == NULL)
			{
				return -EPERM;
			}
			else
			{
				cs1550_root_directory* dir = malloc(sizeof(cs1550_root_directory));
				int dir_index = 0;
				int is_found = 0;
				
				for (get_directory(fp, dir_index, &dir); get_directory(fp, dir_index, &dir) == 0; dir_index++)
				{
					if (strcmp(dir->directories[dir_index].dname, dname) == 0)
					{
						is_found = 1;
						break;
					}
				}	
			
				if (is_found == 1)
				{
					FILE* disk = fopen(".disk", "rb+");
					
					cs1550_directory_entry* entry = malloc(sizeof(cs1550_directory_entry));
					fread(entry, sizeof(cs1550_directory_entry), 1, disk);
					
					if (get_file(entry, fname, fext) != -ENOENT)
					{
						free(fp);
						free(dir);
						free(entry);
						return -EEXIST;
					}
					else
					{
						struct cs1550_file_directory* file = malloc(sizeof(struct cs1550_file_directory));
						
						strcpy(file->fname, fname);
						
						if (parsed == 3)
						{
							strcpy(file->fext, fext);
						}
						else
						{
							strcpy(file->fext, "");
						}
						
						int i = 0;
						for (i = 0; i < MAX_FILES_IN_DIR; i++)
						{
							if (entry->files[i].fname[0] == '\0')
							{
								break;
							}
						}
						
						entry->files[i] = *file;
						entry->nFiles++;
						
						disk = fopen(".disk", "rb+");
						
						fseek(disk, i*sizeof(cs1550_root_directory), SEEK_SET);
						fwrite(dir, sizeof(cs1550_directory_entry), 1, disk);
						
						fclose (fp);
						free(dir);
						free(entry);
						free(file);
						fclose(disk);
					}
				}
				
			}
		}
	}
	return 0;
}

/*
 * Deletes a file
 */
static int cs1550_unlink(const char *path)
{
    (void) path;

    return 0;
}

/* 
 * Read size bytes from file into buf starting from offset
 *
 */
static int cs1550_read(const char *path, char *buf, size_t size, off_t offset,
			  struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	//check to make sure path exists
	//check that size is > 0
	//check that offset is <= to the file size
	//read in data
	//set size and return, or error

	if (size <= 0)
	{
		return -1;
	}
	char dname[MAX_FILENAME + 1];	//directory name (plus space for nul)
	char fname[MAX_FILENAME + 1];	//filename (plus space for nul)
	char fext[MAX_EXTENSION + 1];	//file extension (plus space for nul)
		
		

	int parsed = parse_path(path, dname, fname, fext);
	
	//if directory
	if (parsed < 2)
	{
		return -EISDIR;
	}
	else
	{
		int is_found = 0;
		FILE* fp = get_directories();
		cs1550_root_directory* dir = malloc(sizeof(cs1550_root_directory));
		int dir_index = 0;
		
		for (get_directory(fp, dir_index, &dir); get_directory(fp, dir_index, &dir) == 0; dir_index++)
		{
			if (strcmp(dir->directories[dir_index].dname, dname) == 0)
			{
				is_found = 1;
				break;
			}
		}
		int res = 0;
		if (is_found == 1)
		{
			cs1550_directory_entry* entry = malloc(sizeof(cs1550_directory_entry));
			FILE* disk = fopen(".disk", "rb+");
			
			fseek(disk, dir->directories[dir_index].nStartBlock*BLOCK_SIZE, SEEK_SET);
			res = fread(entry, sizeof(cs1550_directory_entry), 1, disk);
		
			int file_loc = get_file(entry, fname, fext);
			
			if (file_loc == -ENOENT)
			{
				return file_loc;
			}
			else
			{
				long block = entry->files[file_loc].nStartBlock;
				long fsize = entry->files[file_loc].fsize;
				
				if (offset > fsize)
				{
					return -EFBIG;
				}
				
				cs1550_disk_block* db = malloc(sizeof(cs1550_disk_block));
				int num_offset_blocks = offset / MAX_DATA_IN_BLOCK;
				
				fseek(disk, block*BLOCK_SIZE, SEEK_SET);
				res = fread(db, sizeof(cs1550_disk_block), 1, disk);

				int i = 0;
				for (i = 0; i < num_offset_blocks; i++)
				{
					block = db->nNextBlock;
					
					fseek(disk, block*BLOCK_SIZE, SEEK_SET);
					res = fread(db, sizeof(cs1550_disk_block), 1, disk);
				}
				
				int ret = 0;
				int bytes = offset % MAX_DATA_IN_BLOCK;
				
				for (ret = 0; ret < size; ret++)
				{
					if ((ret + bytes) % MAX_DATA_IN_BLOCK == 0 && ret > 0)
					{
						block = db->nNextBlock;
						fseek(disk, block*BLOCK_SIZE, SEEK_SET);
						fread(db, sizeof(cs1550_disk_block), 1, disk);
					}
					else if (ret + offset > fsize)
					{
						break;
					}
					
					buf[ret] = db->data[(ret + bytes) % MAX_DATA_IN_BLOCK];
				}
				
				fclose(fp);
				fclose(disk);
				free(db);
				free(entry);
				free(dir);
				return ret;
				
			}
		}
		else
		{
			fclose(fp);
			free(dir);
			return -ENOENT;
		}

	}
	size = 0;

	return size;
}

/* 
 * Write size bytes from buf into file starting from offset
 *
 */
static int cs1550_write(const char *path, const char *buf, size_t size, 
			  off_t offset, struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	//check to make sure path exists
	//check that size is > 0
	//check that offset is <= to the file size
	//write data
	//set size (should be same as input) and return, or error

	if (size <= 0)
	{
		return -1;
	}
	char dname[MAX_FILENAME + 1];	//directory name (plus space for nul)
	char fname[MAX_FILENAME + 1];	//filename (plus space for nul)
	char fext[MAX_EXTENSION + 1];	//file extension (plus space for nul)
		
		

	int parsed = parse_path(path, dname, fname, fext);
	
	//if directory
	if (parsed < 2)	
	{
		return -EISDIR;
	}
	else
	{
		int is_found = 0;
		FILE* fp = get_directories();
		cs1550_root_directory* dir = malloc(sizeof(cs1550_root_directory));
		int dir_index = 0;
		
		for (get_directory(fp, dir_index, &dir); get_directory(fp, dir_index, &dir) == 0; dir_index++)
		{
			if (strcmp(dir->directories[dir_index].dname, dname) == 0)
			{
				is_found = 1;
				break;
			}
		}
		
		int res = 0;
		if (is_found == 1)
		{
			cs1550_directory_entry* entry = malloc(sizeof(cs1550_directory_entry));
			FILE* disk = fopen(".disk", "rb+");
			
			fseek(disk, dir->directories[dir_index].nStartBlock*BLOCK_SIZE, SEEK_SET);
			res = fread(entry, sizeof(cs1550_directory_entry), 1, disk);
		
			int file_loc = get_file(entry, fname, fext);
			
			if (file_loc == -ENOENT)
			{
				return file_loc;
			}
			else
			{
				long block = entry->files[file_loc].nStartBlock;
				long fsize = entry->files[file_loc].fsize;
				
				if (offset > fsize)
				{
					return -EFBIG;
				}
				
				cs1550_disk_block* db = malloc(sizeof(cs1550_disk_block));
				int num_offset_blocks = offset / MAX_DATA_IN_BLOCK;
				
				fseek(disk, block*BLOCK_SIZE, SEEK_SET);
				res = fread(db, sizeof(cs1550_disk_block), 1, disk);

				int i = 0;
				for (i = 0; i < num_offset_blocks; i++)
				{
					block = db->nNextBlock;
					
					fseek(disk, block*BLOCK_SIZE, SEEK_SET);
					res = fread(db, sizeof(cs1550_disk_block), 1, disk);
				}
				
				int ret = 0;
				int bytes = offset % MAX_DATA_IN_BLOCK;
				
				for (ret = 0; ret < size; ret++)
				{
					while ((ret + bytes) % MAX_DATA_IN_BLOCK != 0 || ret == 0)
					{
						db->data[(ret + bytes) % MAX_DATA_IN_BLOCK] = buf[ret];
						ret++;
						
						if (ret > size)
						{
							break;
						}
					}
					
					fseek(disk, block*BLOCK_SIZE, SEEK_SET);
					fwrite(db, sizeof(cs1550_disk_block), 1, disk);
					block = db->nNextBlock;
					fread(db, sizeof(cs1550_disk_block), 1, disk);
					
				}
				
				
				fclose(fp);
				fclose(disk);
				free(db);
				free(entry);
				free(dir);
				return size;
				
			}
		}
		else
		{
			fclose(fp);
			free(dir);
			return -ENOENT;
		}
	}
	
	return size;
}

/******************************************************************************
 *
 *  DO NOT MODIFY ANYTHING BELOW THIS LINE
 *
 *****************************************************************************/

/*
 * truncate is called when a new file is created (with a 0 size) or when an
 * existing file is made shorter. We're not handling deleting files or 
 * truncating existing ones, so all we need to do here is to initialize
 * the appropriate directory entry.
 *
 */
static int cs1550_truncate(const char *path, off_t size)
{
	(void) path;
	(void) size;

    return 0;
}


/* 
 * Called when we open a file
 *
 */
static int cs1550_open(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;
    /*
        //if we can't find the desired file, return an error
        return -ENOENT;
    */

    //It's not really necessary for this project to anything in open

    /* We're not going to worry about permissions for this project, but 
	   if we were and we don't have them to the file we should return an error

        return -EACCES;
    */

    return 0; //success!
}

/*
 * Called when close is called on a file descriptor, but because it might
 * have been dup'ed, this isn't a guarantee we won't ever need the file 
 * again. For us, return success simply to avoid the unimplemented error
 * in the debug log.
 */
static int cs1550_flush (const char *path , struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;

	return 0; //success!
}


//register our new functions as the implementations of the syscalls
static struct fuse_operations hello_oper = {
    .getattr	= cs1550_getattr,
    .readdir	= cs1550_readdir,
    .mkdir	= cs1550_mkdir,
	.rmdir = cs1550_rmdir,
    .read	= cs1550_read,
    .write	= cs1550_write,
	.mknod	= cs1550_mknod,
	.unlink = cs1550_unlink,
	.truncate = cs1550_truncate,
	.flush = cs1550_flush,
	.open	= cs1550_open,
};

//Don't change this.
int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &hello_oper, NULL);
}
	