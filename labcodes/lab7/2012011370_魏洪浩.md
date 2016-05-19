# 练习一

##1. 内核级信号量的设计描述

在UCore中，信号量的实现主要涉及sem.c，sem.h和check_sync.c三个文件，具体内容可以在这三个文件中查看。其中我们看sem.h，有信号量的结构体的定义：
```
    typedef struct {
        int value;
        wait_queue_t wait_queue;
    } semaphore_t;
```
这里内核级信号量semaphore里面有一个value表示当前剩余资源数以及waitqueue表示等待队列。  


有了上述结构体之后，信号量的实现和使用依赖一对PV操作。这两个操作在lab7中为down和up。

一个线程请求资源时使用down函数。对于P操作，即down函数，如下：
```
    static __noinline uint32_t __down(semaphore_t *sem, uint32_t wait_state) {
        bool intr_flag;
        local_intr_save(intr_flag);
        if (sem->value > 0) {
            sem->value --;
            local_intr_restore(intr_flag);
            return 0;
        }
        wait_t __wait, *wait = &__wait;
        wait_current_set(&(sem->wait_queue), wait, wait_state);
        local_intr_restore(intr_flag);
    
        schedule();
    
        local_intr_save(intr_flag);
        wait_current_del(&(sem->wait_queue), wait);
        local_intr_restore(intr_flag);
    
        if (wait->wakeup_flags != wait_state) {
            return wait->wakeup_flags;
        }
        return 0;
    }
```  

先判断value是否大于0，如果是，说明信号量有剩余，那么直接获得信号量并返回，如果value小于等于0，就把当前线程加入到等待队列中，让其睡眠，并用schedule调度其他的线程。等到返回的时候再调用，然后从等待队列里面删除。

需要注意的是value的操作应该是原子操作，此处通过开关中断来保证原子性，这里代码也是这样体现的。

一个线程释放资源时使用up函数，V操作，即up函数。
```
    static __noinline void __up(semaphore_t *sem, uint32_t wait_state) {
        bool intr_flag;
        local_intr_save(intr_flag);
        {
            wait_t *wait;
            if ((wait = wait_queue_first(&(sem->wait_queue))) == NULL) {
                sem->value ++;
            }
            else {
                assert(wait->proc->wait_state == wait_state);
                wakeup_wait(&(sem->wait_queue), wait, wait_state, 1);
            }
        }
        local_intr_restore(intr_flag);
    }
```  
首先取等待队列中的第一个进程，如果是空，说明现在等待队列里面没有进程等待信号量，这样value++即可，如果等待队列中有等待信号的进程，那么刚刚在down中调用schedule的进程会被唤醒，接着schedule()调用后执行，那么则会完成从等待队列中删除等操作。

*2.*用户态进程/线程提供信号量机制的设计方案

首先要判断lab7中的信号量是用户态还是内核态。check_sync.c中实现了我们在练习二中要解决的哲学家问题。check_sync函数是在proc.c中的init_main中调用的，属于内核线程级函数。对于最重要的up和down两个函数，都需要有对调度的调整，显然放在用户态中是不合适的，必须是在内核态中运行的。 一种方法是以新建系统调用sys_sem，sys_up，sys_down，分别用来新建信号量，释放资源和申请资源。 在具体的三个中断处理函数中分别调用内核态的三个函数即可。

# 练习二

##1.内核级条件变量的设计描述

在monitor.c与monitor.h中有两个结构需要我们注意：条件变量condvar_t与管程monitor_t。
```
    typedef struct condvar{
        semaphore_t sem; 
        int count;
        monitor_t * owner; // the owner(monitor) of this condvar
    } condvar_t;
```   
count是等在该条件变量上的进程数数，条件变量中还应有一个等待队列，注意练习一中，我们看到信号量semaphore_t中已经有一个等待队列了。


