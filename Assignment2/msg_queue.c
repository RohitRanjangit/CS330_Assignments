#include <msg_queue.h>
#include <context.h>
#include <memory.h>
#include <file.h>
#include <lib.h>
#include <entry.h>



/************************************************************************************/
/***************************Do Not Modify below Functions****************************/
/************************************************************************************/

struct msg_queue_info *alloc_msg_queue_info()
{
	struct msg_queue_info *info;
	info = (struct msg_queue_info *)os_page_alloc(OS_DS_REG);
	
	if(!info){
		return NULL;
	}
	return info;
}

void free_msg_queue_info(struct msg_queue_info *q)
{
	os_page_free(OS_DS_REG, q);
}

struct message *alloc_buffer()
{
	struct message *buff;
	buff = (struct message *)os_page_alloc(OS_DS_REG);
	if(!buff)
		return NULL;
	return buff;	
}

void free_msg_queue_buffer(struct message *b)
{
	os_page_free(OS_DS_REG, b);
}

/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/


int do_create_msg_queue(struct exec_context *ctx)
{
	/** 
	 * TODO Implement functionality to
	 * create a message queue
	 **/
	int fd =3;
	while(ctx->files[fd]){
		fd++;
		if(fd >= MAX_OPEN_FILES)return -ENOMEM;
	}
	

	struct msg_queue_info* message_queue_info = alloc_msg_queue_info();

	if(!message_queue_info){
		return -ENOMEM;
	}
	struct msg_queue_member_info* member_info = (struct msg_queue_member_info*)(message_queue_info + sizeof(struct msg_queue_info));
	message_queue_info->member_info  = member_info;

	message_queue_info->member_info->member_count =1;
	message_queue_info->member_info->member_pid[0] = ctx->pid;
	message_queue_info->msg_buffer[0] = alloc_buffer();
	message_queue_info->msg_count =0;
	for(int i=0;i<MAX_MEMBERS;i++){
		for(int j=0;j<MAX_MEMBERS;j++){
			message_queue_info->block_process[i][j] = -1;
		}
	}
	struct file* filep = alloc_file();
	if(!filep){
		free_msg_queue_info(message_queue_info);
		return -ENOMEM;
	}
	filep->fops->lseek = NULL;
	filep->fops->read = NULL;
	filep->fops->close = NULL;
	filep->fops->write = NULL;
	filep->msg_queue = message_queue_info;
	filep->type = MSG_QUEUE;
	ctx->files[fd] = filep;
	return fd;
}


int do_msg_queue_rcv(struct exec_context *ctx, struct file *filep, struct message *msg)
{
	/** 
	 * TODO Implement functionality to
	 * recieve a message
	 **/
	if(!msg)return -EINVAL;
	if(!filep) return -EINVAL;
	if(!filep->msg_queue)return -EINVAL;
	int check =0;
	for(int i=0;i<filep->msg_queue->member_info->member_count;i++){
		if(filep->msg_queue->member_info->member_pid[i] == ctx->pid){
			check =1;
			break;
		}
	}
	if(!check)return -EINVAL;
	for(int i=0;i<filep->msg_queue->msg_count;i++){
		if(filep->msg_queue->msg_buffer[i]->to_pid == ctx->pid){
			*msg = *filep->msg_queue->msg_buffer[i];
			for(int j=i;j<filep->msg_queue->msg_count-1;j++){
				//! change here copying instead pointer change
				*filep->msg_queue->msg_buffer[j] = *filep->msg_queue->msg_buffer[j+1];
			}
			// filep->msg_queue->msg_buffer[filep->msg_queue->msg_count-1] = NULL;
			filep->msg_queue->msg_count--;
			return 1;
		}
	}
	return 0;
}


