#ifndef _ZJUNIX_VM_H
#define _ZJUNIX_VM_H

#include <zjunix/page.h>
#define VM_AVL_EMPTY NULL
#define VM_NONE		0x00000000

#define VM_READ		0x00000001	/* currently active flags */
#define VM_WRITE	0x00000002
#define VM_EXEC		0x00000004
#define VM_SHARED	0x00000008

/* mprotect() hardcodes VM_MAYREAD >> 4 == VM_READ, and so for r/w/x bits. */
#define VM_MAYREAD	0x00000010	/* limits for mprotect() etc */
#define VM_MAYWRITE	0x00000020
#define VM_MAYEXEC	0x00000040
#define VM_MAYSHARE	0x00000080

#define VM_GROWSDOWN	0x00000100	/* general info on the segment */
#define VM_UFFD_MISSING	0x00000200	/* missing pages tracking */
#define VM_PFNMAP	0x00000400	/* Page-ranges managed without "struct page", just pure PFN */
#define VM_DENYWRITE	0x00000800	/* ETXTBSY on write attempts.. */
#define VM_UFFD_WP	0x00001000	/* wrprotect pages tracking */

#define VM_LOCKED	0x00002000
#define VM_IO           0x00004000	/* Memory mapped I/O or similar */

					/* Used by sys_madvise() */
#define VM_SEQ_READ	0x00008000	/* App will access data sequentially */
#define VM_RAND_READ	0x00010000	/* App will not benefit from clustered reads */

#define VM_DONTCOPY	0x00020000      /* Do not copy this vma on fork */
#define VM_DONTEXPAND	0x00040000	/* Cannot expand with mremap() */
#define VM_LOCKONFAULT	0x00080000	/* Lock the pages covered when they are faulted in */
#define VM_ACCOUNT	0x00100000	/* Is a VM accounted object */
#define VM_NORESERVE	0x00200000	/* should the VM suppress accounting */
#define VM_HUGETLB	0x00400000	/* Huge TLB Page VM */
#define VM_SYNC		0x00800000	/* Synchronous page faults */
#define VM_ARCH_1	0x01000000	/* Architecture-specific flag */
#define VM_WIPEONFORK	0x02000000	/* Wipe VMA contents in child. */
#define VM_DONTDUMP	0x04000000	/* Do not include in the core dump */

#ifdef CONFIG_MEM_SOFT_DIRTY
# define VM_SOFTDIRTY	0x08000000	/* Not soft dirty clean area */
#else
# define VM_SOFTDIRTY	0
#endif

#define VM_MIXEDMAP	0x10000000	/* Can contain "struct page" and pure PFN pages */
#define VM_HUGEPAGE	0x20000000	/* MADV_HUGEPAGE marked this vma */
#define VM_NOHUGEPAGE	0x40000000	/* MADV_NOHUGEPAGE marked this vma */
#define VM_MERGEABLE	0x80000000	/* KSM may merge identical pages */
#define VM_DEFALUT_ATTR   0x0f
#define VM_CHANGE2PHY   0x80000000

struct mm_struct;
struct mm_struct{
			struct vm_area_struct *mmap;	//list of VMA
			struct vm_area_struct *mmap_avl;//tree of VMA
			struct vm_area_struct *mmap_cache;//LAST FIND VMA RESULT
			int map_count;//number of VMAs

			pgd_t *pgd;//page table pointer

			unsigned long start_code, end_code, start_data, end_data;
			unsigned long start_brk, brk, start_stack;
			unsigned long tatal_vm, locked_vm, shared_vm, exec_vm;
			//进程地址空间的大小，锁住无法换页的个数，共享文件内存映射的页数，可执行内存映射中的页数

};

struct vm_area_struct{
			unsigned long vm_start;		/* Our start address within vm_mm. */
			unsigned long vm_end;		/* The first byte after our end address
						   				within vm_mm. */
			struct mm_struct *vm_mm;    //the address space we belong to 
			//linked list of VM areas per task, sorted by address
			struct vm_area_struct *vm_next, *vm_prev;
 /* 该vma的在一个进程的vma链表中的前驱vma和后驱vma指针，链表中的vma都是按地址来排序的*/ 
/* AVL tree of VM areas per task, sorted by address */
			short vm_avl_height;
			struct vm_area_struct *vm_avl_left;
			struct vm_area_struct *vm_avl_right;
			unsigned long vm_flags;//标识集
			unsigned long vm_pgoff; // 映射文件的偏移量，以page_size为单位
};

struct mm_struct* mm_create();
void mm_delete(struct mm_struct* mm);
unsigned long do_mmap(unsigned long addr, unsigned long len, unsigned long flags);//完成可执行映像向虚存区域的映射，建立有关的虚存区域
int do_unmap(unsigned long addr, unsigned long len);//取消断开可执行映像向虚存区域的映射，删除有关的虚存区域
int is_in_vma(unsigned long addr);
extern void set_tlb_asid(unsigned int asid);
unsigned long mmap_region(unsigned long addr, unsigned long len, unsigned long flags);
struct vm_area_struct *vma_merge(struct mm_struct *mm,struct vm_area_struct *prev, unsigned long addr,unsigned long end, unsigned long vm_flags);


#endif