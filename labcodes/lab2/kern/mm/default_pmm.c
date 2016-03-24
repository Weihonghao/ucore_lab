#include <pmm.h>
#include <list.h>
#include <string.h>
#include <default_pmm.h>

/* In the first fit algorithm, the allocator keeps a list of free blocks (known as the free list) and,
   on receiving a request for memory, scans along the list for the first block that is large enough to
   satisfy the request. If the chosen block is significantly larger than that requested, then it is 
   usually split, and the remainder added to the list as another free block.
   Please see Page 196~198, Section 8.2 of Yan Wei Ming's chinese book "Data Structure -- C programming language"
*/
// LAB2 EXERCISE 1: 2012011364
// you should rewrite functions: default_init,default_init_memmap,default_alloc_pages, default_free_pages.
/*
 * Details of FFMA
 * (1) Prepare: In order to implement the First-Fit Mem Alloc (FFMA), we should manage the free mem block use some list.
 *              The struct free_area_t is used for the management of free mem blocks. At first you should
 *              be familiar to the struct list in list.h. struct list is a simple doubly linked list implementation.
 *              You should know how to USE: list_init, list_add(list_add_after), list_add_before, list_del, list_next, list_prev
 *              Another tricky method is to transform a general list struct to a special struct (such as struct page):
 *              you can find some MACRO: le2page (in memlayout.h), (in future labs: le2vma (in vmm.h), le2proc (in proc.h),etc.)
 * (2) default_init: you can reuse the  demo default_init fun to init the free_list and set nr_free to 0.
 *              free_list is used to record the free mem blocks. nr_free is the total number for free mem blocks.
 
 * (3) default_init_memmap:  CALL GRAPH: kern_init --> pmm_init-->page_init-->init_memmap--> pmm_manager->init_memmap
 *              This fun is used to init a free block (with parameter: addr_base, page_number).
 *              First you should init each page (in memlayout.h) in this free block, include:
 *                  p->flags should be set bit PG_property (means this page is valid. In pmm_init fun (in pmm.c),
 *                  the bit PG_reserved is setted in p->flags)
 *                  if this page  is free and is not the first page of free block, p->property should be set to 0.
 *                  if this page  is free and is the first page of free block, p->property should be set to total num of block.
 *                  p->ref should be 0, because now p is free and no reference.
 *                  We can use p->page_link to link this page to free_list, (such as: list_add_before(&free_list, &(p->page_link)); )
 *              Finally, we should sum the number of free mem block: nr_free+=n
 *
 * (4) default_alloc_pages: search find a first free block (block size >=n) in free list and reszie the free block, return the addr
 *              of malloced block.
 *              (4.1) So you should search freelist like this:
 *                       list_entry_t le = &free_list;
 *                       while((le=list_next(le)) != &free_list) {
 *                       ....
 *                 (4.1.1) In while loop, get the struct page and check the p->property (record the num of free block) >=n?
 *                       struct Page *p = le2page(le, page_link);
 *                       if(p->property >= n){ ...
 *                 (4.1.2) If we find this p, then it' means we find a free block(block size >=n), and the first n pages can be malloced.
 *                     Some flag bits of this page should be setted: PG_reserved =1, PG_property =0
 *                     unlink the pages from free_list
 *                     (4.1.2.1) If (p->property >n), we should re-caluclate number of the the rest of this free block,
 *                           (such as: le2page(le,page_link))->property = p->property - n;)
 *                 (4.1.3)  re-caluclate nr_free (number of the the rest of all free block)
 *                 (4.1.4)  return p
 *               (4.2) If we can not find a free block (block size >=n), then return NULL
 
 * (5) default_free_pages: relink the pages into  free list, maybe merge small free blocks into big free blocks.
 *               (5.1) according the base addr of withdrawed blocks, search free list, find the correct position
 *                     (from low to high addr), and insert the pages. (may use list_next, le2page, list_add_before)
 *               (5.2) reset the fields of pages, such as p->ref, p->flags (PageProperty)
 *               (5.3) try to merge low addr or high addr blocks. Notice: should change some pages's p->property correctly.
 */
