注意
trap.c文件在复制黏贴的时候
有个declare是在上面
注意要复制过来，不然 make fail
struct trapframe switchk2u, *switchu2k;

#问题一
##1 实现过程
练习一的函数do_pgfault的作用是处理缺页中断，其中传入的参数包括管理结构mm，错误代码error_code和产生缺页中断的线性地址addr。
Ucore的do——pgfault根据注释的提示，其实只处理三种情况，分别是
1处理写一个存在的地址
2写一个不存在但是地址可写
3读一个不存在但是地址可读

其余情况都直接保存。
对于缺页情况，首先需要从硬盘的swap文件读取一个页到内存，因此要完成线性地址到swap的对应起始山区的转换。
如果页不在内存中，这个时候页表项完成的仅仅是储存扇区编号的功能，所以需要通过get_pte函数获取页表项，以便完成之后对扇区的处理。
如果页表项全部为0时，该页表尚未映射对应的物理空间，通过pgdir_alloc_page函数完成分配。这个函数在分配的时候，还完成通过调用page_insert函数建立地址映射，加入FIFO链表等工作。

##2 描述页目录表和页表组成部分对UCore实现页替换算法的潜在用处

这答题貌似在lab2有类似的问题。
页目录表项中包含页表的基址和一些标志位
页表项中包含页的基址和一些标志位
其中标志位在mmu.h中有比较明确的定义，

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

但是对页替换项算法有潜在作用的，应该只有PTE_P，PTE_A和PTE_D

PTE_P:页表或者页是否存在的标志位。如果该位为0，说明对应的页不在内存中而是在Swap分区中，需要从硬盘中读取内容。

PTE_A:访问标志,表示该页是刚加入内存尚未被访问，还是已经被访问过了。该位可以用于Clock算法中，记录页是否被访问，从而按一定规则选该位为0的页进行替换。

PTE_D:表示该页是否是Dirty的。当发生缺页时，需要从硬盘中把一个页调入内存，若此时内存没有空闲的页，则需要根据相应的替换算法替换一个页出去。Dirty表示该页是否被写过，若被写过，则替换时需要将其内容更新到硬盘（交换分区）中；若未被写过，则直接丢弃即可。



##3. 如果UCore缺页服务例程在执行过程中访存出现页访问异常，硬件要做哪些事情
简要来说，硬件需要做以下几件事：
1 保存缺页服务例程的现场
2 产生缺页中断
3 查找相应的IDT和GDT
4 返回缺页服务例程处理缺页异常
其中需要注意的细节是：
取硬盘上的虚拟内存文件或交换分区中寻找相应的页加入到内存中时，
	若内存无空闲页
		需要按照一定的替换算法替换出相应的页，
	若该页被修改过
		需要将该页写回交换分区或虚拟内存文件中
然后还要用iret返回，



#练习二
##1 实现过程
_fifo_map_swappable函数
只需要添加list_add(head,entry);即可。
这里需要注意的是，因为是一个双向链表，而这个双向链表在init的时候是空的，所以理论上来说用list_add
和list_add_before应该都是可以的，但是后面的实现要一致。
以我的实现为例，现在最新的页都是放在head后面，这样head前面就是最早的页，这样就可以和之前lab的实验相对应了。

_fifo_swap_out_victim函数
我们需要做两件事，第一件事是从pra_list当中把最先进来的page删掉，这里因为我们是把最新的放在head后面，所以就获得head前的地址删掉。
第二件事是获取这个被删掉的页是什么。
*ptr_page = page;
注意不能够用
ptr_page = &page;
因为这个page本身可能被删掉（因为是被置换出去的），这个时候调用ptr_page的时候就会出现问题。

##UCore中实现extended clock页替换算法的设计方案
###需要被置换出的页的特征是什么
access位应该是0，且dirty位也应该是0，说明这个页已经被置换到access==0没有被访问过，dirty==0和外存内容没有区别

###ucore中如何判断具有这样特征的页
通过标志位进行判断
access位对应的是PTE_A
dirty位对应的是PTE_D

###何时进行换入和换出操作
首先和fifo一样，从head前面开始，如果发现标志位是11（access|dirty），则变为01，如果是10，变成00，如果是01，则不变。然后接着向前寻找，直到找到第一个为00的，替换出去

#和result的区别
本次实验比较简单，代码量比较少，所以区别不是很多。如果可以的话，可以把环形链表head前后的位置互换。

#相关知识点 
主要是虚拟内存管理相关的知识，包括页缺失的处理和FIFO clock extended clock等页置换算法，以及相应标志位的应用。