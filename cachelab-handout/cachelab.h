/* 
 * cachelab.h - Prototypes for Cache Lab helper functions
 */

#ifndef CACHELAB_TOOLS_H
#define CACHELAB_TOOLS_H

#define MAX_TRANS_FUNCS 100

typedef struct trans_func{
  void (*func_ptr)(int M,int N,int[N][M],int[M][N]);
  char* description;
  char correct;
  unsigned int num_hits;
  unsigned int num_misses;
  unsigned int num_evictions;
} trans_func_t;
struct Node{
    int flag;
    unsigned int id;
    struct Node *next;
    struct Node *prev;
};

struct Group{
    int num;
    struct Node *head;
    struct Node *tail;
};
int parse_index(unsigned long addr);
int parse_id(unsigned long addr);
struct Node * create_node(int id, int flag);
void insert_node(struct Node *node, struct Group *group); // 向组中插入节点
void move_to_head(struct Node *node, struct Group *group); //将节点设置为双向链表的头节点
void replace_node(struct Node *node, struct Group *group); //将node插入到头结点并移除尾节点
void show_args(); // 显示参数的函数声明
int get_args(int argc, char **argv); // 获取命令行参数的函数声明
void help_msg(); // 显示帮助信息的函数声明
void info_msg(int result, char *ins); // 显示信息的函数声明
void load_data(char *ins); // 加载数据的函数声明
void store_data(char *ins); // 存储数据的函数声明
void modify_data(char *ins); // 修改数据的函数声明
unsigned long hexstr_to_ulong(char* str, int len); // 将十六进制字符串转换为长整型数值的函数声明
void show_traces(); // 显示缓存跟踪信息的函数声明
int load_traces(char *filename); // 加载缓存跟踪信息的函数声明
int find_group(char *ins); // 查找所属组的函数声明
int ins_exec(char *ins); // 执行指令的函数声明
int parse_traces(); // 解析缓存跟踪信息的函数声明
int init_cache(); // 初始化缓存的函数声明

/* 
 * printSummary - This function provides a standard way for your cache
 * simulator * to display its final hit and miss statistics
 */ 
void printSummary(int hits,  /* number of  hits */
				  int misses, /* number of misses */
				  int evictions); /* number of evictions */

/* Fill the matrix with data */
void initMatrix(int M, int N, int A[N][M], int B[M][N]);

/* The baseline trans function that produces correct results. */
void correctTrans(int M, int N, int A[N][M], int B[M][N]);

/* Add the given function to the function list */
void registerTransFunction(
    void (*trans)(int M,int N,int[N][M],int[M][N]), char* desc);

#endif /* CACHELAB_TOOLS_H */
