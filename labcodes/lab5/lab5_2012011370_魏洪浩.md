#练习五
##1. 设计实现过程
这次需要补充的地方比较多，有部分是对之前lab的升级。
1 首先，在alloc_proc函数.proc_struct增加了4个变量，分别是wait_state（设置为0）和cptr,yptr,optr（设置为NULL），这里cptr指的是子进程，yptr指的是弟弟进程，optr指的是哥哥进程。

2 do_fork函数,在调用alloc_proc函数之后,根据注释，需要增加下面两个语句:
```
proc->parent = current;
assert(current->wait_state == 0);
```
lab5在这里和lab4相比，增加了进程之间关系的管理。也就是说，proc_list的增加删除不能简单地add remove，有专门函数如set_links维护。
```
set_links(struct proc_struct *proc) {
    list_add(&proc_list, &(proc->list_link));
    proc->yptr = NULL;
    if ((proc->optr = proc->parent->cptr) != NULL) {
        proc->optr->yptr = proc; //如果它父亲的孩子非空,那么他父亲的那个孩子就是它哥哥
    }													//然后设置它哥哥的弟弟是他自己
    proc->parent->cptr = proc; //设置它父亲当前的孩子为它
    nr_process ++;
}

// remove_links - clean the relation links of process
static void
remove_links(struct proc_struct *proc) {
    list_del(&(proc->list_link));
    if (proc->optr != NULL) {  //如果它有哥哥,那么设置它哥哥的弟弟为它的弟弟,即把自己删除
        proc->optr->yptr = proc->yptr;
    }
    if (proc->yptr != NULL) { //如果他有弟弟,那么设置它弟弟的哥哥为它的哥哥
        proc->yptr->optr = proc->optr;
    }
    else { //如果它没有弟弟,那么就设置它父亲的孩子为它哥哥,即他父亲只有它哥哥一个孩子
       proc->parent->cptr = proc->optr;
    }
    nr_process --;
}
```
上面是set和remove link函数的代码。可以看到它对cptr,optr和yptr这些表达关系的成员变量有很明确的说明，主要就是在加入proc_list外增添了这些步骤，需要注意的是，这里已经有nr_process++和list_add等动作，所以lab4里面的部分要注释掉。

3 在trap.c里面设置SYS_CALL的idt
```
SETGATE(idt[T_SYSCALL], 1, GD_KTEXT, __vectors[T_SYSCALL], DPL_USER);
```
4 在trap.c中
调用SYS_CALL的时候，这里一定要注意把lab1的print_ticks注释掉，不然编译会出错
```
ticks ++;
if (ticks % TICK_NUM == 0) {
    //print_ticks();
    assert(current != NULL);
    current->need_resched = 1; //原来时间片是在这里做的!
}
```
5
在proc.c里面设置中断帧
```
    tf->tf_cs = USER_CS;
    tf->tf_ds = USER_DS;
    tf->tf_es = USER_DS;
    tf->tf_ss = USER_DS;
    tf->tf_esp = USTACKTOP;
    tf->tf_eip = elf->e_entry;
    tf->tf_eflags |= FL_IF;
    ret = 0;
```
因为是运行在用户态,所以cs/ds/es/ss等段寄存器都直接设置为用户态的段即可,esp页设置为用户态的栈顶即可.另外需要设置eip,是的进程切换后能执行用户程序的第一条语句,所以eip就设为elf中保存的程序入口地址.之后使能中断就行.

##2. 用户态进程被ucore选择占用CPU执行（RUNNING态）到具体执行应用程序第一条指令的整个经过
do_execv加载具体应用函数前，是在内核线程userproc.在idleproc后调度后,执行initproc,initproc线程执行的是init_main函数.init_main函数中,又创建了一个内核线程,该内核线程执行user_main函数.然后initproc进入wait状态,等待其子线程,即userproc退出.initproc进入等待状态后,schedule则会选择userproc,执行proc_run完成进程的切换,从initproc切换到userproc.proc_run函数主要部分如下:
```
current = proc;
load_esp0(next->kstack + KSTACKSIZE);
lcr3(next->cr3); //进程地址空间的切换
switch_to(&(prev->context), &(next->context));
```
这里可以看lab4中对这里的解释，和swtich_to.s有关
创建新线程时,do_fork中调用了copy_thread函数,该函数指定了next->context中的eip,如下:
```
proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE) - 1;
*(proc->tf) = *tf;
proc->tf->tf_regs.reg_eax = 0;
proc->tf->tf_esp = esp;
proc->tf->tf_eflags |= FL_IF;

proc->context.eip = (uintptr_t)forkret;
proc->context.esp = (uintptr_t)(proc->tf);
```
以看到语句proc->context.eip = (uintptr_t)forkret,即切换后的新线程,切换后要执行的函数是forkret.该函数的实现如下:
```
static void forkret(void) {
    forkrets(current->tf);
}
```
其中,forkrets是汇编函数,在trapentry.S中
trap.c中可以看到,T_SYSCALL系统调用执行了函数syscall(),该函数在syscall.c中,根据系统调用号而执行sys_exec这一服务例程,而sys_exec又调用了do_execve函数.do_execve函数位于proc.c中,先清空了当前进程current(此时即userproc)的mm,把页目录表该位内核页目录表,然后调用了load_icode.load_icode函数完成了以下任务:

