#ifndef __SIGNTABLE_H__
#define __SIGNTABLE_H__
#include <stdio.h>
#include <string.h>
#include "node.h"
#include <stdbool.h>

#define MAX_STACK_SIZE 100

#define INITIAL_CAPACITY 8   // 初始哈希表容量
#define LOAD_FACTOR 0.75     // 负载因子

#define TYPE_INT 0
#define TYPE_FLOAT 1

char* strdup(const char *);

//类型表示
typedef struct Type_* Type; 
typedef struct FieldList_* FieldList; 
typedef struct node_st node_st;

typedef struct node_st
{
    char* key;  //选用变量或函数的name作为key
    int tot; //用来从函数形参中提取出来顺序列表,同时也是表项加入的时间戳
    bool defined;
    char* value; //就是语法树节点中的info
    int line;
    Type type;
    node_st* next;
} node_st;

// 定义哈希表结构
typedef struct HashTable {
    int capacity;   // 当前容量
    int size;       // 当前元素个数
    char* domain;   // 当前散列表代表的域
    node_st** table;   // 指向节点数组的指针
    Type retVal;   //如果是函数记录一下返回值
} HashTable;

struct Type_ 
{ 
    enum { BASIC, ARRAY, STRUCTURE,FUNCTION } kind; 
    union 
    { 
      // 基本类型 
      int basic; 
      // 数组类型信息包括元素类型与数组大小构成 
      struct { Type elem; int size; } array; 
      // 结构体类型信息是一个链表，其实就是一个新的局部符号表
      HashTable* local_sign_table;//结构体成员集合
    } u; 
    Type retVal;
}; 

struct FieldList_ 
{ 
    char* name;  // 域的名字 
    Type type;  // 域的类型 
    FieldList tail;  // 下一个域 
 }; 
//存放不同域的符号表的栈
typedef struct {
    int top;
    HashTable* my_stack[MAX_STACK_SIZE];
} stack_st;

extern stack_st sign_table;
void init_stack(stack_st *stack) ;
bool is_empty(stack_st *stack) ;
void push(stack_st *stack, HashTable* ht) ;
HashTable* pop(stack_st *stack);
HashTable* getTop(stack_st *stack);

// 动态扩展哈希表
void resize_table(HashTable* table);
// 释放哈希表
void free_table(HashTable* table);
void free_sign_table();
void freeType(Type type) ;
void freeNode(node_st *node);
// 查找元素
node_st* get(HashTable* table, const char* key);
// 插入元素
void insert(HashTable* table, const char* key, Type type,int line);
// 创建新的哈希表
HashTable* create_table(int capacity,const char* domain);
// 创建新的节点
node_st* create_node_st(const char* key,Type type,int line) ;
// 哈希函数，基于字符串的ASCII值
unsigned int hash_function(const char* key, int capacity);
//初始化符号表
void init_sign_table();
//先序遍历整棵语法分析树
void dfs(node_t* node);
int handleArgs(node_t* node,node_st* Args[],int index);
void handleCompSt(node_t* node);
void handleDef(node_t* node);
node_st* handleExp(node_t* node);
void handleExtDef(node_t* node);
void handleExtDecList(Type t,node_t* node);
void handleFunDec(Type t,node_t* node);
void handleSpecifier(Type t,node_t* node);
void handleStructSpecifier(Type t,node_t* node);
void handleDefList(node_t* node);
void handleDecList(Type t,node_t* node);
void handleDec(Type t,node_t* node);
void handleVarDec(Type t,node_t* node);
void handleVarList(node_t* node);
void handleParamDec(node_t* node);
int handleAssignopLeft(node_t* node);
int checkAssignopType(Type t1,Type t2);
int checkOperatorType(Type t1,Type t2);
int checkLogicType(Type t1,Type t2);
int checkEqualType(Type t1,Type t2);
int checkArgs(node_st* Params[],node_st* Args[],int len);
//从符号表中查找符号类型
void my_error(int type,int line,const char *msg) ;
node_st* lookup(const char* key,int type,int line);
char* append_string(char* original, const char* to_append) ;
char* formString(node_st* Args[],int len,char* init);
#endif
