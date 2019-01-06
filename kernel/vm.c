#include "vm.h"
#include <zjunix/slab.h>
#include <zjunix/utils.h>
#include <zjunix/pc.h>
#include <driver/vga.h>
#include <arch.h>

struct mm_struct* mm_create()
{
	struct mm_struct* mm;
	mm = kmalloc(sizeof(*mm));
#ifdef VMA_AREA_DEBUG
	kernel_printf("mm_create:%x\n", mm);
#endif
	if(mm)
	{
		kernel_memset(mm , 0 , sizeof(*mm));
		mm->pgd = kmalloc(PAGE_SIZE);
		if(mm->pgd)
		{
			kernel_memset(mm->pgd,0,PAGE_SIZE)；
			return mm;
		}
#ifdef VMA_AREA_DEBUG
		kernel_printf("mm_create failed!\n");
#endif
		kfree(mm);
	}
}

void mm_delete(struct mm_struct* mm)
{
#ifdef VMA_AREA_DEBUG
	kernel_printf("mm_delete:pgd%x\n",mm->pdg);
#endif
	pgd_delete(mm->pgd);
	exit_map(mm);
#ifdef VMA_AREA_DEBUG
	kernel_printf("exit_map finished!\n");
#endif
	kfree(mm);	
}

void pgd_delete(pgd_t* pgd)
{
	int i, j;
	unsigned int pde;// page directory item
	unsigned int pte;//page table item
	unsigned int* pde_ptr;
	int i = 0;
	while(i<1024)
	{
		pde = pgd[i];
		pde = pde&PAGE_MASK;
		if(pde==0)//not exist in 2 level page table
			continue;
#ifdef	VMA_AREA_DEBUG
		kernel_printf("delete pde:%x\n", pde);
#endif
		pde_ptr = (unsigned int*)pde;
			for(j=0;,j<1024,j++)
			{
				pte = pde_ptr[j];
				pte = pte&PAGE_MASK;//页目录中的每一项的内容（每项8个字节，64位）低12位之后的M-12位用来放一个页表（页表 放在一个物理页中）的物理地址，低12bit放着一些标志。

				if(pte!=0){
#ifdef VMA_AREA_DEBUG
				kernel_printf("delete pte:%x\n", pte);
#endif			kfree((void*)pte);
			}
		}
		kfree((void*)pde_ptr);	
		i++;	
	}
	kfree(pgd);
#ifdef VMA_AREA_DEBUG
	kernel_printf("pgd_delete success\n");
#endif
	return;
}

struct vm_area_struct *vma_merge(struct mm_struct *mm,struct vm_area_struct *prev, unsigned long addr,unsigned long end, unsigned long vm_flags)
{
	struct vm_area_struct *area, *next;
 	unsigned long pglen = (end-addr)>>PAGE_SHIFT;
	/*
	 * We later require that vma->vm_flags == vm_flags,
	 * so this tests vma->vm_flags & VM_SPECIAL, too.
	 */
	if (vm_flags & VM_SPECIAL)
		return NULL;
 
	if (prev)//指定了先驱vma，则获取先驱vma的后驱vma
		next = prev->vm_next;
	else     //否则指定mm的vma链表中的第一个元素为后驱vma
		next = mm->mmap;
	area = next;
 
	/*后驱节点存在，并且后驱vma的结束地址和给定区域的结束地址相同，
	  也就是说两者有重叠，那么调整后驱vma*/
	if (next && next->vm_end == end)		/* cases 6, 7, 8 */
		next = next->vm_next;
 
	/*
	 * 先判断给定的区域能否和前驱vma进行合并，需要判断如下的几个方面:
	   1.前驱vma必须存在
	   2.前驱vma的结束地址正好等于给定区域的起始地址
	   3.两者的struct mempolicy中的相关属性要相同，这项检查只对NUMA架构有意义
	   4.其他相关项必须匹配，包括两者的vm_flags，是否映射同一个文件等等
	 */
	if (prev && prev->vm_end == addr ) {
		/*
		 *确定可以和前驱vma合并后再判断是否能和后驱vma合并，判断方式和前面一样，
		  不过这里多了一项检查，在给定区域能和前驱、后驱vma合并的情况下还要检查
		  前驱、后驱vma的匿名映射可以合并
		 */
		
		vma_adjust(prev, prev->vm_start,next->end, prev->vm_pgoff, NULL);
		return prev;
	}
 
