#include <debug.h>
#include <context.h>
#include <entry.h>
#include <lib.h>
#include <memory.h>


/*****************************HELPERS******************************************/

/* 
 * allocate the struct which contains information about debugger
 *
 */
struct debug_info *alloc_debug_info()
{
	struct debug_info *info = (struct debug_info *) os_alloc(sizeof(struct debug_info)); 
	if(info)
		bzero((char *)info, sizeof(struct debug_info));
	return info;
}

/*
 * frees a debug_info struct 
 */
void free_debug_info(struct debug_info *ptr)
{
	if(ptr)
		os_free((void *)ptr, sizeof(struct debug_info));
}

/*
 * allocates memory to store registers structure
 */
struct registers *alloc_regs()
{
	struct registers *info = (struct registers*) os_alloc(sizeof(struct registers)); 
	if(info)
		bzero((char *)info, sizeof(struct registers));
	return info;
}

/*
 * frees an allocated registers struct
 */
void free_regs(struct registers *ptr)
{
	if(ptr)
		os_free((void *)ptr, sizeof(struct registers));
}

/* 
 * allocate a node for breakpoint list 
 * which contains information about breakpoint
 */
struct breakpoint_info *alloc_breakpoint_info()
{
	struct breakpoint_info *info = (struct breakpoint_info *)os_alloc(
		sizeof(struct breakpoint_info));
	if(info)
		bzero((char *)info, sizeof(struct breakpoint_info));
	return info;
}

/*
 * frees a node of breakpoint list
 */
void free_breakpoint_info(struct breakpoint_info *ptr)
{
	if(ptr)
		os_free((void *)ptr, sizeof(struct breakpoint_info));
}

/*
 * Fork handler.
 * The child context doesnt need the debug info
 * Set it to NULL
 * The child must go to sleep( ie move to WAIT state)
 * It will be made ready when the debugger calls wait_and_continue
 */
void debugger_on_fork(struct exec_context *child_ctx)
{
	child_ctx->dbg = NULL;	
	child_ctx->state = WAITING;	
}


/******************************************************************************/

/* This is the int 0x3 handler
 * Hit from the childs context
 */
long int3_handler(struct exec_context* ctx)
{
	struct exec_context *parent_ctx = get_ctx_by_pid(ctx->ppid);
	if(!parent_ctx)return -1;
	if(!parent_ctx->dbg)return -1;
	
	*((u64*)(ctx->regs.entry_rsp) - 1) = ctx->regs.rbp;
	ctx->regs.entry_rsp -= 8;
	ctx->state = WAITING;
	parent_ctx->state = READY;

	//!storing backtrace info
	parent_ctx->dbg->backtrace_info[0] = ctx->regs.entry_rip-1;
	int idx=1;
	u64* rbp_address = (u64*)(ctx->regs.entry_rsp);
	u64 addr = (u64)*(rbp_address+1);
	while(addr!=END_ADDR && idx < MAX_BACKTRACE){
		parent_ctx->dbg->backtrace_info[idx++] = addr;
		rbp_address = (u64*)*(rbp_address);
		addr = (u64)*(rbp_address+1);
	}
	if(idx < MAX_BACKTRACE)parent_ctx->dbg->backtrace_info[idx] = END_ADDR;

	//! adding return value
	parent_ctx->regs.rax = (s64)(ctx->regs.entry_rip-1);

	//! schedule parent
	schedule(parent_ctx);
	return 0;
}

/*
 * Exit handler.
 * Called on exit of Debugger and Debuggee 
 */
void debugger_on_exit(struct exec_context *ctx)
{
	if(ctx->dbg){
		struct breakpoint_info* bpoint_info = ctx->dbg->head;
		while (bpoint_info)
		{
			struct breakpoint_info* next = 	bpoint_info->next;
			free_breakpoint_info(bpoint_info);
			bpoint_info = next;
		}
		free_debug_info(ctx->dbg);
		ctx->dbg = NULL;
	}
	struct exec_context* parent_ctx = get_ctx_by_pid(ctx->ppid);
	if(parent_ctx && parent_ctx->dbg){
		parent_ctx->state = READY;
		//!add return value
		parent_ctx->regs.rax = (s64)CHILD_EXIT;
	}
}

/*
 * called from debuggers context
 * initializes debugger state
 */
int do_become_debugger(struct exec_context *ctx)
{
	ctx->dbg = alloc_debug_info();
	if(!ctx->dbg)return -1;
	ctx->dbg->head = NULL;
	ctx->dbg->size = 0;
	ctx->dbg->breakpt_num =0;
	return 0;
}

/*
 * called from debuggers context
 */
int do_set_breakpoint(struct exec_context *ctx, void *addr)
{
	if(!ctx->dbg)return -1;
	struct breakpoint_info* bpoint_info = ctx->dbg->head;
	
	while(bpoint_info){
		if(bpoint_info->addr == (u64)addr){
			bpoint_info->status =1;
			*(char*)addr = INT3_OPCODE;
			return 0;
		}
		bpoint_info = bpoint_info->next;
	}

	if(ctx->dbg->size >= MAX_BREAKPOINTS){
		return -1;
	}
	struct breakpoint_info* breakpoint_head = alloc_breakpoint_info();
	if(!breakpoint_head)return -1;
	breakpoint_head->num = ++ctx->dbg->breakpt_num;
	breakpoint_head->status =1;
	*(char*)addr = INT3_OPCODE;
	breakpoint_head->addr = (u64)addr;
	breakpoint_head->next = ctx->dbg->head;
	ctx->dbg->head = breakpoint_head;	
	ctx->dbg->size++;
	
	return 0;
}

