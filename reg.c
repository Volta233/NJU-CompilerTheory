#include "reg.h"

int total_use_reg = 0;
int INIT_LEN = 20;

char* get_reg_test(const char* name){
    char *c = malloc(sizeof(char) * INIT_LEN);
    sprintf(c, "$%d" , total_use_reg);
    total_use_reg++;
    return c;
}

static char* get_const_val(char* src){
    if (src == NULL) {
        return NULL;
    }

    int len = strlen(src);
    char* const_val = malloc(sizeof(char) * (len + 1));  // +1 for null terminator

    if (const_val == NULL) {
        return NULL;  // Allocation failed
    }

    // Copy string, starting from index 1 to skip the first character
    for (int i = 1; i < len; i++) {
        const_val[i - 1] = src[i];
    }

    const_val[len - 1] = '\0';  // Ensure null termination
    return const_val;
}

void init_regs(){
    int i = 0;
    for(i;i < 32;i++){
        regs[i] = malloc(sizeof(reg_des_t));
        regs[i]->name = i;
        regs[i]->is_free = true;
        regs[i]->content = NULL;
    }
}

void init_spilling_table(){
    spilling_table = malloc(sizeof(spilling_table_t));
    spilling_table->top = 0;
    spilling_num = 0;
    for(int i = 0; i < MAX_LOCATION_LEN; i++){
        spilling_table->stack[i] = NULL;
    }
}

void init_frames(){
    frame = malloc(sizeof(call_frame_t));
    frame->args_num = 0;
    frame->args_head = NULL;
}

void init_DEC_mem(){
    DEC_num = 0;
    for(int i = 0;i<1000;i++){
        DEC_mem[i] = NULL;
    }
}

void free_DEC_mem(){
    for(int i = 0;i < DEC_num;i++){
        free(DEC_mem[i]->name);
        bound_var_t* cur = DEC_mem[i]->bound_var;
        bound_var_t* del;
        while(cur != NULL){
                del = cur;
                free(del->name);
                cur = cur->next;
                free(del);
        }
        free(DEC_mem[i]);
    }
}

void create_DEC_var(char* name, spilling_var_t* loc){
    DEC_mem[DEC_num] = malloc(sizeof(DEC_Union_t));
	DEC_Union_t* d = DEC_mem[DEC_num];
	d->name = strdup(name);
	d->loc = loc;
	d->bound_var = NULL;
    DEC_num++;
}

void binding_var(char* var_name, char* Set_name){
    DEC_Union_t* d;
    bound_var_t* new_bound_var = malloc(sizeof(bound_var_t));
    new_bound_var->name = strdup(var_name);
    new_bound_var->next = NULL;
    for(int i = 0;i< DEC_num;i++){
        d = DEC_mem[i];
        if(strcmp(d->name, Set_name)==0){
            if(d->bound_var == NULL){
                d->bound_var = new_bound_var;
            }else{
                bound_var_t* tail = d->bound_var;
                while(tail->next != NULL){
                    tail = tail->next;
                }
                tail->next = new_bound_var;
            }
            return;
        }
    }
}

spilling_var_t* get_DEC_loc(char* name){
    bound_var_t* find;
    for(int i = 0;i < DEC_num;i++){
        find = DEC_mem[i]->bound_var;
        while(find != NULL){
            if(strcmp(find->name,name)==0){
                return DEC_mem[i]->loc;
            }
            find = find->next;
        }
    }
    return NULL;
}

void free_frames(){
    if(frame == NULL) return;
    spilling_var_t* cur = frame->args_head;
    spilling_var_t* del;
    while(cur != NULL){
                del = cur;
                free(del->name);
                cur = cur->next;
                free(del);
    }
    free(frame);
}