free_area_t free_area;

#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

static void
default_init(void) { //这里不需要更改，已经实现1 初始化free_list 2 number for free mem blocks =0
    list_init(&free_list);
    nr_free = 0;
}

static void
default_init_memmap(struct Page *base, size_t n) { //如何将一个个连续的空闲块加到空闲块列表中，siz_t n是number of block，我们这里实现和answer中不同，只是把每个空闲快的第一个page加进去，而不是所有page
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(PageReserved(p));
        ClearPageProperty(p); //中间页的property位置为0
        p->flags = 0;//p->flags should be set bit PG_property (means this page is valid)
        p->property = 0;//if this page  is free and is not the first page of free block, p->property should be set to 0.
        set_page_ref(p, 0);//p->ref should be 0, because now p is free and no reference.
    }
    base->property = n;//if this page  is free and is the first page of free block, p->property should be set to total num of block.
    SetPageProperty(base); //首页的property位置为0
    nr_free += n;//add new blocks to free mem blocks
    if (list_next(&free_list) == &free_list) {//注意free_list是一个环形链表，尾地址指向头地址，如果头地址下一个就是自己，说明这个环形链表是空的
        list_add(&free_list, &(base->page_link)); //注意，空闲块链表(free_list)只是连接空闲块的第一个page
    } else {
        list_add(list_prev(&free_list), &(base->page_link));//环形链表头地址的previous是尾地址，就是应该add的地方
    }

}

static struct Page *
default_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > nr_free) {
        return NULL;
    }

    struct Page *page = NULL;
    list_entry_t *le = &free_list;
   // le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        if (p->property >= n) {
            page = p;
            break;
        }
    }

    if (page != NULL) { //之前while循环找到第一个符合条件大小大于n的空闲快
        list_entry_t *pageprev = page->page_link.prev;

        list_del(&(page->page_link));//unlink the pages from free_list
        //该空闲块的地址确定，切出来剩下的空闲块，只需要连接到原page之前的那个空闲块后面即可
        if (page->property > n) {
            struct Page *p = page + n; //p正好为分配后剩下的
            p->property = page->property - n;
            assert(p->property > 0);
            SetPageProperty(p);
            list_add(pageprev, &(p->page_link));
        }

        nr_free -= n;//总空闲快数量减少
        ClearPageProperty(page);//PG_property =0
        SetPageReserved(page);//PG_reserved =1
    }

    return page;
}

static void
default_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;

   /*for (; p != base + n; p ++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        set_page_ref(p, 0);
    }*/

    base->property = n;
    base->flags = 0;
    set_page_ref(base, 0);
    SetPageProperty(base);
    assert(PageProperty(base));

    list_entry_t *le = list_next(&free_list);

    while (le != &free_list) { //查找空闲列表
        p = le2page(le, page_link);
        le = list_next(le);
        if (base + base->property == p) { //base右边有空闲块
                base->property += p->property;
                ClearPageProperty(p);
                p->ref = 0;
                p->property = 0;
                list_del(&(p->page_link));
                //cprintf(" right merge! \n");
        } else if (p + p->property == base) { //base左边有空闲块
                p->property += base->property;
                ClearPageProperty(base);
                //ClearPageProperty(p);
                //SetPageProperty(p);
                base->ref = 0;
                base->property = 0;
                list_del(&(p->page_link));
                base = p;
                //cprintf(" left merge! \n");
        }
            //break;
    } //end while
    nr_free += n;


    if (list_empty(&free_list)) {
        list_add(&free_list, &(base->page_link));
        //cprintf(" base insert after head!! \n");
    } else {

        le = list_next(&free_list);
        while (le != &free_list) {
            p = le2page(le, page_link);
            struct Page* nextpage = le2page(list_next(le), page_link);

            if (base<p ){
                //cprintf(" base insert!! \n");
                list_add_before(le, &(base->page_link));
                break;
            } else if (list_next(le) == &free_list) {//如果到最后一个，base还在p的右边，那么就插到末尾
                //cprintf(" base insert!! \n");
                list_add(le, &(base->page_link));
                break;
            }
            le = list_next(le);

        }

    }

    assert(PageProperty(base));

}

