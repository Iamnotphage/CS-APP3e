# Arch Lab

首先是安装好需要的内容。

`flex`, `bison`

`gcc`和`make`就不多说了。

然后根据自己情况调整`Makefile`(sim各级目录下的`Makefile`)

> 笔者遇到-lfl找不到的情况，前往Makefile把LEXLIB = -lfl改成空就行了
>
> 此外就是GUI mode的情况，我懒得安装tcl/tk直接把Makefile相关的注释就行

仔细阅读`writeup`和书本`chapter 4`

# Part A

工作在`arch/sim/misc`目录下

仿照书本`figure 4-7`并对照`example.c`容易写出`sum.ys`:

(最后一行空行构成EOF)

然后`./yas sum.ys`和`./yis sum.yo`检查`Status`和`%rax`是否正常

```asm
# Execution begins at address 0
        .pos 0
        irmovq stack, %rsp
        call main
        halt

# Sample linked list
.align 8
ele1:
        .quad 0x00a
        .quad ele2
ele2:
        .quad 0x0b0
        .quad ele3
ele3:
        .quad 0xc00
        .quad 0

main:
        irmovq  ele1, %rdi
        call    sum_list
        ret

# long sum_list(list_ptr ls)
# ls in %rdi
sum_list:
        xorq    %rax, %rax      # long val = 0;
loop:
        andq    %rdi, %rdi      # stop when %rdi == 0
        je      exit
        mrmovq  (%rdi), %r10
        addq    %r10, %rax      # val += ls -> val;
        mrmovq  8(%rdi), %rdi
        jmp     loop
exit:   
        ret

# Stack starts here and grows to lower addresses
	    .pos 0x200
stack:

```

然后是`rsum.ys`:

```asm
# Execution begins at address 0
        .pos 0
        irmovq stack, %rsp
        call main
        halt

# Sample linked list
        .align 8
ele1:
        .quad 0x00a
        .quad ele2
ele2:
        .quad 0x0b0
        .quad ele3
ele3:
        .quad 0xc00
        .quad 0

main:
        irmovq  ele1, %rdi
        call    rsum_list
        ret

# long rsum_list(list_ptr ls)
# ls in %rdi
rsum_list:
        pushq   %rbx            # callee saved register
        xorq    %rax, %rax
        andq    %rdi, %rdi
        jne     else
        xorq    %rax, %rax
        jmp     exit
else:
        mrmovq  (%rdi), %rbx    # %rbx is val
        mrmovq  8(%rdi), %rdi	# %rdi = %rdi -> next
        call    rsum_list
        addq    %rbx, %rax	    # val + rsum_list()
exit:
        popq	%rbx
        ret

# Stack starts here and grows to lower addresses
	    .pos 0x200
stack:

```

最后是`copy.ys`也很简单，照着c代码写就行:

```asm
# Execution begins at address 0
        .pos 0
        irmovq stack, %rsp
        call main
        halt

        .align 8
# Source block
src:
        .quad 0x00a
        .quad 0x0b0
        .quad 0xc00
# Destination block
dest:
        .quad 0x111
        .quad 0x222
        .quad 0x333

main:
        irmovq  src, %rdi
        irmovq  dest, %rsi
        irmovq  $3, %rdx
        call    copy_block
        ret

# long copy_block(long *src, long *dest, long len)
# src in %rdi, dest in %rsi, len in %rdx
copy_block:
        irmovq  $8, %r9
        irmovq  $-1, %r10
        xorq    %rax, %rax

loop:
        andq    %rdx, %rdx
        je      exit

        mrmovq  (%rdi), %r8     # %r8 is val
        addq    %r9, %rdi
        rmmovq  %r8, (%rsi)
        addq    %r9, %rsi
        xorq    %r8, %rax
        addq    %r10, %rdx
        jmp     loop
exit:
        ret

# Stack starts here and grows to lower addresses
	    .pos 0x200
stack:

```

最后这个`copy.ys`经过`./yas`编译`./yis`运行检查后，`%rax`的值仍然是`0xcba`

并且内存的内容相应地也改变。

这个part有30分，每个文件10分。

# Part B

在进行这部分内容时，请选确保已经阅读CS:APP3e的4.2章节和4.3章节

