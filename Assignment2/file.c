#include<types.h>
#include<context.h>
#include<file.h>
#include<lib.h>
#include<serial.h>
#include<entry.h>
#include<memory.h>
#include<fs.h>
#include<kbd.h>


/************************************************************************************/
/***************************Do Not Modify below Functions****************************/
/************************************************************************************/

void free_file_object(struct file *filep)
{
	if(filep)
	{
		os_page_free(OS_DS_REG ,filep);
		stats->file_objects--;
	}
}

struct file *alloc_file()
{
	struct file *file = (struct file *) os_page_alloc(OS_DS_REG); 
	file->fops = (struct fileops *) (file + sizeof(struct file)); 
	bzero((char *)file->fops, sizeof(struct fileops));
	file->ref_count = 1;
	file->offp = 0;
	stats->file_objects++;
	return file; 
}

void *alloc_memory_buffer()
{
	return os_page_alloc(OS_DS_REG); 
}

void free_memory_buffer(void *ptr)
{
	os_page_free(OS_DS_REG, ptr);
}

/* STDIN,STDOUT and STDERR Handlers */

/* read call corresponding to stdin */

static int do_read_kbd(struct file* filep, char * buff, u32 count)
{
	kbd_read(buff);
	return 1;
}

/* write call corresponding to stdout */

static int do_write_console(struct file* filep, char * buff, u32 count)
{
	struct exec_context *current = get_current_ctx();
	return do_write(current, (u64)buff, (u64)count);
}

long std_close(struct file *filep)
{
	filep->ref_count--;
	if(!filep->ref_count)
		free_file_object(filep);
	return 0;
}
struct file *create_standard_IO(int type)
{
	struct file *filep = alloc_file();
	filep->type = type;
	if(type == STDIN)
		filep->mode = O_READ;
	else
		filep->mode = O_WRITE;
	if(type == STDIN){
		filep->fops->read = do_read_kbd;
	}else{
		filep->fops->write = do_write_console;
	}
	filep->fops->close = std_close;
	return filep;
}

int open_standard_IO(struct exec_context *ctx, int type)
{
	int fd = type;
	struct file *filep = ctx->files[type];
	if(!filep){
		filep = create_standard_IO(type);
	}else{
		filep->ref_count++;
		fd = 3;
		while(ctx->files[fd])
			fd++; 
	}
	ctx->files[fd] = filep;
	return fd;
}
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/

/* File exit handler */
void do_file_exit(struct exec_context *ctx)
{
	/*TODO the process is exiting. Adjust the refcount
	of files*/
	//! confusion
	for(int i=0;i<MAX_OPEN_FILES;i++){
		if(ctx->files[i]){
			ctx->files[i]->ref_count--;
			if(!(ctx->files[i]->ref_count))
				free_file_object(ctx->files[i]);
		}
	}
}

/*Regular file handlers to be written as part of the assignmemnt*/


static int do_read_regular(struct file *filep, char * buff, u32 count)
{
	/** 
	*  TODO Implementation of File Read, 
	*  You should be reading the content from File using file system read function call and fill the buf
	*  Validate the permission, file existence, Max length etc
	*  Incase of Error return valid Error code 
	**/
	if(!filep){
		return -EINVAL;
	}
	if(!filep->inode){
		return -EINVAL;
	}
	if(!(filep->mode & O_READ)){
		return -EACCES;
	}
	int read_count = flat_read(filep->inode, buff,count, &(filep->offp));
	filep->offp += read_count;
	return read_count;
}

/*write call corresponding to regular file */

static int do_write_regular(struct file *filep, char * buff, u32 count)
{
	/** 
	*   TODO Implementation of File write, 
	*   You should be writing the content from buff to File by using File system write function
	*   Validate the permission, file existence, Max length etc
	*   Incase of Error return valid Error code 
	* */
	if(!filep){
		return -EINVAL;
	}
	if(!filep->inode){
		return -EINVAL;
	}
	if(!(filep->mode & O_WRITE)){
		return -EACCES;
	}
	int write_count = flat_write(filep->inode, buff,count, &(filep->offp));
	filep->offp += write_count;
	return write_count;
}

