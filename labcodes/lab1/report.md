##第一问问题
------
通过make V=命令和查看makefile的相关信息，可以看出编译生成的过程大概分为以下几步（因为相关的代码比较多，所以只取部分比较有代表的makefile和make信息）：

首先定义compiler相关的信息，然后将所有的.c文件编译成.o文件
这里主要注明以下各个参数的含义：
```
HOSTCC    	:= gcc
HOSTCFLAGS	:= -g -Wall -O2
CC		:= $(GCCPREFIX)gcc
CFLAGS	:= -fno-builtin -Wall -ggdb -m32 -gstabs -nostdinc $(DEFS)
CFLAGS	+= $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)
```
```
gcc -Ikern/init/ -fno-builtin -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/init/init.c -o obj/kern/init/init.o
```



|-I	|添加头文件路径|
|-fno-builtin	|除了__builtin_开头进行引用，不然不识别所有inline函数|
|-Wall	|显示所有警告|
|-O2	|优化级别|
|-ggdb	|生成gdb专用的调试信息|
|-m32	|进行32位的编译|
|-gstabs	|以stabs格式生成调试信息|
|-nostdinc	|使编译器不在系统缺省的头文档目录里面找头文档|
|-fno-stack-protector	|禁用堆栈保护|
|-c 	|只编译不链接|
|-o 	|指定输出文件|

然后用ld命令把生成的.o文件链接起来，即创建bootblock，生成bootblock.o

```
$(V)$(LD) $(LDFLAGS) -N -e start -Ttext 0x7C00 $^ -o $(call toobj,bootblock)
```
```
ld -m    elf_i386 -nostdlib -T tools/kernel.ld -o bin/kernel  obj/kern/init/init.o obj/kern/libs/readline.o obj/kern/libs/stdio.o obj/kern/debug/kdebug.o obj/kern/debug/kmonitor.o obj/kern/debug/panic.o obj/kern/driver/clock.o obj/kern/driver/console.o obj/kern/driver/intr.o obj/kern/driver/picirq.o obj/kern/trap/trap.o obj/kern/trap/trapentry.o obj/kern/trap/vectors.o obj/kern/mm/pmm.o  obj/libs/printfmt.o obj/libs/string.o
+ ld bin/bootblock
ld -m    elf_i386 -nostdlib -N -e start -Ttext 0x7C00 obj/boot/bootasm.o obj/boot/bootmain.o -o obj/bootblock.o
```
这里主要注明以下各个参数的含义：
|-m elf_i386	|elf格式输出32位的文件|
|-nostdlib	|不连接系统标准启动文件和标准库文件|
|-T 	|指定自己的链接脚本|
|-o 	|指定输出文件|
|-N 	|ldconfig不重建缓存文件|
|-e start 	|start为入口|
|-Ttext 0x7c00 	|以0x7c00地址开始|

最后用dd命令创建UCore.img文件
```
$(V)dd if=/dev/zero of=$@ count=10000
$(V)dd if=$(bootblock) of=$@ conv=notrunc
$(V)dd if=$(kernel) of=$@ seek=1 conv=notrunc
```
这三句话的意思分别是用空白符初始化，然后以bootblock为输入，接着输入kernel
然后创建UCore.img 文件
```
dd if=bin/bootblock of=bin/ucore.img conv=notrunc
```

视频中陈老师提示在tools目录下的sign.c文件里有相关的信息
```
    buf[510] = 0x55;
    buf[511] = 0xAA;
```
可以看到主引导扇区是一个扇区大小为512字节为扇区
其中两个字分别为0x55和0xAA
其中程序部分不超过510字节
```
if (st.st_size > 510) {
    fprintf(stderr, "%lld >> 510!!\n", (long long)st.st_size);
    return -1;
}
```
这个程序部分应该是446字节的启动代码和64字节的硬盘区分表


##第二问问题
------
首先根据老师在答案里面的提示，把makefile改一下，用q.log导出，方便查看，然后更改gdbinit。
完成后看的就比较清楚了。
###2.1
```
0xfffffff0:  ljmp   $0xf000,$0xe05b
0xfffffff5:  xor    %dh,0x322f
0xfffffff9:  xor    (%bx),%bp
0xfffffffb:  cmp    %di,(%bx,%di)
0xfffffffd:  add    %bh,%ah
0xffffffff:  add    %al,(%bx,%si)
```
和教程上说的一样，加电后第一条指令用于长跳转，目标指向0x000fe05b，单步追踪，然后反汇编
其中部分代码如下：
```
0x000fe05b:  cmpl   $0x0,%cs:0x65a4
0x000fe062:  jne    0xfd2b9
0x000fe066:  xor    %ax,%ax
0x000fe068:  mov    %ax,%ss
0x000fe06a:  mov    $0x7000,%esp
0x000fe070:  mov    $0xf3c4f,%edx
0x000fe076:  jmp    0xfd12a
0x000fd12a:  mov    %eax,%ecx
0x000fd12d:  cli    
0x000fd12e:  cld    
0x000fd12f:  mov    $0x8f,%eax
```
这里有一个循环，猜测是循环加载bootloader到内存中。

