#include "basicblock.h"

int bb_no = 0;

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


void create_new_bb(bb_t* new_bb, int lineno, ir_t* ir){
        new_bb->order_no = bb_no;
        new_bb->entry.ir_node = ir->next;
        new_bb->entry.lineno = lineno + 1;
        new_bb->table = malloc(sizeof(var_table_t));
        new_bb->table->size = 0;
        new_bb->table->table_head = NULL;
        new_bb->next = NULL;
}

void divide_judge(ir_t* ir, int lineno, bb_t* tail){
    if(ir->next == NULL){ //到达最后一句，进行收尾
        tail->exit.ir_node = ir;
        tail->exit.lineno = lineno;
        tail->next = NULL;
    }else if(ir->next->ir_type == IR_LABEL || ir->next->ir_type == IR_FUNCTION){ //下一个是标签，相当于是另一个新的基本块的开头
        tail->exit.ir_node = ir;
        tail->exit.lineno = lineno;
        bb_t* new_bb = malloc(sizeof(bb_t));
        tail->next = new_bb;
        bb_no++;
        create_new_bb(new_bb, lineno, ir);
    }else if(ir->ir_type == IR_GOTO || ir->ir_type == IR_IFTHENGOTO){  //本句是转移语句，是一个BB的最后一句
        tail->exit.ir_node = ir;
        tail->exit.lineno = lineno;
        bb_t* new_bb = malloc(sizeof(bb_t));
        tail->next = new_bb;
        bb_no++;
        create_new_bb(new_bb, lineno, ir);
    }else if(ir->next->ir_type == IR_CALL || ir->ir_type == IR_CALL){  //将CALL作为一个单独的基本块处理
        tail->exit.ir_node = ir;
        tail->exit.lineno = lineno;
        bb_t* new_bb = malloc(sizeof(bb_t));
        tail->next = new_bb;
        bb_no++;
        create_new_bb(new_bb, lineno, ir);
    }
}

void divide_bb(syntax_tree_visitor_t *visitor){  // 进行基本块的划分
    int lineno = 0;
    ir_t *cur = visitor->code;
    BBs_entry = malloc(sizeof(bb_t));
    bb_t* tail = BBs_entry;
    tail->order_no = bb_no;
    tail->entry.ir_node = cur;
    tail->entry.lineno = 0;
    tail->table = malloc(sizeof(var_table_t));
    tail->table->size = 0;
    tail->table->table_head = NULL;
    tail->next = NULL;
    while (cur != NULL) {
        while(tail->next != NULL){
            tail = tail->next;
        }
        divide_judge(cur,lineno,tail);
        cur = cur->next;
        lineno++;
    }
}

void free_history(appear_history_t* history){
    appear_history_t* cur = history;
    appear_history_t* del;
    while(cur != NULL){
        del = cur;
        cur = cur->next;
        free(del);
    }
}

void free_var_des(var_des_t* target){
    var_des_t* cur = target;
    var_des_t* del;
    while(cur != NULL){
        del = cur;
        free(del->name);
        free_history(del->history);
        cur = cur->next;
        free(del);
    }
}

void free_var_table(var_table_t* table){
    free_var_des(table->table_head);
    free(table);
}

void free_bbs(){
    bb_t* cur = BBs_entry;
    bb_t* del;
    while(cur != NULL){
        del = cur;
        free_var_table(cur->table);
        cur = cur->next;
        free(del);
    }
}

void create_new_var(char* key, int lineno, var_des_t* new_var){
    new_var->name = strdup(key);
    new_var->is_dirty = false;
    new_var->history = malloc(sizeof(appear_history_t));
    new_var->history->appear_lineno = lineno;
    new_var->history->next = NULL;
    new_var->next = NULL;
}

void insert_new_var(var_des_t* new_var, var_table_t* table){
    var_des_t* tail = table->table_head;
    while(tail->next != NULL){
        tail = tail->next;
    }
    tail->next = new_var;
}

void add_history(var_des_t* target_var, int new_history){
    appear_history_t* tail = target_var->history;
    while(tail->next != NULL){
        tail = tail->next;
    }
    appear_history_t* new_his = malloc(sizeof(appear_history_t));
    new_his->appear_lineno = new_history;
    new_his->next = NULL;
    tail->next = new_his;
}

var_des_t* find_var(const char* name, bb_t* cur_bb){
    if(name[0] == '#') return NULL;
    var_table_t* table = cur_bb->table;
    var_des_t* res = table->table_head;
    while(res != NULL){
        if(strcmp(res->name,name)==0){
            break;
        }
        res = res->next;
    }
    return res;
}

bool is_needed(char* name, int lineno, bb_t* cur_bb){
    var_des_t* v = find_var(name,cur_bb);
    if(v == NULL){
        // printf("%s is not find in cur_bb %d", name, cur_bb->order_no);
        return false;
    }
    appear_history_t* history = v->history;
    while(history != NULL){
        if(history->appear_lineno > lineno){
            return true;
        }
        history = history->next;
    }
    return false;
}

void lookup_var_table(char* key, int lineno, var_table_t* table){
    if(key[0] == '#') return;
    if(table->size == 0){  //对第一个元素的插入特殊一点
        table->table_head = malloc(sizeof(var_des_t));
        var_des_t* new_var = table->table_head;
        create_new_var(key, lineno, new_var);
        table->size++;
    }else{
        var_des_t* lookup_var = table->table_head;
        while(lookup_var != NULL){
            if(strcmp(lookup_var->name,key)==0){
                break;
            }
            lookup_var = lookup_var->next;
        }
        if(lookup_var == NULL){ //未查找到对应变量
            var_des_t* new_var = malloc(sizeof(var_des_t));
            create_new_var(key, lineno, new_var);
            insert_new_var(new_var,table);
            table->size++;
        }else{  //找到了对应的变量
            add_history(lookup_var, lineno);
        }
    }
}

