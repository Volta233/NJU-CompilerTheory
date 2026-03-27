#ifndef __REG_H__
#define __REG_H__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "basicblock.h"

char* get_reg_test(const char* name);

#define MAX_LOCATION_LEN 1000000
#define MAX_DISTANCE 999999999

#define _zero 0
#define _at 1
#define _v0 2
#define _v1 3
#define _a0 4
#define _a1 5
#define _a2 6
#define _a3 7
#define _t0 8
#define _t1 9
#define _t2 10
#define _t3 11
#define _t4 12
#define _t5 13
#define _t6 14
#define _t7 15
#define _s0 16
#define _s1 17
#define _s2 18
#define _s3 19
#define _s4 20
#define _s5 21
#define _s6 22
#define _s7 23
#define _t8 24
#define _t9 25
#define _k0 26
#define _k1 27
#define _gp 28
#define _sp 29
#define _fp 30
#define _ra 31

static char* t0 = "$t0";
static char* t1 = "$t1";
static char* t2 = "$t2";
static char* t3 = "$t3";
static char* t4 = "$t4";
static char* t5 = "$t5";
static char* t6 = "$t6";
static char* t7 = "$t7";
static char* t8 = "$t8";
static char* t9 = "$t9";
static char* a0 = "$a0";
static char* a1 = "$a1";
static char* a2 = "$a2";
static char* a3 = "$a3";
static char* fp = "$fp";
static char* error = "Error reg";

typedef struct reg_des_s reg_des_t;
typedef struct spilling_var_s spilling_var_t;
typedef struct spilling_table_s spilling_table_t;
typedef struct call_frame_s call_frame_t;
typedef struct bound_var_s bound_var_t;
typedef struct DEC_Union_s DEC_Union_t;

struct reg_des_s{
    int name; //用上面的define编号
    bool is_free; //是否空闲
    var_des_t* content; //存放变量描述符的链表，表示该寄存器内存放的是哪些变量
};

struct spilling_var_s{
    char* name;
    int offset;
    spilling_var_t* next;
};

struct spilling_table_s{
    int top;
    spilling_var_t* stack[MAX_LOCATION_LEN];
};

struct call_frame_s{
    spilling_var_t* args_head; //参数的offset 是相对于$fp往上抽取的
    int args_num;
};

struct bound_var_s{
    char* name;
    bound_var_t* next;
};

struct DEC_Union_s{
    char* name;	
    spilling_var_t* loc;
    bound_var_t* bound_var;
};

reg_des_t* regs[32];
DEC_Union_t* DEC_mem[1000];
spilling_table_t* spilling_table;
static int spilling_num;
static int DEC_num ;
bb_t* cur_bb;
call_frame_t* frame;

void init_regs();
void init_spilling_table();
void init_frames();
void init_DEC_mem();
void free_regs();
void free_DEC_mem();
void free_frames();
void free_spilling_table();
void insert_arg(char* name);
void create_DEC_var(char* name, spilling_var_t* loc);
void binding_var(char* var_name, char* Set_name);
spilling_var_t* get_DEC_loc(char* name);
spilling_var_t* insert_spilling_array(char* name, int offset);
char* get_reg(const char* var_name, int lineno, FILE* f);
void trigger_when_leave_bb(FILE* f, int lineno);
int allocate(const char* var_name, int lineno, FILE* f);
char* trans_for_t_reg(int reg);
int trans_for_t_reg_T(char* name);
void set_reg_free(char* name);
void set_regs_free();
void set_dirty_for_var(char* name);
int* get_spilling_num();
void set_spilling_num(int n);
#endif