###2.2
按照
```
set architecture i8086
target remote :1234
set architecture i8086  
b *0x7c00  
continue          
x /2i $pc  
set architecture i386  
```
可以看到
```
0x7c00:  cli    
0x7c01:  cld 
```
断点正常

###2.3
q.log在之前的makefile里面已经说明怎么更改，然后将单步追踪的反汇编代码和实际代码进行比较，基本相同，但是具体的位置貌似是不太一样的。

###2.4 
用b *0x。。。。。命令设置断点

##第三问问题
------
###3.1
A20的意义在于把寻址从16位变成寻址32位，这样就实际可以使用4G内存。而我们的UCore基于的是80386，所以自然需要A20，具体来说的话，加电后处于实模式，16为寻址，bootloader，开启保护模式->32位寻址。32位寻址，4G内存，则必须开启A20.
具体开启方法是通过写8042端口进行开启，空闲的时候请求输入数据，成功响应后0x60端口输入0xdf，打开A20
```
movb $0xdf, %al                               
outb %al, $0x60 
```
###3.2
GDT表的初始化在pmm.c的gdt_init函数中进行，其中bootasm.S中初始化了一个bootstrap GDT。段机制在现实中是用来完成逻辑地址到线性地址的转换，但是看上去这个GDT只是把虚拟地址直接作为物理地址。如果记得没错的话，老师在视频里说过UCore的段机制是弱化的。

###3.3 进入保护模式是在bootasm.s中执行
```
lgdt gdtdesc
movl %cr0, %eax
orl $CR0_PE_ON, %eax
movl %eax, %cr0
```

功能是把cr0寄存器的0比特位置为1，使用保护模式
这样，进行长跳转后，CS IP重新加载进入保护模式。


##第四问问题
------
###4.1 
bootmain.c中的readsect函数可以较好地解释一个扇区的读写
```
waitdisk(); 等待硬盘准备好 
oubt  输入命令，设置读，相关的状态寄存器等等
waitdisk(); 等待硬盘准备好 
insl(0x1F0, dst, SECTSIZE / 4); 读取数据到指定内存 
```

###4.2 
这个在bootmian()函数看出
```
readseg((uintptr_t)ELFHDR, SECTSIZE * 8, 0); 读取UCore OS文件的elf文件头
e_magic==ELF_MAGIC，elf文件头判断文件是否有效
获取首地址和表项数目
利用readseg函数，将函数段通过循环读入内存
跳转至UCore OS首地址，UCore OS开始有控制权
```




##第五问问题
------
按照注释一步步实现代码
用read_ebp读取当前的ebp值
用read_eip读取当前的eip值
然后沿函数调用栈逐层打印相关的信息，这里通过循环实现。
在循环的每一层，打印eip和ebp的16进制值。
这里分别解释和ebp相关的变量值
ebp->内存单元保存的是上一个函数的ebp值 
ebp+1->返回地址	
ebp+2->第一个参数的地址(代码中用的是uint32_t* argp = (uint32_t*)ebp + 2)
如果有一个数组，ebp+2是起始地址，则args[0:4]代表四个参数
再调用print_debuginfo打印相关的信息。
注释最后说明了popup a calling过程
ebp->上一个函数的ebp值
eip->上一个函数的返回地址

最后一行是bootmain的最后一句话（调用kern_init）的语句地址。

##第六问问题
------
###6.1
一个表项为一个中断门描述符，在mmu.h里面有个关于门描述符的struct gatdesc，可以看出一个中断描述符大小为8个字节。其中，0~15位是offset的低16位，48~63位是offset的高16位，16~31位是selector。selector可以查到起始地址，这样再加上offset就是中断处理代码的入口。

###6.2 
根据注释，这里主要有一个地方需要更改。首先是在idt_init完成初始化过程,题目要求我们使用mmu.h中的SETGATE宏初始化各个中断描述符，其中各个参数为：优先级为0，T_SYSCALL 0x80为3，sel为GD_KTEXT，起始位置在__vectors[]中。

###6.3
按照注释里面的要求，只要到达TICK_NUM就调用print_ticks()输出。