int do_msg_queue_send(struct exec_context *ctx, struct file *filep, struct message *msg)
{
	/** 
	 * TODO Implement functionality to
	 * send a message
	 **/
	if(!filep)return -EINVAL;
	if(!filep->msg_queue)return -EINVAL;
	if(!msg)return -EINVAL;
	int check =0;
	for(int i=0;i<filep->msg_queue->member_info->member_count;i++){
		if(filep->msg_queue->member_info->member_pid[i] == ctx->pid){
			check =1;
			break;
		}
	}
	if(!check)return -EINVAL;
	if(msg->from_pid != ctx->pid || msg->from_pid == msg->to_pid){
		return -EINVAL;
	}
	int delivered =0;
	if(msg->to_pid == BROADCAST_PID){
		for(int i=0;i<filep->msg_queue->member_info->member_count;i++){
			if(filep->msg_queue->member_info->member_pid[i]!=ctx->pid){
				int block_check =0;
				for(int j =0;j<MAX_MEMBERS;j++){
					if(ctx->pid== filep->msg_queue->block_process[i][j])block_check =1;
				}
				if(block_check)continue;
				filep->msg_queue->msg_buffer[filep->msg_queue->msg_count] = (struct message*)(filep->msg_queue->msg_buffer[0] + filep->msg_queue->msg_count*(sizeof(struct message)));
				*filep->msg_queue->msg_buffer[filep->msg_queue->msg_count] = *msg;
				filep->msg_queue->msg_buffer[filep->msg_queue->msg_count]->to_pid = filep->msg_queue->member_info->member_pid[i];
				filep->msg_queue->msg_count++;
				delivered++;
			}
		}
		return delivered;
	}else{
		for(int i=0;i<filep->msg_queue->member_info->member_count;i++){
			if(filep->msg_queue->member_info->member_pid[i]==msg->to_pid){
				int block_check =0;
				for(int j=0;j<MAX_PROCESSES;j++){
					if(filep->msg_queue->block_process[i][j] == ctx->pid)block_check =1;
				}
				if(block_check)return  -EINVAL;
				filep->msg_queue->msg_buffer[filep->msg_queue->msg_count] = (struct message*)(filep->msg_queue->msg_buffer[0] + filep->msg_queue->msg_count*(sizeof(struct message)));
				*filep->msg_queue->msg_buffer[filep->msg_queue->msg_count] = *msg;
				filep->msg_queue->msg_count++;
				return 1;
			}
		}
	}
	return -EINVAL;
}

void do_add_child_to_msg_queue(struct exec_context *child_ctx)
{
	/** 
	 * TODO Implementation of fork handler 
	 **/
	for(int fd=0;fd<MAX_OPEN_FILES;fd++){
		if(child_ctx->files[fd] && child_ctx->files[fd]->msg_queue ){
			child_ctx->files[fd]->msg_queue->member_info->member_count++;
			child_ctx->files[fd]->msg_queue->member_info->member_pid[child_ctx->files[fd]->msg_queue->member_info->member_count-1] = child_ctx->pid;
		}
	}
}

void do_msg_queue_cleanup(struct exec_context *ctx)
{
	/** 
	 * TODO Implementation of exit handler 
	 **/
	for(int i=0;i<MAX_OPEN_FILES;i++){
		if(ctx->files[i] && ctx->files[i]->msg_queue){
			do_msg_queue_close(ctx, i);
		}
	}
}

int do_msg_queue_get_member_info(struct exec_context *ctx, struct file *filep, struct msg_queue_member_info *info)
{
	/** 
	 * TODO Implementation of exit handler 
	 **/
	if(!filep)return -EINVAL;
	if(!filep->msg_queue)return -EINVAL;
	*info = *filep->msg_queue->member_info;
	return 0;
}


int do_get_msg_count(struct exec_context *ctx, struct file *filep)
{
	/** 
	 * TODO Implement functionality to
	 * return pending message count to calling process
	 **/
	if(!filep)return -EINVAL;
	if(!filep->msg_queue)return -EINVAL;
	int check =0;
	for(int i=0;i<filep->msg_queue->member_info->member_count;i++){
		if(filep->msg_queue->member_info->member_pid[i] == ctx->pid){
			check =1;
			break;
		}
	}
	if(!check)return -EINVAL;
	int addressed =0;
	for(int i=0;i<filep->msg_queue->msg_count;i++){
		if(filep->msg_queue->msg_buffer[i]->to_pid == ctx->pid)addressed++;
	}
	return addressed;
}