void insert_arg(char* name){
    spilling_var_t* new_arg = malloc(sizeof(spilling_var_t));
    new_arg->name = strdup(name);
    new_arg->offset = frame->args_num * 4;
    new_arg->next = NULL;
    frame->args_num++;
    if(frame->args_head == NULL){
        frame->args_head = new_arg;
    }else{
        spilling_var_t* tail = frame->args_head;
        while(tail->next != NULL){
            tail = tail->next;
        }
        tail->next = new_arg;
    }
}

void free_spilling_table(){
    for(int i = 0; i < MAX_LOCATION_LEN; i++){
        if(spilling_table->stack[i] != NULL){
            spilling_var_t* cur = spilling_table->stack[i];
            spilling_var_t* del;
            while(cur != NULL){
                del = cur;
                free(del->name);
                cur = cur->next;
                free(del);
            }
        }
    }
    free(spilling_table);
}

void free_regs(){
    int i = 0;
    for(i;i < 32;i++){
        free(regs[i]);
    }
}

void lookup_spilling_table(const char* name, FILE* f, int reg){
    // 找一下变量之前的值是否被存储到内存中，如果找到需要先加载到被分配到的寄存器中
    if(name[0] == '#') return;
    if(name[0] == '&'){
        // 取地址，需要加载地址值
        char* name_to_be_found = get_const_val(name);
        spilling_var_t* cur = spilling_table->stack[spilling_table->top];
        while(cur != NULL){
            if(strcmp(cur->name, name_to_be_found)==0){
                // 找到数组变量在表中的那一项， 根据offset 结合$fp获得相应地址装入分配到的寄存器
                fprintf(f,"  addi %s, $fp, -%d\n",trans_for_t_reg(reg), cur->offset);
                break;
            }
        cur = cur->next;
        }
        free(name_to_be_found);
        return;
    }
    spilling_var_t* cur = spilling_table->stack[spilling_table->top];
    while(cur != NULL){
        if(strcmp(cur->name, name)==0){
            // 该变量的值之前存在了内存中，需要装回来
            fprintf(f,"  lw %s, -%d($fp)\n",trans_for_t_reg(reg), cur->offset);
            return;
        }
        cur = cur->next;
    }
}

int* get_spilling_num(){
    return &spilling_num;
}

void set_spilling_num(int n){
    spilling_num = n;
}

spilling_var_t* insert_spilling_array(char* name, int offset){
    spilling_var_t* tail = spilling_table->stack[spilling_table->top];
    if(tail == NULL){
        tail = malloc(sizeof(spilling_var_t));
        tail->name = strdup(name);
        tail->offset = offset;
        tail->next = NULL;
        spilling_table->stack[spilling_table->top] = tail;
        spilling_num += offset / 4;
        return tail;
    }else{
        while(tail->next != NULL){
            tail = tail->next;
        }
        spilling_var_t* new_node = malloc(sizeof(spilling_var_t));
        new_node->name = strdup(name);
        spilling_num += offset / 4;
        new_node->offset = spilling_num * 4;
        new_node->next = NULL;
        tail->next = new_node;
        return new_node;
    }
}

void spilling(var_des_t* v, FILE* f, int lineno){
    // 溢出操作，将变量溢出到栈中
    // 首先需要在spilling table中记录一下，后续生成指令时需要找到存在内存的哪个位置
    spilling_var_t* cur = spilling_table->stack[spilling_table->top];
    // 每次push进去东西都要增加spilling_table中所有变量的offset
    while(cur != NULL){
        if(strcmp(cur->name, v->name)==0){
            // 该函数处已经压入过该变量，再次压入
            fprintf(f,"  sw %s, -%d($fp)\n",get_reg(cur->name,lineno,f), cur->offset);
            v->is_dirty = false;
            return;
        }
        cur = cur->next;
    }
    // 之前并没有压入
    spilling_var_t* tail = spilling_table->stack[spilling_table->top];
    if(tail == NULL){
        char* reg = get_reg(v->name,lineno,f);
        tail = malloc(sizeof(spilling_var_t));
        tail->name = strdup(v->name);
        tail->offset = 4;
        tail->next = NULL;
        cur = tail;
        spilling_table->stack[spilling_table->top] = cur;
        spilling_num++;
        v->is_dirty = false;
        // fprintf(f,"subu $sp, $sp, 4\n");
        fprintf(f,"  sw %s, -%d($fp)\n",reg, tail->offset);
    }else{
        while(tail->next != NULL){
            tail = tail->next;
        }
        char* reg = get_reg(v->name,lineno,f);
        spilling_var_t* new_node = malloc(sizeof(spilling_var_t));
        new_node->name = strdup(v->name);
        spilling_num++;
        new_node->offset = spilling_num * 4;
        new_node->next = NULL;
        tail->next = new_node;
        cur = new_node;
        v->is_dirty = false;
        // fprintf(f,"subu $sp, $sp, 4\n");
        fprintf(f,"  sw %s, -%d($fp)\n",reg, new_node->offset);
    }

}