	/*
	 * Can this new request be merged in front of next?
	 */
	 /*如果前面的步骤失败，那么则从后驱vma开始进行和上面类似的步骤*/
	if (next && end == next->vm_start ) {
		if (prev && addr < prev->vm_end)	/* case 4 */
			vma_adjust(prev, prev->vm_start,addr, prev->vm_pgoff, NULL);
		else					/* cases 3, 8 */
			vma_adjust(area, addr, next->vm_end, next->vm_pgoff - pglen, NULL);
		return area;
	}
 
	return NULL;
}




 /* Look up the first VMA which satisfies  addr<vm_end,  NULL if none. */
struct vm_area_struct* find_vma(struct mm_struct* mm, unsigned long addr)
{
	struct vm_area_struct *vma = NULL;
	if(mm)
	{	
		/* Check the cache first. */
        /* (Cache hit rate is typically around 35%.) */
 		vma = mm->mmap_cache;
 		if(!(vma && vma->vm_end > addr && vma->vm_start <= addr))
 		{
 			if(!mm->mmap_avl)//goto the innear list
 			{
 				vma = mm->mmap;
 				while(vma&&vma->vm_end<=addr)
 				{
 					vma = vma->vm_next;
 				}
 			}
 			else//goto the AVL tree
 			{
 				struct vm_area_struct *tree  = mm->mmap_avl;
 				vma = NULL;
 				for(;;)
 				{
 					if(tree==VM_AVL_EMPTY)
 						break;
 					if(tree->vm_end>addr){
 						vma = tree;
 						if(tree->vm_start<=addr)
 							break;
 						tree = tree->vm_avl_left;
 					}
 					else 
 						tree = tree->vm_avl_right;
 				}

 			}
 			if(vma)
 				mm->mmap_cache = vma;
 		}
	}
	return vma;
}

// Return the first vma with ending address greater than addr, recording the previous vma
struct vm_area_struct *find_vma_intersection(struct mm_struct* mm, unsigned long start_addr,unsigned long end_addr)
{
	struct vm_area_struct *vma = find_vma(mm, start_addr);
	if(vma&&end_addr<=vma->vm_start)
		vma=0;
	return vma;
}


// Get unmapped area starting after addr        
struct vm_area_struct *find_vma_and_prev(struct mm_struct* mm, unsigned long addr, struct vm_area_struct** prev)
{
	struct vm_area_struct *vma = NULL;
	*prev = 0;
	if(mm)
	{
		vma = mm->mmap;
		while(vma)
		{
			if(vma->vm_end>addr){
				mm->mmap_cache = vma;
				break;
			}
			*prev = vma;
			vma = vma->vm_next;
		}
	}
	return vma;
}

unsigned long get_unmapped_area(unsigned long addr, unsigned long len, ,unsigned long flags)
{
	struct vm_area_struct* vma;
	struct mm_struct* mm = current_task->mm; //global variable

	addr = Allign(addr, PAGE_SIZE);
	if(addr+len>KERNEL_ENTRY)
		return -1;
	/*
     * 以下分别判断：
     * 1: 请求分配的长度是否小于进程虚拟地址空间大小
     * 2: 新分配的虚拟地址空间的起始地址是否在mmap_min_addr(允许分配虚拟地址空间的最低地址)之上
     * 3: vma是否空
     * 4: vma非空，新分配的虚拟地址空间，是否与相邻的vma重合
     */
    if(addr)
    {
		if(addr&&addr+len<=KERNEL_ENTRY)
		{		
			vma = find_vma(mm,addr);
			for(;;vma = vma->vm_next)
			{
				if(addr+len>KERNEL_ENTRY)
					return -1;
				if(!vma||addr+len<=vma->vm_start)
					return addr;
			}
			addr = vma->vm_end;
		}
	}
	return 0;
}

