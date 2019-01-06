#include "exc.h"

#include <driver/vga.h>
#include <zjunix/pc.h>
#include <zjunix/utils.h>
#include <zjunix/slab.h>
#include <zjunix/page.h>
#include <driver/ps2.h>

#pragma GCC push_options
#pragma GCC optimize("O0")

exc_fn exceptions[32];
int count = 0;
int count2 = 0;
void do_exceptions(unsigned int status, unsigned int cause, context* pt_context) {
    int index = cause >> 2;
    index &= 0x1f;
    #ifdef  TLB_DEBUG
    unsigned int count;
    #endif

    if (index == 2 || index == 3) {
        tlb_refill(bad_addr);
#ifdef TLB_DEBUG
        kernel_printf("refill done\n");
#endif
        return ;
    }
    
    if (exceptions[index]) {
        exceptions[index](status, cause, pt_context);
    } else {
        task_struct* pcb;
        unsigned int badVaddr;
        asm volatile("mfc0 %0, $8\n\t" : "=r"(badVaddr));
        pcb = get_curr_pcb();
        kernel_printf("\nProcess %s exited due to exception cause=%x;\n", pcb->name, cause);
        kernel_printf("status=%x, EPC=%x, BadVaddr=%x\n", status, pcb->context.epc, badVaddr);
        pc_kill_syscall(status, cause, pt_context);
        while (1)
            ;
    }
}

void register_exception_handler(int index, exc_fn fn) {
    index &= 31;
    exceptions[index] = fn;
}

void init_exception() {
    // status 0000 0000 0000 0000 0000 0000 0000 0000
    // cause 0000 0000 1000 0000 0000 0000 0000 0000
    asm volatile(
        "mtc0 $zero, $12\n\t"
        "li $t0, 0x800000\n\t"
        "mtc0 $t0, $13\n\t");
}

