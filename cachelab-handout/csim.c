#include "cachelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#define ADDR_OFFSET 3

#define MISS 0
#define HIT 1
#define MISS_EVICTION 2

int show_help = 0;
int show_info = 0;
int s = 0;
int E = 0;
int b = 0;
int num_lines = 0;

int hits = 0;
int misses = 0;
int evictions = 0;

char* trace_name;
char ** trace;

struct Group *groups;

struct Node * create_node(int id, int flag) {
    struct Node* node = (struct Node *)malloc(sizeof(struct Node *));
    node -> id = id;
    node -> flag = flag;
    node ->next = node ->prev = NULL;
    return node;
}

void insert_node(struct Node *node, struct Group *group){
    if (group->head == NULL){
        group->head = node;
        group->tail = node;
        node ->next = NULL;
        node ->prev = NULL;
    }
    else {
        node->next = group->head;
        node->prev = NULL;
        group->head->prev = node;
        group->head = node;
    }
    (group -> num) ++;

}

void move_to_head(struct Node *node, struct Group *group) {
    if (node == group->head)
        return;
    if (node == group->tail) {
        node -> prev -> next = NULL;
        group -> tail = node->prev;
    }
    else {
        node -> prev -> next = node -> next;
        node -> next -> prev = node -> prev;
    }
        node->next = group->head;
        node->prev = NULL;
        group->head->prev = node;
        group->head = node;
    
}

void replace_node(struct Node *node, struct Group *group) {
    if (group -> head == NULL)
        return;
    insert_node(node, group);
    struct Node* pre_tail = group->tail; 
    pre_tail -> prev -> next = NULL;
    group->tail = pre_tail ->prev;
    free(pre_tail);
    group->num --;
}

void show_args () {
    printf("s= %d  E = %d b = %d trace_name = %s\n", s, E, b ,trace_name);
}

int get_args(int argc, char **argv){
    int opt;

    while((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
        switch(opt) {
            case 'h':
                show_help = 1;
                break;
            case 'v':
                show_info = 1;
                break;
            case 's':
                s = atoi(optarg);
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 't':
                trace_name = optarg;
                break;
            default:
                printf("Invalid option\n");
                return -1;
        }
    }
    show_args();
    return 0;
}
void help_msg () {

}

void info_msg (int result,char *ins) {
    if (!show_info)
        return;
    printf("%s", ins+1);
    if (result == MISS) {
        printf(" miss\n");
    }
    else if (result == MISS_EVICTION) {
        printf(" miss eviction\n");
    }
    else if (result == HIT) {
        printf(" hit\n");
    }
}

unsigned long hexstr_to_ulong(char* str, int len) {
    unsigned long result = 0;
    for (int i = 0; i < len; i++) {
        if (isxdigit(str[i])) {
            if (isdigit(str[i])) {
                result = (result << 4) + (str[i] - '0');
            } 
            else {
                result = (result << 4) + (str[i] - 'a' + 10);
            }
        } else {
            printf("Invalid hex string\n");
            return 0;
        }
    }
    return result;
}

void show_traces() {
    for (int i = 0; i < num_lines; i++) {
        printf("%s\n",trace[i]);
    }
}

int load_traces(char *filename) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        printf("Failed to open %s\n", filename);
        return -1;
    }

    // 统计文件行数
    char line[100];
    while (fgets(line, 100, file) != NULL) {
        num_lines++;
    }
    rewind(file);

    // 创建二维char *数组
    trace = (char**)malloc((num_lines) * sizeof(char*));
    for (int i = 0; i < num_lines; i++) {
        (trace)[i] = (char*)malloc(100 * sizeof(char));
    }

    // 读取文件内容到二维char*数组
    int line_num = 0;
    while (fgets((trace)[line_num], 100, file) != NULL) {
        // 删除换行符
        (trace)[line_num][strlen((trace)[line_num]) - 1] = '\0';
        line_num++;
    }

    fclose(file);
    //show_traces();
    return 0;
}

int parse_index(unsigned long addr) {
    return (addr >> b) & ((1 << s) - 1);
}

int parse_id(unsigned long addr) {
    return addr >> (s+b);
}

unsigned long parse_addr(char *ins) {
    int addr_len = strlen(ins) - ADDR_OFFSET - 2;
    return hexstr_to_ulong(ins+ADDR_OFFSET, addr_len);
}

int search_group(int index, int id) {
    struct Group* group = &groups[index];
    int hit = 0;
    int rtn = MISS;
    struct Node *cur = group->head;
    while (cur != NULL) {
        if (cur->id == id){
            // cache hit
            hit = 1;
            hits++;
            move_to_head(cur, group);
            rtn = HIT;
            break;
        }
        cur = cur ->next;
    }
    if (hit == 0) {
        //cache miss
        misses++;
        struct Node *node = create_node(id, 1);
        if (group->num < E) {
            // cold miss
            insert_node(node, group);
            rtn = MISS;
        }
        else {
            // eviction miss
            evictions++;
            replace_node(node, group);
            rtn = MISS_EVICTION;
        }
    }
    return rtn;
}

void load_data(char *ins) {
    unsigned addr = parse_addr(ins);
    int index = parse_index(addr);
    int id = parse_id(addr);
    printf("%s  index = %d\n",ins, index);
    int rtn = search_group(index, id);
    info_msg(rtn, ins);
}

void store_data(char *ins) {
    load_data(ins);
}

void modify_data(char *ins) {
    load_data(ins);
    store_data(ins);
}

int ins_exec(char *ins) {
    char instruction = ins[1];
    switch (instruction) {
        case 'L': 
            load_data(ins);
            break;
        case 'S':
            store_data(ins);
            break;
        case 'M': 
            modify_data(ins);
            break;
        case ' ':
            break;
        default :
            printf("undefined instruction\n");
            return -1;
    }
    return 0;
}


int parse_traces() {
    for (int i = 0; i < num_lines; i++) {
        ins_exec(trace[i]);
    }
    return 0;
}


int init_cache() {
    int group_nums = pow(2,s);
    /*Allocate groups*/
    groups = (struct Group*)malloc(sizeof(struct Group)*group_nums);
    if (groups == NULL)
        return -1;
    for (int i = 0; i < group_nums; i++) {
        groups[i].head = NULL;
        groups[i].tail = NULL;
        groups[i].num = 0;
    }
    return 0;
}




int main(int argc, char **argv)
{
    get_args(argc, argv);
    load_traces(trace_name);
    init_cache();
    parse_traces();

    printSummary(hits, misses, evictions);

    return 0;
}