void init_bb_var_table(bb_t* bb){
    int start = bb->entry.lineno;
    int end = bb->exit.lineno;
    ir_t* ir = bb->entry.ir_node;
    for(int i = start;i <= end;i++){ //顺序遍历基本块内部所有变量
        // 只要需要reg的都算是变量，相当于试运行一遍write_code
        if(ir == NULL){
            // printf("when ir == NULL , i == %d and end is %d.\n", i , end);
        }
        switch(ir->ir_type){
        case IR_ASSIGN:{
            if(ir->src_names[0][0] == '#'  && ir->dest_name[0] != '*'){
                char* var1 = ir->dest_name;
                lookup_var_table(var1,i,bb->table);
            }else if(ir->src_names[0][0] == '*'){
                char* ptr_val = get_const_val(ir->src_names[0]);
                char* var1 = ptr_val;
                char* var2 = ir->dest_name;
                lookup_var_table(var1,i,bb->table);
                lookup_var_table(var2,i,bb->table);
                free(ptr_val);
            }else if(ir->dest_name[0] == '*'){
                char* ptr_val = get_const_val(ir->dest_name);
                char* var1 = ptr_val;
                lookup_var_table(var1,i,bb->table);
                if(ir->src_names[0][0] != '#') {
                    char* var2 = ir->src_names[0];
                    lookup_var_table(var2,i,bb->table);
                }
                free(ptr_val);
            }else{
                char* var1 = ir->dest_name;
                char* var2 = ir->src_names[0];
                lookup_var_table(var1,i,bb->table);
                lookup_var_table(var2,i,bb->table);
            }
            break;
        }
        case IR_PLUS:{
            char* var1 = ir->dest_name;
            char* var2 = NULL;
            char* var3 = NULL;
            if(ir->src_names[0][0] != '#'){
                var2 = ir->src_names[0];
            }
            if(ir->src_names[1][0] != '#'){
                var3 = ir->src_names[1];
            }
            lookup_var_table(var1,i,bb->table);
            if(var2 != NULL){
                lookup_var_table(var2,i,bb->table);
            }
            if(var3 != NULL){
                lookup_var_table(var3,i,bb->table);
            }
            break;
        }
        case IR_SUB:{
            char* var1 = ir->dest_name;
            char* var2 = NULL;
            char* var3 = NULL;
            if(ir->src_names[0][0] != '#'){
                var2 = ir->src_names[0];
            }
            if(ir->src_names[1][0] != '#'){
                var3 = ir->src_names[1];
            }
            lookup_var_table(var1,i,bb->table);
            if(var2 != NULL){
                lookup_var_table(var2,i,bb->table);
            }
            if(var3 != NULL){
                lookup_var_table(var3,i,bb->table);
            }
            break;
        }
        case IR_MULTI:{
            char* var1 = ir->dest_name;
            char* var2 = NULL;
            char* var3 = NULL;
            if(ir->src_names[0][0] != '#'){
                var2 = ir->src_names[0];
            }
            if(ir->src_names[1][0] != '#'){
                var3 = ir->src_names[1];
            }
            lookup_var_table(var1,i,bb->table);
            if(var2 != NULL){
                lookup_var_table(var2,i,bb->table);
            }
            if(var3 != NULL){
                lookup_var_table(var3,i,bb->table);
            }
            break;
        }
        case IR_DIV:{
            char* var1 = ir->dest_name;
            char* var2 = ir->src_names[0];
            char* var3 = ir->src_names[1];
            lookup_var_table(var1,i,bb->table);
            lookup_var_table(var2,i,bb->table);
            lookup_var_table(var3,i,bb->table);
            break;
        }
        case IR_IFTHENGOTO:{
            char* var1 = ir->src_names[0];
            char* var2 = ir->src_names[2];
            lookup_var_table(var1,i,bb->table);
            lookup_var_table(var2,i,bb->table);
            break;
        }
        case IR_RETURN:{
            char* var1 = ir->src_names[0];
            lookup_var_table(var1,i,bb->table);
            break;
        }
        case IR_ARG:{
            char* var1 = ir->src_names[0];
            lookup_var_table(var1,i,bb->table);
            break;
        }
        case IR_CALL:{
            if (!ir->dest_name) {
                ;
            } else {
                char* var1 = ir->dest_name;
                lookup_var_table(var1,i,bb->table);
            }
            break;
        }
        case IR_READ:{
            char* var1 = ir->dest_name;
            lookup_var_table(var1,i,bb->table);
            break;
        }
        case IR_WRITE:{
            char* var1 = ir->src_names[0];
            lookup_var_table(var1,i,bb->table);
            break;
        }
        case IR_FUNCTION:{
            break;
        }
        case IR_PARAM:{
            char* var1 = ir->src_names[0];
            lookup_var_table(var1,i,bb->table);
            break;
        }
        case IR_DEC:{
            char* var1 = ir->src_names[0];
            lookup_var_table(var1,i,bb->table);
            break;
        }
        default : {break;}
        }
        ir = ir->next;
    }
}


void construct_var_table(){
    bb_t* cur = BBs_entry;
    while(cur != NULL){
        init_bb_var_table(cur);
        cur = cur->next;
    }
}