首先通过前面的部分可以知道指令的字节码如下:

表格一列代表4bit

|0|1|2|3|4|5|...|19|
|--|--|--|--|--|--|--|--|
|icode|ifun|rA|rB|||||

后续的空格代表立即数(64bit)

其次，要对执行一条指令的几个阶段了解(查看figure 4-18):

1. 取指
2. 译码
3. 执行
4. 访存
5. 写回
6. 更新PC

所以要实现`iaddq`的指令，根据书本的描述:

```text
C 0 F [rB] [V]
```

前面的`icode`和`ifun`和`rA`都是固定的，`V`表示立即数。

仿照课本的例子，`iaddq`的执行阶段也就是:

```text
1. icode:ifun <- M1[PC]         # 开始取指
2. rA:rB      <- M1[PC + 1]
3. valC       <- M8[PC + 2]     # 64bit立即数
4. valP       <- PC + 10        # 1B 指令类别 1B寄存器编号 8B立即数
5. valB       <- R[rB]          # 开始译码
6. valE       <- valC + valB    # 开始执行
7. R[rB]      <- valE           # 开始写回
8. PC         <- valP           # 更新PC
```

到这一步，要确保阅读过`4.3.2`已经了解`SEQ`的硬件结构，方便对照修改`seq-full.hcl`文件

---

对照`figure 4-27`SEQ的取指阶段，修改`seq-full.hcl`

首先`seq-full.hcl`已经在42行定义了

`wordsig IIADDQ	'I_IADDQ'`

我们只需要到`fetch stage`修改对应的内容，修改它为valid指令，同时需要寄存器和常数。

```text
################ Fetch Stage     ###################################

# Determine instruction code
word icode = [
	imem_error: INOP;
	1: imem_icode;		# Default: get from instruction memory
];

# Determine instruction function
word ifun = [
	imem_error: FNONE;
	1: imem_ifun;		# Default: get from instruction memory
];

bool instr_valid = icode in 
	{ INOP, IHALT, IRRMOVQ, IIRMOVQ, IRMMOVQ, IMRMOVQ,
	       IOPQ, IJXX, ICALL, IRET, IPUSHQ, IPOPQ, IIADDQ };

# Does fetched instruction require a regid byte?
bool need_regids =
	icode in { IRRMOVQ, IOPQ, IPUSHQ, IPOPQ, 
		     IIRMOVQ, IRMMOVQ, IMRMOVQ, IIADDQ };

# Does fetched instruction require a constant word?
bool need_valC =
	icode in { IIRMOVQ, IRMMOVQ, IMRMOVQ, IJXX, ICALL, IIADDQ };
```

---

同样地，对照`figure 4-28`SEQ的译码和写回阶段，修改`seq-full.hcl`

我们的`iaddq`只需要用到`rB`表示的寄存器，并且写回`rB`寄存器，不需要根据`Cnd`条件信号进行操作。

```text
################ Decode Stage    ###################################

## What register should be used as the A source?
word srcA = [
	icode in { IRRMOVQ, IRMMOVQ, IOPQ, IPUSHQ  } : rA;
	icode in { IPOPQ, IRET } : RRSP;
	1 : RNONE; # Don't need register
];

## What register should be used as the B source?
word srcB = [
	icode in { IOPQ, IRMMOVQ, IMRMOVQ, IIADDQ  } : rB;
	icode in { IPUSHQ, IPOPQ, ICALL, IRET } : RRSP;
	1 : RNONE;  # Don't need register
];

## What register should be used as the E destination?
word dstE = [
	icode in { IRRMOVQ } && Cnd : rB;
	icode in { IIRMOVQ, IOPQ, IIADDQ} : rB;
	icode in { IPUSHQ, IPOPQ, ICALL, IRET } : RRSP;
	1 : RNONE;  # Don't write any register
];

## What register should be used as the M destination?
word dstM = [
	icode in { IMRMOVQ, IPOPQ } : rA;
	1 : RNONE;  # Don't write any register
];
```

---

同样继续，继续`figure 4-29`来修改执行阶段的`.hcl`

在ALU的输入口中，A口输入valC, B口输入valC, 同时设定条件码(因为本质上同IOPQ)