void set_reg_free(char* name){
    int reg = trans_for_t_reg_T(name);
    if(regs[reg]->is_free == false){
        regs[reg]->is_free = true;
    }
}

void set_regs_free(){
    for(int i = _t0;i <= _t7;i++){
        regs[i]->is_free = true;
        regs[i]->content = NULL;

    }
    for(int i = _t8;i <= _t9;i++){
        regs[i]->is_free = true;
        regs[i]->content = NULL;
    }
    for(int i = _a0;i <= _a3;i++){
        regs[i]->is_free = true;
        regs[i]->content = NULL;
    }
}


void trigger_when_leave_bb(FILE* f, int lineno){
    var_des_t* c = cur_bb->table->table_head;
    while(c != NULL){
        if(c->is_dirty){ //被修改过的变量需要写回内存
            spilling(c,f,lineno);
            c->is_dirty = false;
        }
        c = c->next;
    }
    set_regs_free();
    int offset = spilling_num * 4;
    fprintf(f,"  addi $s1, $fp, -%d \n", offset); 
    fprintf(f,"  slt $s3, $s1, $s2 \n");
    fprintf(f,"  movn $s2, $s1, $s3 \n");
}

void set_dirty_for_var(char* name){
    int reg = trans_for_t_reg_T(name);
    var_des_t* c =  regs[reg]->content;
    c->is_dirty = true;
}

int already_in_t_reg(const char* var_name){
    // 看看参数，其实并没有用到
    // for(int i = _a0;i <= _a3;i++){
    //     var_des_t* c = regs[i]->content;
    //     if(c == NULL) continue;
    //     if(strcmp(c->name,var_name)==0){
    //             return i;
    //     }
    // }
    for(int i = _t0;i <= _t7;i++){
        var_des_t* c = regs[i]->content;
        if(c == NULL) continue;
        if(strcmp(c->name,var_name)==0){
                return i;
        }
    }
    for(int i = _t8;i <= _t9;i++){
        var_des_t* c = regs[i]->content;
        if(c == NULL) continue;
        if(strcmp(c->name,var_name)==0){
                return i;
        }
    }
    return -1;
}

int trans_for_t_reg_T(char* name){
    int ret;
    if(strcmp(name,t0)==0) ret = _t0;
    else if(strcmp(name,t1)==0) ret = _t1;
    else if(strcmp(name,t2)==0) ret = _t2;
    else if(strcmp(name,t3)==0) ret = _t3;
    else if(strcmp(name,t4)==0) ret = _t4;
    else if(strcmp(name,t5)==0) ret = _t5;
    else if(strcmp(name,t6)==0) ret = _t6;
    else if(strcmp(name,t7)==0) ret = _t7;
    else if(strcmp(name,t8)==0) ret = _t8;
    else if(strcmp(name,t9)==0) ret = _t9;
    else if(strcmp(name,a0)==0) ret = _a0;
    else if(strcmp(name,a1)==0) ret = _a1;
    else if(strcmp(name,a2)==0) ret = _a2;
    else if(strcmp(name,a3)==0) ret = _a3;
    else if(strcmp(name,fp)==0) ret = _fp;
    return ret;
}

