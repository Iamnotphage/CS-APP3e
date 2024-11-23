# Cache Lab

在开始之前，先确保阅读CS:APPe3的第六章(尤其是`6.2`, `6.3`, `6.4`)

(关于SRAM和DRAM部分的文字和我看过的国内教材神似，很难不怀疑是不是抄袭CSAPP的)

然后就对高速缓存的结构有了新认识: (S, E, B, m)元组表示

先阅读一下`writeup`发现有个内存调试工具`valgrind`

在docker运行的容器里面安装一下

```bash
yum install valgrind
```

# Part A

在`writeup`中给出了例子:

```bash
[root@5fd2dc6af315 cache]# ./csim-ref -v -s 4 -E 1 -b 4 -t traces/yi.trace 
L 10,1 miss 
M 20,1 miss hit 
L 22,1 hit 
S 18,1 hit 
L 110,1 miss eviction 
L 210,1 miss eviction 
M 12,1 miss eviction hit 
hits:4 misses:5 evictions:3
```

我们的程序`csim.c`就是要模拟cache的机制，统计上述的输出信息。

首先来剖析一下这个输入输出

输入参数给定了(S, E, B, m) = (16, 1, 16, 64)

(m = 64bit 假设是64位机器)

所以地址分段应该是这样:

|t|s|b|
|-|-|-|
|56|4|4|

所以这个cache有16组，每组1行，一行的数据块有16个。

第一个操作`L 10,1 miss`因为cache为空，加载地址`0x10`时，第1组第0块没内容，所以miss

miss之后，将内存`0x10`开始的内容加载到cache中

接下来就是`M 20,1 miss hit`此时第二组仍然是空，所以miss，加载地址`0x20`开始的内容到cache中

之后修改，所以hit

后续同理......

---

对于编写`csim.c`有以下要求:

* 开头注释标明姓名和ID
* 不能有任何警告
* 对于任意参数s,E,b都能正确工作
* 忽略`trace`中的所有取指信息(`I`开头的)
* 最后必须调用`printSummary()`统计信息
* 在本实验中，您应假设内存访问已正确对齐，因此单次内存访问绝不会跨越块边界。有了这个假设，你就可以忽略 valgrind 跟踪中的请求大小。
  
而且eviction要满足LRU

这不就是leetcode的LRU题么

输入来自`valgrind`的trace信息，以及s, E, b参数，模拟cache行为并统计信息

在Part A开始之前，`writeup`有几个建议:

* 可以先在小的trace文件进行debug
* 推荐实现`-v`选项，毕竟方便debug
* 推荐使用`getopt.h`来解析参数(一般shell程序用这个来解析args)
* 每次数据加载 (L) 或存储 (S) 操作最多只能导致一次cache miss。数据修改操作 (M) 被视为加载然后存储到同一地址的操作。因此，一个 M 操作可能会导致两次缓存hits，或一次miss和一次hit加上一次可能的eviction。

解析参数参考一下`getopt()`函数很容易写出

这里重点考虑构造cache的数据结构。

首先是`cacheRow`的数据结构，包含一个`valid bit`和一个`tag` 由于不关注`block`的内容本身，所以`block`可以省略。

```c
int hits = 0;
int misses = 0;
int evictions = 0;

/**
 * @brief This is a cache line, which line has a valid bit, tags and blocks. 
 */
struct cacheRow {
    int valid;  /** valid bit */
    int tag;    /** tags */
};

/**
 * @brief The cache.
 */
struct cacheRow* cache = NULL;
```

其次是需要考虑`LRU`算法，如果`E` > 1的话，就要考虑组内eviction

如何手搓`LRU`也是有各种各样的解法，这里采用双向链表:

越靠近`dummyHead`的结点表示越常用，当需要eviction时，一般替换靠近`dummyTail`的结点。(详见leetcode的LRU缓存)

