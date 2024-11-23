/**
 * @author Iamnotphage
 * @note https://iamnotphage.github.io
 */

#include "cachelab.h"
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

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

/**
 * @brief The vital info about trace: operation and address.
 */
struct traceLine {
    char op;
    int address;
    // int block;
};

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

/**
 * @brief Find the index of sets from address.
 * 
 * @param[in] address the address
 * @param[in] s       set bits
 * @param[in] b       block bits
 * 
 * @return index of sets
 */
int whichSet(int address, int s, int b) {
    int mask = (1 << s) - 1;
    return (address >> b) & mask;
}

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

int main(int argc, char *argv[]) {
    int verbose = 0;
    // set, Lines per set, Block
    int s = 0, E = 0, b = 0;
    char *trace_file = NULL;

    int opt; // 用于存储选项字符
    // 使用 getopt 来解析命令行参数
    while ((opt = getopt(argc, argv, "vs:E:b:t:")) != -1) {
        switch (opt) {
            case 'v':
                verbose = 1;
                break;
            case 's':
                s = atoi(optarg); // 将参数转换为整数
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 't':
                trace_file = optarg; // 直接获取文件名
                break;
            default: /* '?' */
                fprintf(stderr, "Usage: %s [-v] -s num -E num -b num -t file\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // allocate how many rows.
    initCache(s, E);

    if (trace_file) {
        // printf("Trace file: %s\n", trace_file);
    } else {
        fprintf(stderr, "Error: No trace file specified.\n");
        exit(EXIT_FAILURE);
    }

    // 打开文件并打印内容
    FILE *file = fopen(trace_file, "r");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // printf("Content of the file '%s':\n", trace_file);
    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        struct traceLine* inputLine = parseTraces(line);
        if (inputLine == NULL) continue;

        if (verbose == 1) {
            line[strlen(line) - 1] = ' '; // process '\n'
            printf("%s", line + 1);
        }

        processCache(inputLine, s, E, b, verbose);

    }
    fclose(file);

    printSummary(hits, misses, evictions);
    return 0;
}