char* trans_for_t_reg(int reg){
    char* res;
    switch(reg){
        case _t0 : {res = t0; break;}
        case _t1 : {res = t1; break;}
        case _t2 : {res = t2; break;}
        case _t3 : {res = t3; break;}
        case _t4 : {res = t4; break;}
        case _t5 : {res = t5; break;}
        case _t6 : {res = t6; break;}
        case _t7 : {res = t7; break;}
        case _t8 : {res = t8; break;}
        case _t9 : {res = t9; break;}
        case _a0 : {res = a0; break;}
        case _a1 : {res = a1; break;}
        case _a2 : {res = a2; break;}
        case _a3 : {res = a3; break;}
        case _fp : {res = fp; break;}
        default : {res = error; break;}
    }
    return res;
}

int get_distance(int lineno, var_des_t* v){
    appear_history_t* history = v->history;
    int next_line;
    while(history != NULL){
        next_line = history->appear_lineno;
        if(next_line >= lineno){
            return next_line - lineno;
        }
        history = history->next;
    }
    return MAX_DISTANCE;
}

int replacer(int lineno){
    int max_distance = -1;
    int ret = _t0;
    var_des_t* cur_var;
    for(int i = _t0; i <= _t7 ;i++){
        // 假设每个寄存器只放一个变量描述符, 不需要对next进行遍历，因为存的是原基本块的变量表中的某一表项，后续遍历会产生错误
        if(regs[i]->is_free == false && regs[i]->content != NULL){
            cur_var = regs[i]->content;
            int dis = get_distance(lineno,cur_var);
            if(dis > max_distance){
                max_distance = dis;
                ret = i;
            }
        }
    }
    for(int i = _t8; i <= _t9 ;i++){
        // 假设每个寄存器只放一个变量描述符, 不需要对next进行遍历，因为存的是原基本块的变量表中的某一表项，后续遍历会产生错误
        if(regs[i]->is_free == false && regs[i]->content != NULL){
            cur_var = regs[i]->content;
            int dis = get_distance(lineno,cur_var);
            if(dis > max_distance){
                max_distance = dis;
                ret = i;
            }
        }
    }
    return ret;
}

int allocate(const char* var_name, int lineno, FILE* f){
    int i;
    for(i = _t0; i <= _t7 ;i++){
        if(regs[i]->is_free){
            regs[i]->is_free = false;
            if(regs[i]->content != NULL && regs[i]->content->is_dirty){
                spilling(regs[i]->content, f, lineno);
            }
            regs[i]->content = find_var(var_name, cur_bb);
            return i;
        }
    }
    for(i = _t8;i <= _t9;i++){
       if(regs[i]->is_free){
            regs[i]->is_free = false;
            if(regs[i]->content != NULL && regs[i]->content->is_dirty){
                spilling(regs[i]->content, f, lineno);
            }
            regs[i]->content = find_var(var_name, cur_bb);
            return i;
        }
    }
    // printf("allocate reg %d for %s\n", i, var_name);
    //所有寄存器均不空闲，需要进行替换策略
    int replaced_reg = replacer(lineno);
    spilling(regs[replaced_reg]->content, f, lineno);
    regs[replaced_reg]->content = find_var(var_name,cur_bb);
    return replaced_reg; 
}

char* get_reg(const char* var_name, int lineno, FILE* f){ //实质是框架里的ensure函数
    int try = already_in_t_reg(var_name);
    if(try != -1){
        // printf("find %s in reg %d\n", var_name, try);
        regs[try]->is_free = false;
        return trans_for_t_reg(try);
    }else{
        try = allocate(var_name, lineno, f);
        // printf("allocate reg %d for %s\n", try, var_name);
        lookup_spilling_table(var_name,f,try);
        return trans_for_t_reg(try);
    }
}