void insert_vma_struct(struct mm_struct* mm, struct vm_area_struct* area)
{	
	//find the vma preceding addr
	struct vm_area_struct* vma = mm->mmap;
	struct vm_area_struct* prev = 0;
	while(vma)
	{
		if(area->vm_start<vma->vm_start)
			break;
		prev = vma;
		vma = vma->vm_next;
	}
	vma = prev;
#ifdef VMA_AREA_DEBUG
	kernel_printf("Insert: %x	%x\n", vma, area->vm_start);
#endif
	if(!vma)
	{
#ifdef VMA_AREA_DEBUG
		kernel_printf("mmap init.\n");
#endif
		mm->mmap = area;
		area->vm_next = 0;
	}
	else{
		area->vm_next = vma->vm_next;
		vma->vm_next = area;
	}
	mm->map_count++;
}

//map
unsigned long do_mmap(unsigned long addr, unsigned long len, unsigned flags)
{
	struct mm_struct* mm = current_task->mm;
	struct vm_area_struct* vma, *prev;
	if(!len)
		return addr;
	addr = get_unmapped_area(addr, len, flags);
	vma = kmalloc(sizeof(struct vm_area_struct));
	if(!vma)
		return -1;
	vma->vm_mm = mm;
	vma->vm_start = addr;
	vma->vm_end = Allign(addr+len, PAGE_SIZE);
#ifdef VMA_AREA_DEBUG
	kernel_printf("MAP: %x	%x\n", vma->vm_start, vma->vm_end);
#endif
	insert_vma_struct(mm,vma);
	return addr;
}

int do_unmap(unsigned long addr, unsigned long len)
{
	unsigned long end;
	struct mm_struct* mm = current_task->mm;
	struct mm_area_struct *vma, *prev;
	if(addr>KERNEL_ENTRY||len+addr>KERNEL_ENTRY)//if the addr is bigger than kernal_entry, or the length is 0
		return -1;
	//find the first overlapping VMA
	vma = find_vma_and_prev(mm,addr, &prev);
	if(!vma)
		return 0;
	end  = addr + len;
	if(vma->vm_start>=end)
		return 0;
#ifdef VMA_AREA_DEBUG
	kernel_printf("do_unmap: %x	%x\n", addr, vma->vm_start);
#endif
//match the length
	if(vma->vm_end>=end)
	{
#ifdef	VMA_AREA_DEBUG
		kernel_printf("length match.\n");
#endif
		if(!prev)
		{
			mm->mmap = vma->vm_next;//if prev ==NLLL, mm->mmap get the fisrt address
		}
		else
		{
			prev->vm_next = vma->vm_next;
		}
		kfree(vma);
		mm->map_count--;
#ifdef	VMA_AREA_DEBUG
		kernel_printf("unmapped finished! %d vmas left\n", mm->map_count);
#endif
		return 0;
	}
//the length doesn't match
#ifdef	VMA_AREA_DEBUG
		kernel_printf("length mismatch!\n");
#endif
		return 1;
		if(mm->locked_vm)
		{
			struct vm_area_struct *tmp = vma;
			while(tmp&&tmp->vm_start<end)
			{
				if(tmp->vm_flags&VM_LOCKED)
				{
					mm->locked_vm -=vma_pages(tmp);
					munlock_vma_pages_all(tmp);
				}
				tmp = tmp->vm_next;
			}
		}
}

//free the VMAs
void exit_map(struct mm_struct* mm)
{
	struct vm_area_struct * vmap = mm->mmap;
	mm->mmap_cache = 0;
	mm->mmap = mm->mmap_cache;
	while(vmap)
	{
		struct vm_area_struct *next = vmap->vm_next;
		kfree(vmap);
		mm->map_count--;
		vmap = next;
	}
	if(mm->map_count)
	{
#ifdef	VMA_AREA_DEBUG
		kernel_printf("exit mmap bug! %dVMA left.\n", mm->map_count);
#endif
		while(1);
	}
}

int is_in_vma(unsigned long addr)
{
	struct mm_struct* mm = current_task->mm;
	struct vm_area_struct *vma = find_vma(mm,addr);
	if(!vma||vma->vm_start>addr)
		return 0;
	else
		return 1;
}