```
    typedef struct monitor{
        semaphore_t mutex;
        semaphore_t next;     
        int next_count;
        condvar_t *cv;
    } monitor_t;
```
其中mutex用于进入管程的互斥。
cv是条件变量，一个管程中可以有多个条件变量，所以声明为指针，也就是一个含多个条件变量的数组。
next信号量表示等待使用管程的线程
nextcount表示等待使用管程的线程数。


每个哲学家要的动作是：思考->拿刀叉->吃饭->放下刀叉->思考。刀叉看是管程控制的互斥资源。
拿刀叉的函数如下：
```
    void phi_take_forks_condvar(int i) {
         down(&(mtp->mutex)); //进入管程
         state_condvar[i] = HUNGRY;
         phi_test_condvar(i);
         if ( state_condvar[i] != EATING) { //取刀叉失败时   
        	 cond_wait(&mtp->cv[i]); //等待条件变量   
         }
          if(mtp->next_count>0)
             up(&(mtp->next));
          else
             up(&(mtp->mutex));
    }
```    
执行拿刀叉函数，就是进入管程，此时会先申请管程的mutex，以确保管程的互斥访问。
然后将自己状态设为饥饿，然后看自己左右的人是否在吃饭
如果取刀叉失败，就等在在自己的条件变量上
不然的话，如果有别的等待队列上的进程，就唤醒他，自己吃饭结束。不然的话就释放互斥锁。
```
    void
    cond_wait (condvar_t *cvp) {
        cvp->count++;
        if ( (cvp->owner) ->next_count>0) {
        	up(&(cvp->owner->next)); //sem_signal(monitor.next) 释放next信号，让执行了signal后等在next上的进程启动
        } else {
        	up(&(cvp->owner->mutex)); //没有进程等在next上，则开放管程入口
        }
        //调用cond_wait的进程等在cv的sem上
        down(&(cvp->sem));
        cvp->count--; //得到条件变量，等待数减1
    }
```   
该函数将等待在该条件变量上的进程数加1，然后检查管程的next等待队列。
应该优先执行那些执行了signal后等在next队列上的进程。
如果有这样的进程，就释放next信号量，利用up操作唤醒一个这样的进程。
否则就释放mutex，允许新的进程进入管程。
最后利用down操作，将当前进程设为等待状态并挂到条件变量的等待队列上，但被唤醒返回时，将count减1。

放下刀叉的函数如下：
```
    void phi_put_forks_condvar(int i) {
         down(&(mtp->mutex));
         state_condvar[i] = THINKING;
    
         phi_test_condvar(LEFT);
         phi_test_condvar(RIGHT);
         if(mtp->next_count>0)
            up(&(mtp->next));
         else
            up(&(mtp->mutex));
    }
```  
和phi_take_forks_condvar很相似，不同的就是把自己状态设成thinking，然后看左右是否需要刀叉。
这里要看一下phi_test_condvar函数。在phi_test_condvar函数中，吃完饭后调用cond_signal释放条件变量
```
    void 
    cond_signal (condvar_t *cvp) {
       if (cvp->count>0) {
        	   cvp->owner->next_count++;
        	   up(&(cvp->sem)); //释放条件变量
        	   down(&(cvp->owner->next)); //自己等在next上
        	   //回来后，next_count减一
        	   cvp->owner->next_count--;
       }
    }
```
表示此时该条件成立，可以唤醒等待该条件的线程。此时优先让等待条件的线程运行， 故把当前线程放入等待队列，唤醒相应线程，并重新调度。

##2.用户态进程/线程条件变量的设计方案
和练习一中的答案类似，我们也新建三个系统调用，来分别初始化，等待以及释放条件变量，用内核态来完成相关的调度。

##和答案不同
基本上差不多，其中priority和上次一样有时不能通过，然后用make run-priority的结果可以看截图。用上一次的方法无法解决，所以这次需要修改AX_TIME_SLICE，从20改成5，就可以解决问题。

##重要知识点
这次的实现很多和课上的内容并不一样，比图说up函数中，课上说的是value减一然后再判断value是否小于0，这样value为负数时，其值就可以表示等待队列里正在等待的进程数。但是大体的思路和课上还是一样的。
