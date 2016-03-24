#第一题
这一题要求完成frist-fit
修改以下几个函数（按照注释）：
default_init:
不用改，已经完成 initiallize free_list，并把free block的个数设置为0

##default_init_memmap
需要修改
这里主要是如何将一个空闲的块加到空闲列表中
注意我们实现的方式和answer里面不同，我们并不是把所有的page都加进去，而只是假如每个block中第一个page
这样的话，需要把原来每个page clear掉，不表示是某一个block的第一个page，然后将flag和大小（置零）
注意如果是某个块的第一个page，它的property要设置为块大小
然后通过检查环向链表是否首尾相接，根据链表是否为空，判断这个块（其实是这个块的第一个page）加到free_list的哪个位置。


##default_alloc_pages
需要修改
这里主要是如何把一个free block按照firstin算法分配出去，并将block剩余的部分重新放回free_list中
首先，如果需求内存大小大于现有的空闲块大小之和，那么无法完成
找到第一个大于所需大小的块，然后将这个块的第一个page从free_list里面删去，并将这个块剩余的部分，改变大小，设定新的块的新第一个page，并将这个page加入到free_list中。

##default_free_pages
释放非空块，并且将可能合并的free block合并。
可以先合并再插入，也可以先插入在合并。我们选择的是先合并，在插入，所以发现和释放块临近的那个空闲快，需要先从free_list中被删掉
首先通过while循环找到相邻的空闲块。如果在释放块的右边，那么将释放块的大小增加，空闲快的第一个page不再是一个块的第一个page，修改大小和reference后从free_list中删掉
如果实在释放块的左边，那么就把释放块的第一个page不再是一个块的page，修改大小和ref，删掉空闲块
然后我们将新的合并后的空闲块插入
如果free_list为空，那么就插在末尾，不然通过while寻找，知道找到第一个在目标空闲块右边的空闲块，插入，或者找不到这样的块就在末尾插入。

和answer不同，我们用每个块的第一个page表示这个块，而不是这个块中的所有page，因此无论是分配还是释放都不用遍历块的内部，比较好。

##改进之处：
应该是实现了answer中的改进，剩下的可以通过数据结构实现，比如现在查找都是用O(N)，可以用线段树把复杂度降到O(logN)

#第二题
这道题一开始在一级页表二级页表上面绕了很久，其实想通了就是一个球pde和pte的问题，并没有涉及到后面offset的部分。
所以为了方便考虑，可以直接考虑成一个只有一级的系统，因为二级页表也是一个页。

思路是先把虚拟地址转换成pde，然后在创建相应的页面，根据注释，分别处理需不需要create和申请失败的情况。

这里有个疑问是memset为什么是虚地址，不太清楚，希望助教能说明一下。

然后就是PTX取虚地址的中间10位，PDE通过PDE_ADDR去掉标识位，只保留表示二次页表位置的部分，也就是高20位，然后转成虚地址，和之前PTX结果相加即可。


##页目录项和页表中每个组成部分的含义和以及对ucore而言的潜在用处
页目录表项中包含页表的基址和若干标志位，页表项中包含页基址和若干标志位。

其中标志位可以通过mmu.h来查看
```
#define PTE_P           0x001                   // Present
#define PTE_W           0x002                   // Writeable
#define PTE_U           0x004                   // User
#define PTE_PWT         0x008                   // Write-Through
#define PTE_PCD         0x010                   // Cache-Disable
#define PTE_A           0x020                   // Accessed
#define PTE_D           0x040                   // Dirty
#define PTE_PS          0x080                   // Page Size
#define PTE_MBZ         0x180                   // Bits must be zero
#define PTE_AVAIL       0xE00                   // Available for software use
```
具体含义为
PTE_P:页表或者页是否存在的标志位。

PTE_W:页表是否可写的权限控制位。

PTE_U:判断该页是否是用户态，是否能被用户操作。

PTE_PWT:写穿透标志。该标志位在启用Cache有用，若该位为1，则每次写操作时，需要同时写Cache和主存。

PTE_PCD:是否启用Cache的标志。

PTE_A:访问标志,表示该页是刚加入内存尚未被访问，还是已经被访问过了。

PTE_D:表示该页是否是Dirty的。此标志位在虚拟内存中有用。当发生缺页时，需要从硬盘中把一个页调入内存，若此时内存没有空闲的页，则需要根据相应的替换算法替换一个页出去。Dirty表示该页是否被写过，若被写过，则替换时需要将其内容更新到硬盘（交换分区）中；若未被写过，则直接丢弃即可。

PTE_PS: 页的大小，可以根据offset来加以检查，看页内访问是否越界。

##如果ucore执行过程中访问内存，出现了页访问异常，请问硬件要做哪些事情	

与访问权限相关的异常	中断服务例程，报错
使用虚拟存储时，发生页缺失异常	找到相应的页加入到内存中
在页缺失中，如果没有空闲页，那么需要用替换算法，把不用的页替换出来
如果页已经被修改，需要写回交换分区或虚拟内存文件。

#第三题
这道题按照注释进行
首先判断这个page是否存在
如果存在的话，找到这个表，并且将page的reference减少，如果减少后page的reference已经为0，那么就需要free page
然后将page的地址在二级列表中清除，这里直接置零
然后在tlb中将对应的快表信息无效掉。

##数据结构Page的每一项与页表中的页目录项和页表项的对应关系
有关系
从pmm.c的代码中可以看出page是从end开始的一段连续空间。
```
page = maxpa / PGSIZE; 
pages = (struct Page *)ROUNDUP((void *)end, PGSIZE); 
for (i = 0; i < npage; i ++) {
    SetPageReserved(pages + i); 
}
uintptr_t freemem = PADDR((uintptr_t)pages + sizeof(struct Page) * npage);
```
而页表项和页目录项和本身的地址有关
从pmm.h的pa2page函数可以看出page也是和地址有关，并且是rank的关系，因此推测page和页表项与页目录项有关。
```
static inline struct Page *
pa2page(uintptr_t pa) {
if (PPN(pa) >= npage) {
    panic("pa2page called with invalid pa");
}
return &pages[PPN(pa)];
```


##修改lab2使得虚拟地址与物理地址相等

lab2中，因为段机制采用的是对等映射，也就是说虚地址等于线性地址，如果要等于物理地址，那么必须起点一样。
因此,让gcc编译出的虚拟起始地址从0x100000开始即可。
```
virtual_addr = linear_addr = physical_addr + 0xC0000000
```
