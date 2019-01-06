#include <arch.h>
#include <driver/vga.h>
#include <zjunix/bootmm.h>
#include <zjunix/utils.h>

struct bootmm bmm;
unsigned int firstusercode_start;
unsigned int firstusercode_len;

// const value for ENUM of mem_type
char *mem_msg[] = {"Kernel code/data", "Mm Bitmap", "Vga Buffer", "Kernel page directory", "Kernel page table", "Dynamic", "Reserved"};

// set the content of struct bootmm_info
void set_mminfo(struct bootmm_info *info, unsigned int start, unsigned int end, unsigned int type) {
    info->start = start;
    info->end = end;
    info->type = type;
}
unsigned char bootmmmap[MACHINE_MMSIZE >> PAGE_SHIFT];
/*
* return value list:
*		0 -> insert_mminfo failed
*		1 -> insert non-related mm_segment
*		2 -> insert forward-connecting segment
*		4 -> insert following-connecting segment
*		6 -> insert forward-connecting segment to after-last position
*		7 -> insert bridge-connecting segment(remove_mminfo is called
for deleting
--
*/
unsigned int insert_mminfo(struct bootmm *mm, unsigned int start, unsigned int end, unsigned int type) {
    unsigned int i;
    struct bootmm_info *infoArray = mm->info;
    for (i = 0; i < mm->cnt_infos; i++) {
        if(infoArray[i].start>start)
            break;
        if (infoArray[i].type != type)
            continue;  // ignore the type-mismatching items to find one likely
                       // mergable
        if (infoArray[i].end == start - 1) {// new-in mm is connecting to the forwarding one
            if ((i + 1) < mm->cnt_infos) {// current info is still not the last segment
                   if (infoArray[i+1].type==type && infoArray[i+1].start==end+1) {
                    // Two-way merging
                    infoArray[i].end = infoArray[i + 1].end;
                    remove_mminfo(mm, i + 1);
                    return TwowayMerge;
                    }
                }
            // Forward Merging
            infoArray[i].end = end;
            return ForwardMerge;
            }
        }
        if (i >= MAX_INFO) {
        return Failed;
    }
    if (i < mm->cnt_infos) {
        if (infoArray[i].type==type && infoArray[i].start==end+1) {

            infoArray[i].start = start;
            return BackwardMerge;            // Backward Merging
        }
    }
    set_mminfo(infoArray+mm->cnt_infos, start, end, type);
    mm->cnt_infos=mm->cnt_infos+1;
    return NoMerge;
}

/* get one sequential memory area to be split into two parts
 * (set the former one.end = split_start-1)
 * (set the latter one.start = split_start)
 */
unsigned int split_mminfo(struct bootmm *mm, unsigned int index, unsigned int split_start) {
    unsigned int start, end;
    unsigned int tmp;

    start = mm->info[index].start;
    end = mm->info[index].end;
    split_start &= PAGE_ALIGN;

    if ((split_start <= start) || (split_start >= end))
        return 0;  // split_start out of range

    if (mm->cnt_infos == MAX_INFO)
        return 0;  // number of segments are reaching max, cannot alloc anymore
                   // segments
    if(index>=mm->cnt_infos)
            return 0;//invaild index
    // using copy and move, to get a mirror segment of mm->info[index]
    for (tmp = mm->cnt_infos - 1; tmp >= index; --tmp) {
        mm->info[tmp + 1] = mm->info[tmp];
    }
    mm->info[index].end = split_start - 1;
    mm->info[index + 1].start = split_start;
    mm->cnt_infos++;
    return 1;
}

// remove the mm->info[index]
void remove_mminfo(struct bootmm *mm, unsigned int index) {
    unsigned int i;
    if (index >= mm->cnt_infos)
        return;

    if (index + 1 < mm->cnt_infos) {
        for (i = (index + 1); i < mm->cnt_infos; i++) {
            mm->info[i - 1] = mm->info[i];
        }
    }
    mm->cnt_infos--;
}

void init_bootmm() {
    unsigned int index;
    unsigned char *t_map;
    unsigned int end;
    end = 16 * 1024 * 1024;
    kernel_memset(&bmm, 0, sizeof(bmm));
    bmm.phymm = get_phymm_size();
    bmm.max_pfn = bmm.phymm >> PAGE_SHIFT;
    bmm.s_map = bootmmmap;
    bmm.e_map = bootmmmap + sizeof(bootmmmap);
    bmm.cnt_infos = 0;
    kernel_memset(bmm.s_map, PAGE_FREE, sizeof(bootmmmap));
    insert_mminfo(&bmm, 0, (unsigned int)(end - 1), _MM_KERNEL);
    bmm.last_alloc_end = (((unsigned int)(end) >> PAGE_SHIFT) - 1);

    for (index = 0; index<end>> PAGE_SHIFT; index++) {
        bmm.s_map[index] = PAGE_USED;
    }
}