static size_t
default_nr_free_pages(void) {
    return nr_free;
}

static void
basic_check(void) {
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;
    assert((p0 = alloc_page()) != NULL);
    //cprintf(" p0 addr:%08llx! \n",p0);
    //cprintf(" p0+1 addr:%08llx! \n",p0+1);
    assert((p1 = alloc_page()) != NULL);
    //cprintf(" p1 addr:%08llx! \n",p1);
    //cprintf(" p1+1 addr:%08llx! \n",p1+1);
    assert((p2 = alloc_page()) != NULL);
    //cprintf(" p2 addr:%08llx! \n",p2);

    //cprintf(" basic check 1 ok! \n");

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);

    assert(page2pa(p0) < npage * PGSIZE);
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);

    //cprintf(" basic check 2 ok! \n");

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    assert(alloc_page() == NULL);

    //cprintf(" basic check 3 ok! \n");

    free_page(p0);
    //cprintf(" p0 freed! \n");
    free_page(p1);
    //cprintf(" p1 freed! \n");
    //cprintf(" basic check 4 ok! \n");
    free_page(p2);
    //cprintf(" p2 freed! \n");
    //cprintf(" basic check 5 ok! \n");
    assert(nr_free == 3);

    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(alloc_page() == NULL);

    free_page(p0);
    assert(!list_empty(&free_list));


    struct Page *p;
    assert((p = alloc_page()) == p0);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    free_list = free_list_store;
    nr_free = nr_free_store;

    free_page(p);
    free_page(p1);
    free_page(p2);
}

// LAB2: below code is used to check the first fit allocation algorithm (your EXERCISE 1) 
// NOTICE: You SHOULD NOT CHANGE basic_check, default_check functions!
static void
default_check(void) {
    int count = 0, total = 0;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p));
        count ++, total += p->property;
    }
    assert(total == nr_free_pages());
    //cprintf(" enter basic check! \n");
    basic_check();
    //cprintf(" basic check ok!---------------------------------------------------- \n");

    struct Page *p0 = alloc_pages(5), *p1, *p2;
    assert(p0 != NULL);
    assert(!PageProperty(p0));

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));
    assert(alloc_page() == NULL);

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    free_pages(p0 + 2, 3);
    assert(alloc_pages(4) == NULL);
    assert(PageProperty(p0 + 2) && p0[2].property == 3);
    assert((p1 = alloc_pages(3)) != NULL);
    assert(alloc_page() == NULL);
    assert(p0 + 2 == p1);

    p2 = p0 + 1;
    //cprintf(" ready0! \n");
    free_page(p0);
    assert(PageProperty(p0) );
    //cprintf(" ready1! \n");
    free_pages(p1, 3);
    assert(PageProperty(p0) && p0->property == 1);
    //assert(p0->property == 1);
    //assert(PageProperty(p0) );
    assert(PageProperty(p1) && p1->property == 3);
    //cprintf(" ok1! \n");
    assert((p0 = alloc_page()) == p2 - 1);
    free_page(p0);
    assert((p0 = alloc_pages(2)) == p2 + 1);

    free_pages(p0, 2);
    free_page(p2);

    assert((p0 = alloc_pages(5)) != NULL);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    nr_free = nr_free_store;

    free_list = free_list_store;
    free_pages(p0, 5);

    le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        count --, total -= p->property;
    }
    assert(count == 0);
    assert(total == 0);
}

const struct pmm_manager default_pmm_manager = {
    .name = "default_pmm_manager",
    .init = default_init,
    .init_memmap = default_init_memmap,
    .alloc_pages = default_alloc_pages,
    .free_pages = default_free_pages,
    .nr_free_pages = default_nr_free_pages,
    .check = default_check,
};