int do_msg_queue_block(struct exec_context *ctx, struct file *filep, int pid)
{
	/** 
	 * TODO Implement functionality to
	 * block messages from another process 
	 **/
	if(!filep)return -EINVAL;
	if(!filep->msg_queue)return -EINVAL;
	if(pid <0)return -EINVAL;
	int idx_ctx =-1 , idx_pid=-1;
	for(int i=0;i<filep->msg_queue->member_info->member_count;i++){
		if(filep->msg_queue->member_info->member_pid[i] == ctx->pid){
			idx_ctx =i;
		}
		if(filep->msg_queue->member_info->member_pid[i] == pid){
			idx_pid =i;
		}
	}
	if(idx_pid ==-1 || idx_ctx == -1 || idx_ctx == idx_pid)return -EINVAL;
	for(int i=0;i<MAX_MEMBERS;i++){
		if(filep->msg_queue->block_process[idx_ctx][i] == pid){
			return 0;
		}
	}
	for(int i=0;i<MAX_MEMBERS;i++){
		if(filep->msg_queue->block_process[idx_ctx][i] == -1){
			filep->msg_queue->block_process[idx_ctx][i] = pid;
			return 0;
		}
	}
	return -EINVAL;
}

int do_msg_queue_close(struct exec_context *ctx, int fd)
{
	/** 
	 * TODO Implement functionality to
	 * remove the calling process from the message queue 
	 **/
	if(fd <0)return -EINVAL;
	if(!ctx->files[fd]) return -EINVAL;
	if(!ctx->files[fd]->msg_queue) return -EINVAL;
	for(int i =0;i<ctx->files[fd]->msg_queue->member_info->member_count;i++){
		if(ctx->files[fd]->msg_queue->member_info->member_pid[i] == ctx->pid){
			for(int j =i;j<MAX_MEMBERS-1;j++){
				ctx->files[fd]->msg_queue->member_info->member_pid[j] = ctx->files[fd]->msg_queue->member_info->member_pid[j+1];
				for(int k=0;k<MAX_MEMBERS;k++){
					ctx->files[fd]->msg_queue->block_process[j][k] = ctx->files[fd]->msg_queue->block_process[j+1][k];
				}
			}
			ctx->files[fd]->msg_queue->member_info->member_count--;
			for(int k=0;k<MAX_MEMBERS;k++){
				ctx->files[fd]->msg_queue->block_process[ctx->files[fd]->msg_queue->member_info->member_count][k] = -1;
			}
			for(int j=0;j<MAX_MEMBERS;j++){
				for(int k =0;k<MAX_MEMBERS;k++){
					if(ctx->files[fd]->msg_queue->block_process[j][k] == ctx->pid)
						ctx->files[fd]->msg_queue->block_process[j][k] =-1;
				}
			}

			//! message remove handling
			int msg_count_temp =0;
			// struct message* msg_buffer_temp[128];
			for(int j=0;j<ctx->files[fd]->msg_queue->msg_count;j++){
				if(ctx->files[fd]->msg_queue->msg_buffer[j]->to_pid!=ctx->pid){
					*ctx->files[fd]->msg_queue->msg_buffer[msg_count_temp++] = *ctx->files[fd]->msg_queue->msg_buffer[j];
				}
			}
			ctx->files[fd]->msg_queue->msg_count = msg_count_temp;

			// //! if member count goes to zero
			if(ctx->files[fd]->msg_queue->member_info->member_count == 0){
				free_msg_queue_buffer(ctx->files[fd]->msg_queue->msg_buffer[0]);
				free_msg_queue_info(ctx->files[fd]->msg_queue);
				free_file_object(ctx->files[fd]);
			}
			ctx->files[fd] = NULL;
			return 0;
		}
	}
	return -EINVAL;
}