/*
 * called from debuggers context
 */
int do_remove_breakpoint(struct exec_context *ctx, void *addr)
{
	if(!ctx->dbg)return -1;
	struct breakpoint_info* bpoint_info = ctx->dbg->head;
	struct breakpoint_info* bpoint_parent = NULL;
	while(bpoint_info){
		if(bpoint_info->addr == (u64)addr){
			if(!bpoint_parent){
				ctx->dbg->head = bpoint_info->next;
			}else{
				bpoint_parent->next = bpoint_info->next;
			}
			free_breakpoint_info(bpoint_info);
			ctx->dbg->size--;
			*(char*)addr = PUSHRBP_OPCODE;
			return 0;
		}
		bpoint_parent = bpoint_info;
		bpoint_info = bpoint_info->next;
	}

	return -1;
}

/*
 * called from debuggers context
 */
int do_enable_breakpoint(struct exec_context *ctx, void *addr)
{
	if(!ctx)return -1;
	if(!ctx->dbg) return -1;
	struct breakpoint_info* bpoint_info = ctx->dbg->head;
	while(bpoint_info){
		if(bpoint_info->addr == (u64)addr){
			bpoint_info->status = 1;
			*(char*)bpoint_info->addr = INT3_OPCODE;
			return 0;
		}
		bpoint_info = bpoint_info->next;
	}
	return -1;
}

/*
 * called from debuggers context
 */
int do_disable_breakpoint(struct exec_context *ctx, void *addr)
{
	if(!ctx)return -1;
	if(!ctx->dbg) return -1;
	struct breakpoint_info* bpoint_info = ctx->dbg->head;
	while(bpoint_info){
		if(bpoint_info->addr == (u64)addr){
			bpoint_info->status = 0;
			*(char*)bpoint_info->addr = PUSHRBP_OPCODE;
			return 0;
		}
		bpoint_info = bpoint_info->next;
	}
	return -1;
}

/*
 * called from debuggers context
 */ 
int do_info_breakpoints(struct exec_context *ctx, struct breakpoint *ubp)
{
	if(!ubp)return -1;
	if(!ctx)return -1;
	if(!ctx->dbg)return -1;
	struct breakpoint_info* bpoint_info = ctx->dbg->head;
	int idx =0;
	while(bpoint_info){
		ubp[ctx->dbg->size-idx-1].num = bpoint_info->num;
		ubp[ctx->dbg->size-idx-1].addr = bpoint_info->addr;
		ubp[ctx->dbg->size-idx-1].status = bpoint_info->status;
		idx++;
		bpoint_info = bpoint_info->next;
	}
	return ctx->dbg->size;
}

/*
 * called from debuggers context
 */
int do_info_registers(struct exec_context *ctx, struct registers *regs)
{
	struct exec_context* child_ctx = NULL;
	for(int ctr =1;ctr < MAX_PROCESSES;ctr++){
		struct exec_context *new_ctx = get_ctx_by_pid(ctr);
		if(new_ctx->state != UNUSED) {
			if(new_ctx->ppid == ctx->pid){
				child_ctx = new_ctx;
				break;
			}
		}
	}
	if(!child_ctx)return -1;
	regs->r15 = child_ctx->regs.r15;
	regs->r14 = child_ctx->regs.r14;
	regs->r13 = child_ctx->regs.r13;
	regs->r12 = child_ctx->regs.r12;
	regs->r11 = child_ctx->regs.r11;
	regs->r10 = child_ctx->regs.r10;
	regs->r9 = child_ctx->regs.r9;
	regs->r8 = child_ctx->regs.r8;
	regs->rbp = child_ctx->regs.rbp;
	regs->rdi = child_ctx->regs.rdi;
	regs->rsi = child_ctx->regs.rsi;
	regs->rdx = child_ctx->regs.rdx;
	regs->rcx = child_ctx->regs.rcx;
	regs->rbx = child_ctx->regs.rbx;
	regs->rax = child_ctx->regs.rax;
	regs->entry_rip = child_ctx->regs.entry_rip-1;  
	regs->entry_cs = child_ctx->regs.entry_cs;  
	regs->entry_rflags = child_ctx->regs.entry_rflags;
	regs->entry_rsp = child_ctx->regs.entry_rsp+8;
	regs->entry_ss = child_ctx->regs.entry_ss;
	return 0;
}

/* 
 * Called from debuggers context
 */
int do_backtrace(struct exec_context *ctx, u64 bt_buf)
{
	if(!ctx->dbg)return -1;
	
	u64* bt = (u64*)bt_buf;
	if(!bt)return -1;
	int count=0;
	while(count < MAX_BACKTRACE && ctx->dbg->backtrace_info[count]!=END_ADDR){
		bt[count] = ctx->dbg->backtrace_info[count];
		count++;
	}
	return count;
}


/*
 * When the debugger calls wait
 * it must move to WAITING state 
 * and its child must move to READY state
 */

s64 do_wait_and_continue(struct exec_context* ctx)
{
	if(!ctx->dbg)return -1;
	struct exec_context* child_ctx = NULL;
	for(int ctr =1;ctr < MAX_PROCESSES;ctr++){
		struct exec_context *new_ctx = get_ctx_by_pid(ctr);
		if(new_ctx->state != UNUSED) {
			if(new_ctx->ppid == ctx->pid){
				child_ctx = new_ctx;
				break;
			}
		}
	}
	if(!child_ctx)return -1;
	ctx->state = WAITING;
	child_ctx->state = READY;
	//! updating rax values for return values
	ctx->regs.rax = (s64)-1;
	schedule(child_ctx);
	return -1;
}

