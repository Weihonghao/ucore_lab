#实验四
##练习一
###1. 设计实现过程

alloc_proc函数中对新申请的proc_struct进行初始化。初始化的代码如下:
```
proc->state = PROC_UNINIT;//表示尚未完成创建
proc->pid = -1; //do_fork函数要用
proc->runs = 0;
proc->kstack = 0;
proc->need_resched = 0;
proc->parent = NULL;
proc->mm = NULL;
proc->tf = NULL;
proc->cr3 = boot_cr3; //内核线程,直接使用内核堆栈
proc->flags = 0;
memset(proc->name,0,sizeof(PROC_NAME_LEN + 1));
memset(&(proc->context),0,sizeof(struct context));
```
大部分的成员变量都设为0或NULL。
需要注意的有一下几个成员变量：
1 state设置为PROC_UNINIT,也就是五状态模型里面的创建状态，还没有创建完毕，该线程正在创建,还在进行资源分配,数据结构初始化等工作
2 pid=-1,同样也是表示处于创建状态,尚未完成创建。这个成员变量之所以要设置，是因为pid会在do_fork函数中通过get_pid函数获得。-1表示还没有创建。
3 cr3设置为boot_cr3。因为内核线程全部共用的是UCore的内核空间,所以直接把一级页表的基址设为UCore的页目录表基址即可。要注意的是，这里是内核线程，如果是用户进程来说,成员mm负责管理用户空间,其页目录表的基页也存于mm结构体中的pgdir里。

###2. 请说明proc_struct中struct context context和struct trapframe *tf成员变量含义和在本实验中的作用
context是该线程的上下文内容。
proc.h里面对于上下文内容有这样的定义：
```
struct context {
    uint32_t eip;
    uint32_t esp;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
};
```
和上课了解的内容相同，上下文主要是保存在切换进程时候的各种寄存器的值，保存后方便恢复。

我们可以通过往年试题卷上的题目看出这些上下文在保存什么。
在proc_run函数里
```
current = proc;
load_esp0(next->kstack + KSTACKSIZE);
lcr3(next->cr3); //进程地址空间的切换
switch_to(&(prev->context), &(next->context));
```
在加载新进程的栈段和cr3后，要注意switch_to这个函数，具体在switch.S中。
首先需要注意的是，和之前函数压栈不同，switch.s里面并没有把esp复制给ebp，所以现在(esp)就是返回地址，而不是4(esp)。
现在4(esp)是函数第一个参数的地址，看switch_to函数，知道，第一个参数，4(esp)是prev->context的基址,8(esp)是next->prev的基址.
然后,通过popl 0(%eax) ,将(esp)中的内容,即返回地址存于0(eax),即prev->context的第一个变量,可以看到该变量为eip.随后将各种寄存器的值都保存在pre->context的相应变量中.然后用语句 movl 4(%esp), %eax 将next->context的基址加载到eax寄存器中.注意,因为前面已经将prev->context基址pop出去了,所以此时4(%esp)即是next->context的基址.将next->context中保存的各个寄存器的值恢复到相应寄存器中,然后用pushl 0(%eax) 将next->context中保存的eip值压栈,则此时esp,即返回地址是next->context中的eip.

trapframe是该线程的中断帧，上一个进程被打断时的trapframe

##练习二
###1. 设计实现过程
按照注释一步步操作，注意每个函数的参数。
这里解释一下前三步对于错误情况的处理：
alloc_proc分配失败时,直接跳转到fork_out返回错误参数即可.
setup_kstack失败时,需要把之前成功分配的proc空间释放掉,此时应该跳转到bad_fork_cleanup_proc进行相应空间的释放.
若是在copy_mm时失败,此时内核堆栈都proc都已经分配,则需要跳转至bad_fork_cleanup_kstack将已分配的内核堆栈和proc都释放.
另外需要注意两点：
1 在添加链表指针之前需要设置好pid，否则会出现unhandled page fault。pid在hash这里要用到。
2 要注意先关闭中断，完成后再打开中断。

###2. 请说明ucore是否做到给每个新fork的线程一个唯一的id？请说明你的分析和理由
是的，是唯一的id。首先，设置MAX_PROCESS为4096,而MAX_PID为MAX_PROCESS的两倍.这个设置一般来说已经可以保证么个线程都有一个唯一的id.
另外，ucore还有别的实现保证这个唯一性的过程：
实现的代码如下：
```
if (last_pid >= next_safe) {
inside:
    next_safe = MAX_PID;
repeat:
    le = list;
    while ((le = list_next(le)) != list) {
        proc = le2proc(le, list_link);
        if (proc->pid == last_pid) {
            if (++ last_pid >= next_safe) {
                if (last_pid >= MAX_PID) {
                    last_pid = 1;
                }
                next_safe = MAX_PID;
                goto repeat;
            }
        }
        else if (proc->pid > last_pid && next_safe > proc->pid) {
            next_safe = proc->pid;
        }
    }
}
```
代码的意思是
每次在PCB链表上顺序遍历,如果进程id一直是逐次加一分配的,那么就找到链表末尾的id,然后再加1并返回
如果PCB链表中有某个proc被删除,即线程结束了,那么else if中的语句作用就是把last_pid取为那个被删除的proc在顺序计数时的id.
另外,如果所有线程号都已经被分配出去了,那么在最内层if判断,会不断跳转到repeat,不断重复while循环,直到PCB链表中某个线程被删除,有空余的pid为止.


##练习三
###1. 在本实验的执行过程中，创建且运行了几个内核线程

在proc_init中可以看到,本次实验中,创建了和运行了两个线程,一个是idleproc,该线程实际上一运行就被调度了,另外一个是init,主要执行了init_main函数,输出了一些语句.

###2. 语句local_intr_save(intr_flag);....local_intr_restore(intr_flag)的作用

上面已经说明过,这两个语句的作用是关中断和开中断,以保证在这两个语句之间的代码段是原子操作,不被打断.这样做是为了保证一些安全性,例如在do_fork中将proc插入proc链表,proc_run中线程切换等等地方都需要保证原子操作.这两个函数的实现如下:
```
static inline bool
__intr_save(void) {
    if (read_eflags() & FL_IF) {
        intr_disable();
        return 1;
    }
    return 0;
}    
static inline void
__intr_restore(bool flag) {
    if (flag) {
        intr_enable();
    }
}
#define local_intr_save(x)      do { x = __intr_save(); } while (0)
#define local_intr_restore(x)   __intr_restore(x);
```
即关中断时,先查看此时中断是否开启,是的话,就关中断,并返回1,保存在intr_flag中,执行外原子代码段后,local_intr_restore根据intr_flag判断中断是否关闭,是的话就开启中断.里面的实现比较好,避免了很多重复操作.
在这里的作用就是防止这段程序被中断打断。