```text
################ Execute Stage   ###################################

## Select input A to ALU
word aluA = [
	icode in { IRRMOVQ, IOPQ } : valA;
	icode in { IIRMOVQ, IRMMOVQ, IMRMOVQ, IIADDQ } : valC;
	icode in { ICALL, IPUSHQ } : -8;
	icode in { IRET, IPOPQ } : 8;
	# Other instructions don't need ALU
];

## Select input B to ALU
word aluB = [
	icode in { IRMMOVQ, IMRMOVQ, IOPQ, ICALL, 
		      IPUSHQ, IRET, IPOPQ, IIADDQ } : valB;
	icode in { IRRMOVQ, IIRMOVQ } : 0;
	# Other instructions don't need ALU
];

## Set the ALU function
word alufun = [
	icode == IOPQ : ifun;
	1 : ALUADD;
];

## Should the condition codes be updated?
bool set_cc = icode in { IOPQ, IIADDQ };
```

---

`iaddq`不需要访问内存。

访存阶段跳过，直接到更新PC:

更新PC阶段，发现并不需要修改，因为默认就是通过`valP`来更新PC

---

最后make，测试就行。

in `sim/seq`
```bash
unix > make clean
unix > make VERSION=full
unix > ./ssim -t ../y86-code/asumi.yo # init test
unix > cd ../y86-code; make testssim  # benchmark test
unix > cd ../ptest; make SIM=../seq/ssim TFLAGS=-i # regression test
```

ALL SUCCEED就行

# Part C

`Part A`熟悉这个模拟的Y86架构

`Part B`熟悉指令顺序执行的情况，包括取指译码执行等操作，但是没有流水线，导致硬件浪费和效率感人。

这个`Part`将针对流水线进行优化`ncopy.ys`的性能。

在进行之前，请先确保阅读`4.4`和`4.5`，了解流水线的概念，SEQ+和SEQ的区别，了解流水线寄存器以及仔细查看`figur 4-41`。此外，`writeup`说阅读`5.8`的`loop unrolling`会有帮助。

总之，终极目标是优化`ncopy.ys`的性能，观察其代码发现

```nasm
# You can modify this portion
	# Loop header
	xorq %rax,%rax		# count = 0;
	andq %rdx,%rdx		# len <= 0?
	jle Done		# if so, goto Done:

Loop:	mrmovq (%rdi), %r10	# read val from src...
	rmmovq %r10, (%rsi)	# ...and store it to dst
	andq %r10, %r10		# val <= 0?
	jle Npos		# if so, goto Npos:
	irmovq $1, %r10
	addq %r10, %rax		# count++
Npos:	irmovq $1, %r10
	subq %r10, %rdx		# len--
	irmovq $8, %r10
	addq %r10, %rdi		# src++
	addq %r10, %rsi		# dst++
	andq %rdx,%rdx		# len > 0?
	jg Loop			# if so, goto Loop:
```

有多个地方用到了这样的一套招数:

```nasm
irmovq $1, %r10
addq   %r10, %rax
```

