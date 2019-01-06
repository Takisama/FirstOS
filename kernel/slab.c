#include <arch.h>
#include <driver/vga.h>
#include <zjunix/slab.h>
#include <zjunix/utils.h>

#define KMEM_ADDR(PAGE, BASE) ((((PAGE) - (BASE)) << PAGE_SHIFT) | 0x80000000)

/*
 * one list of PAGE_SHIFT(now it's 12) possbile memory size
 * 96, 192, 8, 16, 32, 64, 128, 256, 512, 1024, (2 undefined)
 * in current stage, set (2 undefined) to be (4, 2048)
 */
struct kmem_cache kmalloc_caches[PAGE_SHIFT];

static unsigned int size_kmem_cache[PAGE_SHIFT] = {96, 192, 8, 16, 32, 64, 128, 256, 512, 1024, 1536, 2048};

// init the struct kmem_cache_cpu
void init_kmem_cpu(struct kmem_cache_cpu *kcpu) {
    kcpu->page = 0;
}

// init the struct kmem_cache_node
void init_kmem_node(struct kmem_cache_node *knode) {
    INIT_LIST_HEAD(&(knode->full));
    INIT_LIST_HEAD(&(knode->partial));
}

void init_each_slab(struct kmem_cache *cache, unsigned int size) {
    cache->objsize = size;
    cache->objsize  = Allign(size, SIZE_INT);
    cache->size = cache->objsize + sizeof(void *);  // add one char as mark(available), to link the free obj
    init_kmem_cpu(&(cache->cpu));
    init_kmem_node(&(cache->node));
}

void init_slab() {
    unsigned int i;

    for (i = 0; i < PAGE_SHIFT; i++) {
        init_each_slab(&(kmalloc_caches[i]), size_kmem_cache[i]);
    }
#ifdef SLAB_DEBUG
    kernel_printf("Setup Slub ok :\n");
    kernel_printf("\tcurrent slab cache size list:\n\t");
    for (i = 0; i < PAGE_SHIFT; i++) {
        kernel_printf("%x %x ", kmalloc_caches[i].objsize, (unsigned int)(&(kmalloc_caches[i])));
    }
    kernel_printf("\n");
#endif  // ! SLAB_DEBUG
}

// ATTENTION: sl_objs is the reuse of bplevel
// ATTENTION: slabp must be set right to add itself to reach the end of the page
// 		e.g. if size = 96 then 4096 / 96 = .. .. 64 then slabp starts at
// 64
void format_slabpage(struct kmem_cache *cache, struct page *page) {
    unsigned char *moffset = (unsigned char *)KMEM_ADDR(page, pages);  // physical addr
    (*page).flag = 1<<29;//page_slab
    struct slab_head *s_head = (struct slab_head *)moffset;
    void *ptr;
    *ptr = moffset  + sizeof(struct slab_head);
 
    s_head->end_ptr = ptr;
    s_head->nr_objs = 0;
    s_head->isFull = 0;

    cache->cpu.page = page;
    page->virtual = (void *)cache;
    page->slabp = 0;// slabp represents the base-addr of free space
}

void *slab_alloc(struct kmem_cache *cache) {
    struct slab_head *s_head;
    void *object = 0;
    struct page *newpage;

    if (cache->cpu.page!=0)
        goto fullPage;
    newpage = cache->cpu.page;
    s_head = (struct slab_head*)KMEM_ADDR(newpage, pages);
#ifdef SLAB_DEBUG
    kernel_printf("is_full?%d\n", s_head->isFull);
#endif

FreeList:
    if (newpage->slabp != 0) {     // Allocate from free list
        object = (void*)cur_page->slabp;
        newpage->slabp = *(unsigned int*)newpage->slabp;
        ++(s_head->nr_objs);
#ifdef SLAB_DEBUG
        kernel_printf("From Free-list\nnr_objs:%d\tobject:%x\tnew slabp:%x\n", s_head->nr_objs, object, newpage7s->slabp);
        // kernel_getchar();
#endif  // ! SLAB_DEBUG
        return object;
    }

fullPage:
    kernel_printf("Page is Full\n");
    if(list_empty(&(cache->node.partial))){
    // call the buddy system to allocate one more page to be slab-cache
    newpage = __alloc_pages(0);// get bplevel = 0 page === one page
    if(!newpage)
    {    // allocate failed, memory in system is used up
        kernel_printf("ERROR: slab request one page in cache failed\n");
        while(1);
    }    
#ifdef SLAB_DEBUG
        kernel_printf("\tnew page, index: %x \n", newpage - pages);
#endif  // ! SLAB_DEBUG

    format_slabpage(cache, newpage); // using standard format to shape the new-allocated page,
    s_head = (struct slab_head*)KMEM_ADDR(cache->cpu.page, pages);//set the new page be cpu.page
    goto notFullPage;   
}

notFullPage:
    if(s_head->isFull!=0)
    {
        object = s_head->endptr;
        s_head->end_ptr = object + cache->size;
        ++(s_head->nr_objs);

        if(s_head->end_ptr+cache->size - (void*)s_head>= 1<<PAGE_SHIFT)
        {
            s_head->isFull = 1;
            list_add_tail(&(cache->cpu.page->list), &(cache->node.full));
            init_kmem_cpu(&(cache->cpu));
            //after alloc it may full
#ifdef SLAB_DEBUG
            kernel_printf("Page become full\n");
#endif
        }
#ifdef SLAB_DEBUG
        kernel_printf("Page not full\n");
#endif
        return object;
    }

#ifdef SLAB_DEBUG
        kernel_printf("Get partial page\n");
#endif
        cache->cpu.page = container_of(cache->node.partial.next, struct page, list);
        newpage = cache->cpu.page;;
        list_del(cache->node.partial.next);
        s_head = (struct slab_head*)KMEM_ADDR(newpage, pages);
        goto FreeList;


}

