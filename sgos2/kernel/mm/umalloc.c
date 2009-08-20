//user space memory bigblock allocator
//provide a lot of memory for running SGOS2 user process
//use 0x00000000 - 0x80000000  2GB  VIRTUAL_MEMORY

#include <sgos.h>
#include <bigblock.h>
#include <debug.h>
#include <process.h>
#include <mm.h>

// 用户空间内存初始化
void umalloc_init( PROCESS* proc )
{
	bb_init_block( &proc->memory_info.umem_mgr, 
		0x00100000,		//address
		0x80000000		//size
		);
	//
}

// 分配指定地址的用户态空间内存
void*	umalloc_ex(PROCESS* proc, size_t addr, size_t siz)
{
	MEMORY_INFO* info;
	void* ret;
	if( !proc )
		return NULL;
	//change some mem info
	info = &proc->memory_info;
	info->umem_size += siz;
	if( info->umem_size > info->max_umem ){
		info->umem_size -= siz;
		PERROR("##user memory used out!");
		return NULL;
	}
	ret = bb_alloc_ex( &info->umem_mgr, addr, siz );
	if( ret ){
		return ret;
	}
	//no memory?? 分配失败就撤销增加
	info->umem_size -= siz;
	return NULL;
}

// 分配用户态空间的内存
void*	umalloc(PROCESS* proc, size_t siz)
{
	MEMORY_INFO* info;
	void* ret;
	if( !proc )
		return NULL;
	//change some mem info
	info = &proc->memory_info;
	info->umem_size += siz;
	if( info->umem_size > info->max_umem ){
		info->umem_size -= siz;
		PERROR("##user memory used out!");
		return NULL;
	}
	
	ret = bb_alloc( &info->umem_mgr, siz );
	if( ret )
		return ret;
	//no memory??
	info->umem_size -= siz;
	PERROR("##user memory used out!");
	return NULL;
}

// 释放用户空间的内存
void	ufree(PROCESS* proc, void* p)
{
	MEMORY_INFO* info;
	size_t siz;
	if( proc ){
		//info之前忘了赋值。。。
		//弄了乱出八糟的错误，调试了一天。。。
		info = &proc->memory_info;
		siz = bb_free( &info->umem_mgr, p );
		//change some mem info
		info = &proc->memory_info;
		info->umem_size -= siz;
		if( info->umem_size < 0 )
			info->umem_size = 0;
	}
}

int	ucheck_allocated(PROCESS* proc, size_t addr)
{
	MEMORY_INFO* info;
	if( proc ){
		//change some mem info
		info = &proc->memory_info;
		return bb_check_allocated( &info->umem_mgr, addr );
	}
	return 0;
}