因为原本的`OPq`不支持立即数直接和寄存器运算。如果我们实现`iaddq`会不会有优化呢？(Let's figure it out.)

## 实现IADDQ

这里的实现`iaddq`和`Part B`的差别不大。

记得对照`4.5.7 PIPE各个阶段的实现`来修改`pipe-full.hcl`

### Fetch Stage

取指阶段修改三个字段的内容，同`Part B`一样：`instr_valid`, `need_regids`, `need_valC`(其他没涉及)

```text
# Is instruction valid?
bool instr_valid = f_icode in 
	{ INOP, IHALT, IRRMOVQ, IIRMOVQ, IRMMOVQ, IMRMOVQ,
	  IOPQ, IJXX, ICALL, IRET, IPUSHQ, IPOPQ, IIADDQ };

# Determine status code for fetched instruction
word f_stat = [
	imem_error: SADR;
	!instr_valid : SINS;
	f_icode == IHALT : SHLT;
	1 : SAOK;
];

# Does fetched instruction require a regid byte?
bool need_regids =
	f_icode in { IRRMOVQ, IOPQ, IPUSHQ, IPOPQ, 
		     IIRMOVQ, IRMMOVQ, IMRMOVQ, IIADDQ };

# Does fetched instruction require a constant word?
bool need_valC =
	f_icode in { IIRMOVQ, IRMMOVQ, IMRMOVQ, IJXX, ICALL, IIADDQ };
```

### Decode & Write Back Stage

译码阶段，跟`Part B`一样，需要`B source`和`E destination`

```text
## What register should be used as the B source?
word d_srcB = [
	D_icode in { IOPQ, IRMMOVQ, IMRMOVQ, IIADDQ  } : D_rB;
	D_icode in { IPUSHQ, IPOPQ, ICALL, IRET } : RRSP;
	1 : RNONE;  # Don't need register
];

## What register should be used as the E destination?
word d_dstE = [
	D_icode in { IRRMOVQ, IIRMOVQ, IOPQ, IIADDQ} : D_rB;
	D_icode in { IPUSHQ, IPOPQ, ICALL, IRET } : RRSP;
	1 : RNONE;  # Don't write any register
];
```

### Execute Stage

还是一样，`aluA`, `aluB`, `set_cc`(同正常的`OPq`)

```text
## Select input A to ALU
word aluA = [
	E_icode in { IRRMOVQ, IOPQ } : E_valA;
	E_icode in { IIRMOVQ, IRMMOVQ, IMRMOVQ, IIADDQ } : E_valC;
	E_icode in { ICALL, IPUSHQ } : -8;
	E_icode in { IRET, IPOPQ } : 8;
	# Other instructions don't need ALU
];

## Select input B to ALU
word aluB = [
	E_icode in { IRMMOVQ, IMRMOVQ, IOPQ, ICALL, 
		     IPUSHQ, IRET, IPOPQ, IIADDQ } : E_valB;
	E_icode in { IRRMOVQ, IIRMOVQ } : 0;
	# Other instructions don't need ALU
];

## Set the ALU function
word alufun = [
	E_icode == IOPQ : E_ifun;
	1 : ALUADD;
];

## Should the condition codes be updated?
bool set_cc = E_icode in { IOPQ, IIADDQ } &&
	# State changes only during normal operation
	!m_stat in { SADR, SINS, SHLT } && !W_stat in { SADR, SINS, SHLT };
```

### Memory Stage

`iaddq`不涉及。

### Pipeline Register Control

`iaddq`因为没有涉及内存访问，所以不会出现加载/使用冒险，所以此处也不涉及。

### 测试pipe-full.hcl

阅读`writeup`关于build和run的部分，查看如何测试程序。

```bash
unix > ./psim -t ../y86-code/asumi.yo
unix > cd ../ptest/; make SIM=../pipe/psim TFLAGS=-i
```

## 优化ncopy.ys

在优化之前，先看看我们怎么build和run我们的程序。

在`writeup`中的`Evaluation`中说明了这个部分的目标:

> You should be able to achieve an average CPE of less than 9.00. Our best version averages 7.48.

我们要将CPE(Cycles Per Element)降低到9.0就算合格。

先看看0优化的程序。

```bash
unix > ./correctness.pl -p
unix > ./benchmark.pl
```

得分为0.0/60.0, Average CPE = 15.18

make sense......

把我们最初的设想(`iaddq`替换原本的内容)实现一下:

```nasm
# You can modify this portion
	# Loop header
	xorq %rax,%rax		# count = 0;
	andq %rdx,%rdx		# len <= 0?
	jle Done		# if so, goto Done:

Loop:	mrmovq (%rdi), %r10	# read val from src...
	rmmovq %r10, (%rsi)	# ...and store it to dst
	andq %r10, %r10		# val <= 0?
	jle Npos		# if so, goto Npos:
	iaddq $1, %rax		# count++
Npos:
	iaddq $-1, %rdx		# len--
	iaddq $8, %rdi		# src++
	iaddq $8, %rsi		# dst++
	andq %rdx,%rdx		# len > 0?
	jg Loop			# if so, goto Loop:
```

同样测试，得分居然还是0

Average CPE = 12.70虽然有提升，但是很少（毕竟只是照抄了Part B）。

---

好吧，看来光靠`iaddq`是无法取得巨大突破，只能求助于第五章节的内容。