void slab_free(struct kmem_cache *cache, void *object) {
    struct page *opage = pages + ((unsigned int)object >> PAGE_SHIFT);
    unsigned int *ptr;
    struct slab_head *s_head = (struct slab_head *)KMEM_ADDR(opage, pages);

    if (!(s_head->nr_objs)) {
        kernel_printf("ERROR : slab_free error!\n");
        // die();
        while (1)
            ;
    }
    object = (void*)(unsigned int object)|KERNEL_ENTRY;
#ifdef SLAB_DEBUG
    kernel_printf("page address:%x\n object:%\n slabp:%x\n", opage, object, opage->slabp);//slabp represents the base-addr of free space
#endif
   
    ptr = (unsigned int *)((unsigned char *)object + cache->offset);
    *ptr = *((unsigned int *)(s_head->end_ptr));
    *((unsigned int *)(s_head->end_ptr)) = (unsigned int)object;
    --(s_head->nr_objs);
#ifdef SLAB_DEBUG
    kernel_printf("slabp: %x\n", opage->slabp);
#endif

    if (list_empty(&(opage->list))) //it is cpu
        return;
#ifdef SLAB_DEBUG
    kernel_printf("not cpu\n");
#endif

    if (!(s_head->nr_objs)) {
        list_del_init(&(opage->list));
        __free_pages(opage, 0);
        return;
    }
#ifdef SLAB_DEBUG
    kernel_printf("has freed\n");
#endif
    if((!opage->slabp)&&s_head->isFull){
    list_del_init(&(opage->list));
    list_add_tail(&(opage->list), &(cache->node.partial));
    }
}

// find the best-fit slab system for (size)
unsigned int get_slab(unsigned int size) {
    unsigned int itop = PAGE_SHIFT;
    unsigned int i;
    unsigned int bf_num = (1 << (PAGE_SHIFT - 1));  // half page
    unsigned int bf_index = PAGE_SHIFT;             // record the best fit num & index

    for (i = 0; i < itop; i++) {
        if ((kmalloc_caches[i].objsize >= size) && (kmalloc_caches[i].objsize < bf_num)) {
            bf_num = kmalloc_caches[i].objsize;
            bf_index = i;
        }
    }
    return bf_index;
}

void *kmalloc(unsigned int size) {
    void *result;

    if (!size)
        return 0;
    
    result = phy_kmalloc(size);

    if (result) return (void*)(KERNEL_ENTRY | (unsigned int)result);
    else return 0;
}

void *phy_kmalloc(unsigned int size) {
    struct kmem_cache *cache;
    unsigned int bf_index;

    if (!size)
        return 0;

    // if the size larger than the max size of slab system, then call buddy to
    // solve this
    if (size > kmalloc_caches[PAGE_SHIFT - 1].objsize) {
        size = UPPER_ALLIGN(size, 1<<PAGE_SHIFT);
        return alloc_pages(size >> PAGE_SHIFT);
    }

    bf_index = get_slab(size);
    if (bf_index >= PAGE_SHIFT) {
        kernel_printf("ERROR: No available slab\n");
        while (1)
            ;
    }
    return slab_alloc(&(kmalloc_caches[bf_index]));
}


void kfree(void *obj) {
    struct page *page;

    obj = (void *)((unsigned int)obj & (~KERNEL_ENTRY));
    page = pages + ((unsigned int)obj >> PAGE_SHIFT);
    if (!(page->flag == _PAGE_SLAB))
        return free_pages((void *)((unsigned int)obj & ~((1 << PAGE_SHIFT) - 1)), page->bplevel);

    return slab_free(page->virtual, obj);
}