/*
 * set value of page-bitmap-indicator
 * @param s_pfn	: page frame start node
 * @param cnt	: the number of pages to be set
 * @param value	: the value to be set
 */
void set_maps(unsigned int s_pfn, unsigned int cnt, unsigned char value) {
    while (cnt) {
        bmm.s_map[s_pfn] = (unsigned char)value;
        --cnt;
        ++s_pfn;
    }
}

/*
 * This function is to find sequential page_cnt number of pages to allocate
 * @param page_cnt : the number of pages requested
 * @param s_pfn    : the allocating begin page frame node
 * @param e_pfn	   : the allocating end page frame node
 * return value  = 0 :: allocate failed, else return index(page start)
 */
unsigned char *find_pages(unsigned int page_cnt, unsigned int s_pfn, unsigned int e_pfn, unsigned int align_pfn) {
    unsigned int index, tmp;
    unsigned int cnt;

    if(align_pfn==0)
        align_pfn=1;
    s_pfn = Allign(s_pfn , align_pfn);

    for (index = s_pfn; index < e_pfn;) {
        if (bmm.s_map[index] == PAGE_USED) {
            ++index;
            continue;
        }
        tmp = index;
        cnt = page_cnt;
       
        while (cnt) {
            if (tmp >= e_pfn)
                return 0;
            // reaching end, but allocate request still cannot be satisfied

            if (bmm.s_map[index] == PAGE_FREE) {
                index = tmp+1;//space begin after temp
                break;
            }

            if (bmm.s_map[tmp] == PAGE_USED) {
                break;
            }
            tmp++;
            cnt--;
        }
        if (cnt == 0) {  // cnt = 0 indicates that the specified page-sequence found
            bmm.last_alloc_end = tmp - 1;
            set_maps(index, page_cnt, PAGE_USED);
            return (unsigned char *)(index << PAGE_SHIFT);
        } else {
            index = tmp + align_pfn;  // there will be no possible memory space
                                      // to be allocated before tmp
        }
    }
    if(index>=e_pfn)
        return 0;
}

unsigned char *bootmm_alloc_pages(unsigned int size, unsigned int type, unsigned int align) {
    unsigned int size_inpages;
    unsigned char *res;

    size = Allign (size, 1 << PAGE_SHIFT);
    size_inpages = size >> PAGE_SHIFT;

    // in normal case, going forward is most likely to find suitable area
    res = find_pages(size_inpages, bmm.last_alloc_end + 1, bmm.max_pfn, align >> PAGE_SHIFT);
    if (res) {
        insert_mminfo(&bmm, (unsigned int)res, (unsigned int)res + size - 1, type);
        return res;
    }

    // when system request a lot of operations in booting, then some free area
    // will appear in the front part
    res = find_pages(size_inpages, 0, bmm.last_alloc_end, align >> PAGE_SHIFT);
    if (res) {
        insert_mminfo(&bmm, (unsigned int)res, (unsigned int)res + size - 1, type);
        return res;
    }
    return 0;  // not found, return NULL
}

void bootmap_info(unsigned char *msg) {
    unsigned int index;
    kernel_printf("%s :\n", msg);
    for (index = 0; index < bmm.cnt_infos; ++index) {
        kernel_printf("\t%x-%x : %s\n", bmm.info[index].start, bmm.info[index].end, mem_msg[bmm.info[index].type]);
    }
}

void bootmm_free_pages(unsigned int start, unsigned int size)
{
    unsigned int index, size_inpages;
    unsigned bootmm_info target;
    unsigned int end = start + size -1;
    size = size&PAGE_ALIGN；
    size_inpages = size>>PAGE_SHIFT;

    start = start&PAGE_ALIGN；

    target = bmm.info[index];
    for(index = 0;index<bmm.cnt_infos;index++)
    {
        if(bmm.info[index].start<=start && bmm.info[index].end>=end)
            break;
    }

    if(index==bmm.cnt_infos)
    {
        kernel_printf("bootmm_free_pages: No allocated space %x-%x\n", start, end);
        return;
    }

    if(target.start == start)
    {
        if(target.end==end)
            remove_mminfo(&bmm, index);
        else//the fromt 
            set_mminfo(bmm.info+index, start+index, target.end, target.type);
    }
    else if(target.end == end)//the rear
        set_mminfo(bmm.info+index, target.start, start-1, target.type);
    else
    {
        if(split_mminfo(&bmm, index, start)==0)
        {
            kernel_printf("bootmm_free_pages:split failed\n");
            return;
        }
        set_mminfo(bmm.info+index+1, end+1, target.end, target.type);
    }
    set_maps(start<<PAGE_SHIFT, size_inpages, PAGE_FREE);
}