long do_file_close(struct file *filep)
{
	/** TODO Implementation of file close  
	*   Adjust the ref_count, free file object if needed
	*   Incase of Error return valid Error code 
	*/
	if(!filep){
		return -EINVAL;
	}
	filep->ref_count--;
	if(!filep->ref_count)
		free_file_object(filep);
	return 0;
}

static long do_lseek_regular(struct file *filep, long offset, int whence)
{
	/** 
	*   TODO Implementation of lseek 
	*   Set, Adjust the ofset based on the whence
	*   Incase of Error return valid Error code 
	* */
	if(!filep){
		return -EINVAL;
	}
	long foffset=filep->offp;
	switch (whence)
	{
		case SEEK_SET:
		{
			foffset = offset;
			break;
		}
		case SEEK_CUR:
		{
			foffset += offset;
			break;
		}
		case SEEK_END:
		{
			foffset = filep->inode->file_size + offset;
			break;
		}
		default:
		{
			return -EINVAL;
		}
	}
	if(foffset > filep->inode->file_size || foffset <0){
		return -EINVAL;
	}
	filep->offp = foffset;
	return filep->offp;
}

extern int do_regular_file_open(struct exec_context *ctx, char* filename, u64 flags, u64 mode)
{

	/**  
	*  TODO Implementation of file open, 
	*  You should be creating file(use the alloc_file function to creat file), 
	*  To create or Get inode use File system function calls, 
	*  Handle mode and flags 
	*  Validate file existence, Max File count is 16, Max Size is 4KB, etc
	*  Incase of Error return valid Error code 
	* */
	if(!(flags & (O_READ|O_RDWR))){
		return -EINVAL;
	}
	struct inode* finode = lookup_inode(filename);
	if(!finode){
		if(!(flags & O_CREAT)){
			return -EINVAL;
		}
		finode = create_inode(filename, mode);
		if(!finode){
			return -ENOMEM;
		}
	}
	flags ^= O_CREAT;
	
	if(finode->mode & flags != flags){
		return -EACCES;
	}
	struct file* filep = alloc_file();

	filep->inode = finode;
	filep->mode = flags;

	filep->fops->close = do_file_close;
	filep->fops->lseek = do_lseek_regular;
	filep->fops->read = do_read_regular;
	filep->fops->write = do_write_regular;
	filep->type = REGULAR;
	int fd=3;
	while(ctx->files[fd]){
		fd++;
		if(fd >= MAX_OPEN_FILES)return -ENOMEM;
	}
	ctx->files[fd] = filep;
	return fd;
}

/**
 * Implementation dup 2 system call;
 */
int fd_dup2(struct exec_context *current, int oldfd, int newfd)
{
	/** 
	*  TODO Implementation of the dup2 
	*  Incase of Error return valid Error code 
	**/
	if(!current->files[oldfd]){
		return 	-EINVAL;
	}
	if(current->files[newfd]){
		do_file_close(current->files[newfd]);
	}
	current->files[newfd] = current->files[oldfd];
	current->files[oldfd]->ref_count++;
	return newfd;
}

int do_sendfile(struct exec_context *ctx, int outfd, int infd, long *offset, int count) {
	/** 
	*  TODO Implementation of the sendfile 
	*  Incase of Error return valid Error code 
	**/
	if(!ctx->files[infd])return -EINVAL;
	if(!ctx->files[outfd])return -EINVAL;
	if(!(ctx->files[infd]->mode & O_READ)) return -EACCES;
	if(!(ctx->files[outfd]->mode & O_WRITE)) return -EACCES;
	char *buff = (char*)alloc_memory_buffer();
	int read_count =0;
	if(offset){
		read_count= flat_read(ctx->files[infd]->inode, buff, count, (int*)offset);
		*offset += read_count;
	}else{
		read_count = do_read_regular(ctx->files[infd], buff, count);
	}

	int write_count = do_write_regular(ctx->files[outfd], buff, read_count);
	free_memory_buffer((void*)buff);
	return write_count;
}

