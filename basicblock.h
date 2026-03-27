#ifndef __BB_H__
#define __BB_H__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hir.h"
#include "ir_gen.h"

typedef struct bb_s bb_t;
typedef struct bb_port_s bb_port_t;
typedef struct var_des_s var_des_t;
typedef struct appear_history_s appear_history_t;
typedef struct var_table_s var_table_t;

struct bb_port_s{   //基本块出入口
    ir_t* ir_node;   //对应行的ir
    int lineno;   //对应行的行号
};

struct bb_s{    //基本块
    int order_no;   //编号
    bb_port_t entry;  //顺序入口，这里没考虑流图，只有顺序划分
    bb_port_t exit;   //顺序出口
    var_table_t* table;   //变量表
    bb_t* next;
};

struct appear_history_s{  
    int appear_lineno;   //出现的行号
    appear_history_t* next;   //下一次出现
};

struct var_des_s{   //变量描述符
    char* name;  //名字
    bool is_dirty; //脏位，检查该变量是否被修改过
    appear_history_t* history;   //出现历史
    var_des_t* next; //查找下一项
};

struct var_table_s{
    int size;       // 当前元素个数，可以辅助遍历链表
    var_des_t* table_head;   // 指向基本块中变量表的头指针,虽然顺序查找很耗费时间
};

bb_t* BBs_entry;  //整个程序第一个基本块的入口指针

char* strdup(const char *);
void divide_bb(syntax_tree_visitor_t *visitor);
void construct_var_table();
void free_bbs();
void free_var_des(var_des_t* target);
var_des_t* find_var(const char* name, bb_t* cur_bb);
bool is_needed(char* name, int lineno, bb_t* cur_bb);

#endif