void tlb_refill(unsigned int bad_addr)
{
    pgd_t* pgd;
    unsigned int pde, pte, pte_near;
    unsigned int pde_index, pte_index, pte_near_index;
    unsigned int pte_phyaddr, pte_near_phyaddr;
    unsigned int* pde_ptr, pte_ptr;
    unsigned int entry_lo0, entry_lo1;
    unsigned int entry_hi;

#ifdef TLB_DEBUG
    unsigned int entry_hi_test;
        asm volatile( 
            "mfc0  $t0, $10\n\t"
            "move  %0, $t0\n\t"
            : "=r"(entry_hi_test)
        );
    kernel_printf("tlb_refill: bad_addr = %x    entry_hi = %x \n", bad_addr, entry_hi_test);
    kernel_printf("%x  %d\n", current_task, current_task->pid);
#endif
    if(current_task->mm==0)
    {
#ifdef  TLB_DEBUG
     kernel_printf("tlb_refill: mm is null!!!  %d\n", current_task->pid);
#endif
         while(1);   
    }

    pgd = current_task->mm->pgd;
    if(!pgd)
    {
#ifdef  TLB_DEBUG
        kernel_printf("tlb_refill:pgd == NULL\n");
#endif
        while(1);
    }

    bad_addr = bad_addr & PAGE_SIZE;
    pde_index = bad_addr>>PGD_SHIFT;
    pde = pgd[pde_index];
    pde = pde & PAGE_MASK;

    if(!pde)//the two level page not exist  
    {
        pde = (unsigned int)kmalloc(PAGE_SIZE);
#ifdef TLB_DEBUG
        kernel_printf("two level page not exist.\n");
#endif
        if(!pde)
        {
#ifdef
        kernel_printf("tlb_refill: alloc two level page failed.\n ");
#endif
        while(1);            
        }
        kernel_memset((void*)pde, 0 , PAGE_SIZE);
        pgd[pde_index] = pde;
        pgd[pde_index] = pgd[pde_index] & PAGE_MASK;
        pgd[pde_index] = pgd[pde_index] | VM_DEFALUT_ATTR;//ATTR

    }

#ifdef TLB_DEBUG
    kernel_printf("tlb_refill:%x\n", pde);
#endif

    pde_ptr = (unsigned int*)pde;
    pte_index = (bad_addr >> PAGE_SHIFT) & INDEX_MASK;
    pte = pde_ptr[pte_index];
    pte = pte & PAGE_MASK;
    if(!pte)
    {
#ifdef TLB_DEBUG
        kernel_printf("page not exists.\n");
#endif
        pte = (unsigned int)kmalloc(PAGE_SIZE);
        if(!pte)
        {
#ifdef TLB_DEBUG
            kernel_printf("tlb_refill: alloc page failed.\n");
#endif      
            while(1);
        }
        kernel_memset((void*)pte, 0, PAGE_SIZE);
        pde_ptr[pte_index] = pte;
        pde_ptr[pte_index] = pde_ptr[pte_index] & PAGE_MASK;
        pde_ptr[pte_index] = pde_ptr[pte_index] | VM_DEFALUT_ATTR;//attr

    }  
    pte_near_index = pte_index ^ 0x01;
    pte_near = pde_ptr[pte_near_index];
    pte_near = pte_near & PAGE_MASK;

#ifdef  TLB_DEBUG
    kernel_printf("pte: %x pte_index: %x  pte_near_index: %x\n", pte, pte_index, pte_index_near);
#endif    
    if(!pte_near)//the near item is empty
    {
#ifdef  TLB_DEBUG
        kernel_printf("page near is not exists.\n");
#endif
        pte_near = (unsigned int)kmalloc(PAGE_SIZE);

        if(!pte_near)
        {
#ifdef TLB_DEBUG
            kernel_printf("tlb_refill:alloc near page failed.\n");
#endif
            while(1);
        }
        kernel_memset((void*)pte_near , 0 ,PAGE_SIZE);
        pde_ptr[pte_near_index] = pte_near;
        pde_ptr[pte_near_index] = pde_ptr[pte_near_index] & PAGE_MASK;
        pde_ptr[pte_near_index] = pde_ptr[pte_near_index] | VM_DEFALUT_ATTR;//attr
    }
    //change to the phy address
    pte_phyaddr = pte - VM_CHANGE2PHY;
    pte_near_phyaddr = pte_near - VM_CHANGE2PHY;
#ifdef TLB_DEBUG
        kernel_printf("pte_phyaddr:%x, pte_near_phyaddr:%x\n", pte_phyaddr, pte_near_phyaddr);
#endif
    
    if(pte_index & 0x01 == 0)// even
    {
        entry_lo0 = (pte_phyaddr>>PAGE_SHIFT)<<6;
        entry_lo1 = (pte_near_phyaddr>>PAGE_SHIFT)<<6;

    }   
    else     
    {
        entry_lo0 = (pte_near_phyaddr>>PAGE_SHIFT)<<6;
        entry_lo1 = (pte_phyaddr>>PAGE_SHIFT)<<6l
    }

    entry_lo0 |= (3 << 3);   //cached ??
    entry_lo1 |= (3 << 3);   //cached ??
    entry_lo0 |= 0x06;      //D = 1, V = 1, G = 0
    entry_lo1 |= 0x06;

    entry_hi = (bad_addr & PAGE_MASK) & (~(1<<PAGE_SHIFT));
    entry_hi = entry_hi | current_task->ASID;

#ifdef TLB_DEBUG
    kernel_printf("pid: %d\n", current_task->pid);
    kernel_printf("tlb_refill: entry_hi: %x  entry_lo0: %x  entry_lo1: %x\n", entry_hi, entry_lo0, entry_lo1);
#endif    
     asm volatile (
        "move $t0, %0\n\t"
        "move $t1, %1\n\t"
        "move $t2, %2\n\t"
        "mtc0 $t0, $10\n\t"
        "mtc0 $zero, $5\n\t"
        "mtc0 $t1, $2\n\t"
        "mtc0 $t2, $3\n\t"
        "nop\n\t"
        "nop\n\t"
        "tlbwr\n\t"
        "nop\n\t"
        "nop\n\t"
        :
        : "r"(entry_hi),
          "r"(entry_lo0),
          "r"(entry_lo1)
    );

#ifdef TLB_DEBUG
    kernel_printf("after tlb_refill.\n");
#endif
    //page dictionary and page table
   refill_after(pgd);
    return;

}

void refill_after(pgd_t* pgd)
{
    unsigned int* current_pgd = current_task->mm->pgd;
    unsigned int  current_pde, current_pte;
    unsigned int* current_pde_ptr;
    int i;
    int j;
    count2++;
    for(i=0;i<1024;i++)
    {
        current_pde = current_pgd[i];
        current_pde = current_pde & PAGE_MASK;
        if(!current_pde)// two level page not exist
            continue;
        kernel_printf("current_pde: %x\n", current_pde);
        current_pde_ptr = (unsigned int*)current_pde;
        for(j=0;j<1024;j++)
        {
            current_pte = current_pde_ptr[j];
            current_pte = current_pte & PAGE_MASK;
            if(!current_pte)
                kernel_printf("current_pte: %x\n", current_pte);

        }
    }    
}

#pragma GCC pop_options