```
1. 用mm_create分配一个mm_struct
2.用setup_pgdir申请了一个页目录表,并把UCore内核页目录表拷贝进去
3.根据ELF文，调用mm_map函数对各个段建立对应的vma结构，并把vma插入到mm结构中
4.根据执行程序各个段的大小分配物理内存空间虚拟地址,在页表中建立好物理地址和虚拟地址的映射关系，
   把程序各   个段的内容拷贝到相应的内核虚拟地址中
5.调用mm_mmap函数建立用户栈的vma结构,并分配一定数量的物理内存且建立好栈的虚实映射关系
6. 设置current的mm和cr3为上述新分配的用户空间
7.修改trapframe
```
设置中断帧时,把 tf->tf_eip设置为了用户程序的入口地址elf->e_entry,返回.当这些中断服务例程逐一返回后,SYS_exec系统调用结束,恢复原进程的上下文,此时eip已经被修改为执行程序的入口地址,则会跳转执行执行程序的第一条语句.
总结起来就是
做了switch_to之后，返回到了fortret函数，以里面的tf为参数调用fortrets， 之后回去到了iret之后就会使用这个tf来返回到tf指向的地方。 刚才初始化的时候把tf设置成了用户态的段，那么回去之后就会变成用户态的程序，然后开始执行。

##练习二
###1. "Copy on Write机制"的设计概要

COW即父进用fork新创建一个子进程时，并不复制全部的资源给子进程，若父进程或子进程要对相应内容做写操作，才进行复制。在proc.c的do_fork函数中，可以看到，资源的复制实际上是调用copy_mm函数进行的。现在lab5中，调用copy_mm时，传递的clone_flag都为0，即复制。可以利用此参数，设置为1，mm沿用父进程的。当实际写时，在进行复制。

##练习三
###1. 请分析fork/exec/wait/exit在实现中是如何影响进程的执行状态的

do_fork创建一个新线程,在alloc_proc中,线程初始状态是PROC_UNINIT,do_fork在完成其他设置工作后,会调用wakeup_proc,将新建的线程的状态设置为PROC_RUNNABLE

do_execve会将进程的mm置为NULL,然后调用load_icode,将用户新城拷贝进来,为用户进程建立处于用户态的新的内存空间以及用户栈,然后转到用户态执行.可以说,进程的内容变了,但pid没有变,进程的状态页state也没有变.

do_wait.对某个进程执行do_wait函数,如果该进程是当前进程的子进程,且该进程不是僵尸状态;或者该进程是idleproc,且当前进程有子进程,子进程不是僵尸状态,那么就是当前进程为PROC_SLEEPING,进入等待状态.即将当前进程设置为PROC_SLEEPING状态,直到其某个子进程变为僵尸状态.

do_exit中会将当前进程设为PROC_ZOMBIE状态.如果其父进程处于等待孩子的等待状态,则唤醒父进程,将父进程设为PROC_RUNNABLE状态.如果当前进程有子进程,那么会将这些子进程的父进程设为initproc,因为current退出了.如果某个子进程处于僵尸状态且initproc处于等待孩子状态,则会唤醒initproc.

###2. 请给出ucore中一个用户态进程的执行状态生命周期图
```
PROC_UNINIT(创建)     PROC_ZOMBIE(结束)
            |                   ^    do_exit
            | do_fork           |
            |                   |
            v                   |        do_wait
          PROC_RUNNABLE(就绪)-------------------------> PROC_SLEEPING(等待)
                            <------------------------
                                  wakeup_proc
```