```c
/**
 * @brief A node for constructing a deque.
 */
typedef struct node{
    int offset;
    struct node* prev;
    struct node* next;
}node;

/**
 * @brief A deque, consist of a dummy head and a dummy tail.
 */
typedef struct deque{
    int size;
    int capacity;
    struct node* head;
    struct node* tail;
}deque;

/**
 * @brief For each SET, we have a deque (2^s deques)
 */
deque* deques;

/**
 * @brief Add the node to head of a deque.
 * 
 * @param[in] dq deque
 * @param[in] node node
 */
void addToHead(deque* dq, node* node) {
    struct node* head = dq -> head;
    node -> prev = head;
    node -> next = head -> next;
    head -> next -> prev = node;
    head -> next = node;
}

/**
 * @brief Remove a node from its deque.
 * 
 * @param[in] node node
 */
void removeNode(node* node) {
    node -> prev -> next = node -> next;
    node -> next -> prev = node -> prev;
}

/**
 * @brief Move a node to its deque's head.
 * 
 * @param[in] dq deque
 * @param[in] node node
 */
void moveToHead(deque* dq, node* node) {
    removeNode(node);
    addToHead(dq, node);
}

/**
 * @brief Remove a tail from a deque.
 * 
 * @param[in] dq deque
 * 
 * @return the tail node
 */
node* removeTail(deque* dq) {
    node* res = dq -> tail -> prev;
    removeNode(res);
    return res;
}
```

顺便实现几个API方便后续LRU调用

那么接下来就考虑`cache`的初始化，需要初始化各个组的deque:

```c
/**
 * @brief Initialize cache
 * 
 * @param[in] s set bits
 * @param[in] E lines per line
 */
void initCache(int s, int E) {
    int sets = 1 << s;

    struct cacheRow* cacheLines = (struct cacheRow*)malloc(sizeof(struct cacheRow) * sets * E); // 2^s * E lines of cache

    cache = cacheLines;

    deques = (deque*)malloc(sizeof(deque) * sets);

    for (int i = 0; i < sets; i++) {
        node* dummyHead = (struct node*)malloc(sizeof(node));
        node* dummyTail = (struct node*)malloc(sizeof(node));
        dummyHead -> offset = -1;
        dummyTail -> offset = -1;

        dummyHead->next = dummyTail;
        dummyTail->prev = dummyHead;

        deques[i].head = dummyHead;
        deques[i].tail = dummyTail;
        deques[i].size = 0;
        deques[i].capacity = E;
    }
}
```

对参数的提取就不多说了，利用`getopt`即可。

然后是对`trace`每一行提取关键的信息，比如操作符`op`和`address`(hex format):

```c
/**
 * @brief The vital info about trace: operation and address.
 */
struct traceLine {
    char op;
    int address;
    // int block;
};

/**
 * @brief Parse the trace line to get op and address.
 * 
 * @param[in] line a line of trace info
 * 
 * @return operation and address
 */
struct traceLine* parseTraces(char* line) {
    if (line[0] != ' ')return NULL;
    char op = line[1]; // 'L' for load, 'M' for modify, 'S' for store
    int address = 0;

    char hexAddr[17];  // 存储十六进制地址，假设地址最多16位
    int i = 3, j = 0;

    // 提取逗号之前的十六进制字符串
    while (line[i] != ',' && line[i] != '\0') {
        hexAddr[j++] = line[i++];
    }
    hexAddr[j] = '\0'; // 确保字符串以 '\0' 结尾

    // 使用 strtol 将十六进制字符串转换为整数
    char* endptr;
    address = (int)strtol(hexAddr, &endptr, 16);

    struct traceLine* res = (struct traceLine*)malloc(sizeof(struct traceLine));
    res -> op = op;
    res -> address = address;
    return res;
}
```

那么最核心的就是模拟`cache`的行为了:

1. 通过`address`的`s`字段获取组号
2. 对该组的每一行检查是否直接`hit` (`valid`且`tag`相同)
3. 如果直接`hit`那么直接返回，如果没有，那么继续
4. 找出空的`cache`行，如果该组内无空行，根据`deque`和LRU算法筛选出一行
5. 对选中的这行进行初始化`valid`和`tag`（加载到`cache`）并将对应的结点加入到`deque`
6. 检查加入到`deque`后是否超出容量(`E`)并修正
7. 需要注意的是如果操作符是`M`则表示`L` + `S`只需要在最后的时候多一次`hit`即可

```c
/**
 * @brief Simulate cache
 * 
 * @param[in] line trace info
 * @param[in] s set bits
 * @param[in] E lines per set
 * @param[in] b block bits
 * @param[in] verbose verbose option
 */
void processCache(struct traceLine* line, int s, int E, int b, int verbose) {
    int address = line->address;
    char op = line->op;
    int index = whichSet(address, s, b);
    int tag = address >> (s + b);
    deque* dq = &deques[index];

    struct cacheRow* setStart = cache + index * E;
    struct cacheRow* victimLine = NULL;
    node* victimNode = NULL;

    // Traverse the deque to find if cache hits
    for (node* n = dq->head->next; n != dq->tail; n = n->next) {
        struct cacheRow* cacheLine = setStart + n->offset;
        if (cacheLine->valid && cacheLine->tag == tag) {
            // Cache hit
            if (op == 'L' || op == 'S') {
                hits++;
                if (verbose) printf("hit\n");
            } else if (op == 'M') {
                hits += 2;
                if (verbose) printf("hit hit\n");
            }
            moveToHead(dq, n);
            return;
        }
    }

    // Cache miss: all the nodes in deque miss
    misses++;
    if (verbose && (op == 'L' || op == 'S')) printf("miss ");
    if (verbose && op == 'M') printf("miss hit");

    // Find an empty line or prepare for eviction
    for (int i = 0; i < E; i++) {
        struct cacheRow* cacheLine = setStart + i;
        if (!cacheLine->valid) {
            victimLine = cacheLine;
            victimNode = NULL;
            break;
        }
    }

    // All lines are valid, so evict the LRU line
    if (!victimLine) {
        evictions++;
        if (verbose) printf("eviction");
        victimNode = removeTail(dq);
        victimLine = setStart + victimNode->offset;
        free(victimNode);
    }

    // Update the victim line with new data
    victimLine->valid = 1;
    victimLine->tag = tag;

    // Add new node to head of deque
    node* newNode = (node*)malloc(sizeof(node));
    newNode->offset = victimLine - setStart;
    addToHead(dq, newNode);

    if (dq->size > dq->capacity) {
        removeTail(dq);
        --dq->size;
    }

    // If operation is 'M', the second store operation is a hit
    if (op == 'M') {
        hits++;
    }
    printf("\n");
}
```

最终运行`./test-csim`来检查结果即可:

```text
                        Your simulator     Reference simulator
Points (s,E,b)    Hits  Misses  Evicts    Hits  Misses  Evicts
     3 (1,1,1)       9       8       6       9       8       6  traces/yi2.trace
     3 (4,2,4)       4       5       2       4       5       2  traces/yi.trace
     3 (2,1,4)       2       3       1       2       3       1  traces/dave.trace
     3 (2,1,3)     167      71      67     167      71      67  traces/trans.trace
     3 (2,2,3)     201      37      29     201      37      29  traces/trans.trace
     3 (2,4,3)     212      26      10     212      26      10  traces/trans.trace
     3 (5,1,5)     231       7       0     231       7       0  traces/trans.trace
     6 (5,1,5)  265189   21775   21743  265189   21775   21743  traces/long.trace
    27
```

27分就是满分, 到这里`part A`结束。

要不是之前写过leetcode的LRU缓存，还真不好写这个part的LRU

不过写完之后对`cache`的LRU机制又加深了理解，刀刻般的。

完整代码查看`csim.c`

---

# Part B

