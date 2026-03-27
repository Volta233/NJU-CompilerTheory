#include "sign_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

int magicNumber = 0;
int tot = 0;

void my_error(int type,int line,const char *msg) {
    printf("Error type %d at Line %d: %s.\n", type, line,msg);
}

void init_stack(stack_st *stack) {
    stack->top = -1;
}

bool is_empty(stack_st *stack) {
    return stack->top == -1;
}

void push(stack_st *stack, HashTable* ht) {
    if (stack->top >= MAX_STACK_SIZE - 1) {
        printf("Stack Overflow\n");
        exit(EXIT_FAILURE);  
    }
    stack->my_stack[++(stack->top)] = ht;
}

HashTable* pop(stack_st *stack) {
    if (stack->top < 0) {
        printf("Stack Underflow\n");
        exit(EXIT_FAILURE); 
    }
    return stack->my_stack[(stack->top)--];
}

HashTable* getTop(stack_st *stack){
    return stack->my_stack[(stack->top)];
}

// 哈希函数，基于字符串的ASCII值
unsigned int hash_function(const char* key, int capacity) {
    unsigned long hash = 0;
    while (*key) {
        hash = (hash * 31) + *key++;
    }
    return hash % capacity;
}

// 创建新的节点
node_st* create_node_st(const char* key, Type type,int line) {
    node_st* new_node = (node_st*) malloc(sizeof(node_st));
    new_node->key = strdup(key); // 拷贝字符串
    new_node->type = type;
    new_node->next = NULL;
    new_node->line = line;
    new_node->defined = false; //defined与value需要碰到赋值语句才会更新
    new_node->value = NULL;
    new_node->tot = tot;
    tot++;
    return new_node;
}

// 比较函数，用于根据 node_st 的 tot 字段排序
int compare(const void* a, const void* b) {
    node_st* nodeA = *(node_st**)a;
    node_st* nodeB = *(node_st**)b;
    return (nodeA->tot - nodeB->tot);
}

// 函数：遍历 HashTable 并将所有 node_st 排序后存入静态数组
void traverse_and_sort(HashTable* hashTable, node_st* sortedArray[]) {
    if (hashTable == NULL || sortedArray == NULL) {
        return; // 参数检查
    }

    int index = 0;

    // 遍历哈希表的每个桶
    for (int i = 0; i < hashTable->capacity; i++) {
        node_st* current = hashTable->table[i];

        // 遍历链表中的所有节点
        while (current != NULL) {
            if (index < hashTable->size) {
                sortedArray[index++] = current;
            }
            current = current->next;
        }
    }

    // 对静态数组进行排序
    qsort(sortedArray, index, sizeof(node_st*), compare);
}
// 创建新的哈希表
HashTable* create_table(int capacity,const char* domain) {
    HashTable* table = (HashTable*) malloc(sizeof(HashTable));
    table->capacity = capacity;
    table->size = 0;
    table->domain = strdup(domain);
    table->table = (node_st**) calloc(capacity, sizeof(node_st*)); // 初始化表
    table->retVal = NULL;
    return table;
}

// 插入元素，有定义
void insert(HashTable* table, const char* key, Type type,int line) {
    // 计算负载因子并扩展
    if ((float)table->size / table->capacity >= LOAD_FACTOR) {
        resize_table(table);
    }

    unsigned int index = hash_function(key, table->capacity);
    node_st* head = table->table[index];

    //看看要插入的table是不是结构体的
    char* s = table->domain;
    node_st* nst = lookup(s,100,line);
    // 遍历链表，如果键存在，则说明重复定义
    while (head) {
        if (strcmp(head->key, key) == 0) { //根据Type区分错误类型
            if(nst != NULL && nst->type->kind == STRUCTURE){ //说明要插入的表是一个结构体的内部成员变量
                printf("Error type %d at Line %d: %s\"%s\".\n", 15, line,"Redefined field ",key);
            }else{
                if(type->kind == BASIC){//普通变量
                    if(head->type->kind == BASIC){
                        printf("Error type %d at Line %d: %s\"%s\".\n", 3, line,"Redefined variable ",key);
                        return;
                    }else if(head->type->kind == STRUCTURE){
                        printf("Error type %d at Line %d: %s\"%s\".\n", 3, line,"Redefined variable ",key);//与某个结构体同名
                        return;
                    }else if(head->type->kind == FUNCTION){ //与某个函数匹配到了，假定函数可以和普通变量同名
                        head = head->next;
                        continue;
                    } 
                }else if(type->kind == FUNCTION){
                    if(head->type->kind == FUNCTION){ //函数名重复
                        printf("Error type %d at Line %d: %s\"%s\".\n", 4, line,"Redefined function ",key);
                        return;
                    }
                }else if(type->kind == STRUCTURE){
                    if(head->type->kind == STRUCTURE || head->type->kind == BASIC){
                        printf("Error type %d at Line %d: %s\"%s\".\n", 16, line,"Duplicated name ",key);
                        return;
                    }
                }
            }
        }
        head = head->next;
    }

    // 如果键不存在，插入新节点到链表头部，正常插入符号表元素
    node_st* new_node = create_node_st(key, type,line);
    new_node->next = table->table[index];
    table->table[index] = new_node;
    table->size++;
}

// 扩展哈希表容量
void resize_table(HashTable* table) {
    int old_capacity = table->capacity;
    table->capacity *= 2;  // 容量加倍
    node_st** new_table = (node_st**) calloc(table->capacity, sizeof(node_st*));

    // 重新插入所有旧表中的元素到新表中
    for (int i = 0; i < old_capacity; i++) {
        node_st* head = table->table[i];
        while (head) {
            unsigned int new_index = hash_function(head->key, table->capacity);
            node_st* next_node = head->next;
            
            // 头插法，将节点插入新表
            head->next = new_table[new_index];
            new_table[new_index] = head;

            head = next_node;
        }
    }

    free(table->table);
    table->table = new_table;
}

// 查找单个域元素，返回对应表项node_st
node_st* get(HashTable* table, const char* key) {
    if(table == NULL ) return NULL;
    unsigned int index = hash_function(key, table->capacity);
    node_st* head = table->table[index];

    // 遍历链表寻找键
    while (head) {
        if (strcmp(head->key, key) == 0) {
            return head;
        }
        head = head->next;
    }

    return NULL; // 未找到返回 -1
}

// 释放哈希表
void free_table(HashTable* table) {
    for (int i = 0; i < table->capacity; i++) {
        node_st* head = table->table[i];
        while (head) {
            node_st* temp = head;
            head = head->next;
            free(temp->key); // 释放键字符串
            free(temp);
        }
    }
    free(table->table);
    freeType(table->retVal);
    free(table);
}

void freeType(Type type) {
    if (type == NULL) return;

    // 需要处理 Type 结构体中的不同类型
    switch (type->kind) {
        case BASIC:
            // 对于 BASIC 类型，无需释放额外的内存
            break;
        case ARRAY:
            // 对于 ARRAY 类型，释放其元素类型
            freeType(type->u.array.elem);
            break;
        case STRUCTURE:
            // 结构体类型下可以添加更多的释放逻辑，视具体实现而定
            // 如果结构体中包含动态分配的内存，需要递归释放
            free_table(type->u.local_sign_table);
            break;
        case FUNCTION:
            freeType(type->retVal);
            free_table(type->u.local_sign_table);
            break;
    }

    // 最后释放 Type 结构体本身
    free(type);
}

void freeNode(node_st *node) {
    if (node == NULL) return;

    // 释放 Type 结构体
    freeType(node->type);

    // 释放 value 和 key，假设它们是动态分配的字符串
    if (node->value) free(node->value);
    if (node->key) free(node->key);
    // 递归释放链表中下一个节点
    if (node->next) freeNode(node->next);
    // 最后释放 node_st 本身
    free(node);
}

void free_sign_table(){
    int i = sign_table.top;
    while(i > -1){
        free_table(sign_table.my_stack[i]);
        i--;
    }
}

//全局查找
node_st* lookup(const char* key,int type,int line){
    int i = sign_table.top;
    node_st* retVal = NULL;
    HashTable* ht;
    while(i > -1){
        ht = sign_table.my_stack[i];
        retVal = get(ht,key);
        if(retVal != NULL){  //找到了对应的类型值
            return retVal;
        }
        i--;
    }
    //整个符号表中都没找到
    if(type == 1){
        printf("Error type %d at Line %d: %s\"%s\".\n", type, line,"Undefined variable ",key);
    }else if(type == 2){
        printf("Error type %d at Line %d: %s\"%s\".\n", type, line,"Undefined function ",key);
    }else if(type == 17){
        printf("Error type %d at Line %d: %s\"%s\".\n", type, line,"Undefined structure ",key);
    }
    return retVal;
}

void init_sign_table(){
    HashTable* global = create_table(INITIAL_CAPACITY,"global");
    push(&sign_table,global);
}

void handleVarDec(Type t,node_t* node){
    if(node->child_num == 1){
        insert(getTop(&sign_table),node->childs[0]->info,t,node->childs[0]->line);
    }else{  //数组定义
        Type nt = malloc(sizeof(struct Type_));
        nt->kind = ARRAY;
        nt->u.array.elem = t;
        nt->u.array.size = atoi(node->childs[2]->info);
        handleVarDec(nt,node->childs[0]);
    }
}

void handleDec(Type t,node_t* node){
    if(node->child_num == 1){
        handleVarDec(t,node->childs[0]);
    }else{   //TODO Dec第二个分支，赋值语句，需要判断赋值的Exp与VarDec的定义类型是否一致
        bool flag = false;
        char* key;
        node_t* helper = node->childs[0];
        while(!(strcmp(helper->name,"ID")==0)){
            helper = helper->childs[0];
        }
        key = helper->info; //helper只帮助找到符号表内的位置
        HashTable* ht = getTop(&sign_table);
        if(!(strcmp(ht->domain,"Temp")==0)){
            node_st* onst = lookup(ht->domain,66,node->line);
            if(onst->type->kind == STRUCTURE){
                printf("Error type %d at Line %d: %s\"%s\".\n", 15, node->line,"Redefined field ",key);
                flag = true;
            }
        }
        handleVarDec(t,node->childs[0]);
        node_st* nt = handleExp(node->childs[2]);
        if(nt == NULL ) return;
        int i = checkEqualType(t,nt->type);
        if(i == -1 && flag == false){
            my_error(5,node->line,"Type mismatched for assignment");//15的优先级高于5
        }
    }
}

void handleDecList(Type t,node_t* node){
    handleDec(t,node->childs[0]);
    if(node->child_num >= 3){
        handleDecList(t,node->childs[2]);
    }
}

void handleDef(node_t* node){
    Type t = malloc(sizeof(struct Type_));
    handleSpecifier(t,node->childs[0]);
    handleDecList(t,node->childs[1]);
}

void handleDefList(node_t* node){  //局部变量定义，需要额外开一个符号表
    if(node->childs[0] != EMPTY_NODE){
        handleDef(node->childs[0]);
        handleDefList(node->childs[1]);  
    }
}

void handleStructSpecifier(Type t,node_t* node){
    if(strcmp(node->childs[1]->name,"Tag")==0){ 
        node_t* ID = node->childs[1]->childs[0];
        node_st* target = lookup(ID->info,17,ID->line);
        if(target == NULL) return;
        t->kind = target->type->kind;
        t->u.local_sign_table = target->type->u.local_sign_table;
        return;
    }else if(strcmp(node->childs[1]->name,"OptTag")==0){  //定义结构体
        node_t* OptTag = node->childs[1];
        node_t* ID = OptTag->childs[0];
        HashTable* ht;
        if(ID != EMPTY_NODE){
            node_st* target = get(getTop(&sign_table),ID->info);
            if(target != NULL){
                printf("Error type %d at Line %d: %s\"%s\".\n", 16, ID->line,"Duplicated name ",ID->info);
                return;
            }
            insert(getTop(&sign_table),ID->info,t,ID->line);
            ht = create_table(INITIAL_CAPACITY,ID->info);
        }else{ //匿名结构体给一个独特的隐藏名称
            char suffix[MAX_INFO_LEN];
            sprintf(suffix, "%d", magicNumber);
            magicNumber++;
            char* sn = "SpecialName";
            char* nn = malloc(strlen(sn) + 1);
            strcpy(nn,sn);
            char* newname = append_string(nn,suffix);
            insert(getTop(&sign_table),newname,t,node->line);
            ht = create_table(INITIAL_CAPACITY,newname);
        }
        push(&sign_table,ht);
        t->u.local_sign_table = ht;
        handleDefList(node->childs[3]);//处理完结构体内定义后需要退栈
        pop(&sign_table);
    }
}

void handleSpecifier(Type t,node_t* node){
    if(strcmp(node->childs[0]->name,"TYPE")==0){
        t->kind = BASIC;
        if(strcmp(node->childs[0]->info,"int")==0){
            t->u.basic = TYPE_INT;
        }else if(strcmp(node->childs[0]->info,"float")==0){
            t->u.basic = TYPE_FLOAT;
        }
        return;
    }else if(strcmp(node->childs[0]->name,"StructSpecifier")==0){
        t->kind = STRUCTURE;
        handleStructSpecifier(t,node->childs[0]);
    }
}

void handleExtDecList(Type t,node_t* node){
    handleVarDec(t,node->childs[0]);
    if(node->child_num >= 3){
        handleExtDecList(t,node->childs[2]);
    }
}

void handleParamDec(node_t* node){
    Type t = malloc(sizeof(struct Type_));
    handleSpecifier(t,node->childs[0]);
    handleVarDec(t,node->childs[1]);
}

void handleVarList(node_t* node){
    handleParamDec(node->childs[0]);
    if(node->child_num >= 3){
        handleVarList(node->childs[2]);
    }
}

void handleFunDec(Type t,node_t* node){
    node_t* ID = node->childs[0];
    Type nt = malloc(sizeof(struct Type_));
    nt->kind = FUNCTION;
    HashTable* ht = create_table(INITIAL_CAPACITY,ID->info);
    push(&sign_table,ht);
    nt->u.local_sign_table = ht;//函数的形参列表
    ht->retVal = t;
    nt->retVal = t;
    insert(sign_table.my_stack[0],ID->info,nt,ID->line);  //函数不会嵌套定义，都是全局
    if(node->child_num == 4){
        handleVarList(node->childs[2]);
    }//FunDec之后紧跟的CompSt可能会用到形参，先不退栈
}

void handleExtDef(node_t* node){
    Type t = malloc(sizeof(struct Type_));
    handleSpecifier(t,node->childs[0]);
    node_t* helper = node->childs[1];
    if(strcmp(helper->name,"ExtDecList")==0){
        handleExtDecList(t,helper);
    }else if(strcmp(helper->name,"FunDec")==0){
        handleFunDec(t,helper);
        handleCompSt(node->childs[2]);
        pop(&sign_table);  //把函数的符号表出栈
    }
}

void handleCompSt(node_t* node){
    HashTable* ht = create_table(INITIAL_CAPACITY,"Temp");
    push(&sign_table,ht);
    handleDefList(node->childs[1]);
    dfs(node->childs[2]);   //遍历完推出CompSt需要出栈符号表
    pop(&sign_table);
}

int handleAssignopLeft(node_t* node){
    if(node->child_num == 1 && strcmp(node->childs[0]->name,"ID")==0){//左值为ID
        return 0;
    }else if(node->child_num == 3 && strcmp(node->childs[1]->name,"DOT")==0){//左值为Exp DOT ID
        return 0;
    }else if(node->child_num == 4 && strcmp(node->childs[0]->name,"Exp")==0 && strcmp(node->childs[1]->name,"LB")==0){//左值为 Exp LB Exp RB
        return 0;
    }else{//非法左值错误
        my_error(6,node->line,"The left-hand side of an assignment must be a variable");
        return -1;
    }
}

int checkAssignopType(Type t1,Type t2){
    if(t1->kind == BASIC){
        if(t2->kind == BASIC){//都是基本变量
            if(t1->u.basic != t2->u.basic){
                return -1;
            }
        }else{
            return -1;
        }
    }else if(t1->kind == ARRAY){
        if(t2->kind == ARRAY){
            return checkAssignopType(t1->u.array.elem,t2->u.array.elem);
        }else{
            return -1;
        }
    }else if(t1->kind == STRUCTURE){
        if(t2->kind != STRUCTURE){
            return -1;
        }
    }else if(t1->kind == FUNCTION){//这算语法错误吧？
        if(t2->kind != FUNCTION){
            return -1;
        }
    }
    return 0;
}

int checkOperatorType(Type t1,Type t2){
    if(t1->kind == STRUCTURE || t1->kind == ARRAY || t1->kind == FUNCTION){
        return -1;
    }
    if(t2->kind == STRUCTURE || t2->kind == ARRAY || t2->kind == FUNCTION){
        return -1;
    }
    if(t1->kind == BASIC && t2->kind == BASIC){
        if(t1->u.basic != t2->u.basic){
            return -1;
        }else{
            return 0;
        }
    }
}

int checkLogicType(Type t1,Type t2){
    if(t1->kind == BASIC && t1->u.basic == TYPE_INT && \
       t2->kind == BASIC && t2->u.basic == TYPE_INT){
        return 0;
       }else{
        return -1;
       }
}

int checkArgs(node_st* Params[],node_st* Args[],int len){ //依次检查类型是否相等
    for(int i = 0;i<len;i++){
        if(Params[i] == NULL || Args[i] == NULL){
            continue;
        }
        if(checkEqualType(Params[i]->type,Args[i]->type)== -1){
            return -1;
        }
    }
    return 0;
}

int handleArgs(node_t* node,node_st* Args[],int index){
    if(node->child_num == 1){
        Args[index] = handleExp(node->childs[0]);
        return index;
    }else if(node->child_num == 3){
        Args[index] = handleExp(node->childs[0]);
        return handleArgs(node->childs[2],Args,index+1);
    }
}

// 函数：动态拼接字符串
char* append_string(char* original, const char* to_append) {
    // 计算原字符串和要追加的字符串长度
    size_t original_len = original ? strlen(original) : 0;
    size_t to_append_len = strlen(to_append);
    
    // 使用 realloc 扩展原字符串的内存，+1 是为了放 '\0'
    char* new_string = realloc(original, original_len + to_append_len + 1);
    
    if (new_string == NULL) {
        // 如果内存分配失败，返回 NULL
        printf("Memory allocation failed!\n");
        return NULL;
    }
    
    // 拼接字符串
    strcpy(new_string + original_len, to_append);
    
    return new_string;
}

char* formString(node_st* Args[],int len,char* init){
    char* ret = init;
    ret = append_string(ret,"(");
    for(int i = 0;i<len;i++){
        if(Args[i] == NULL){
            ret = append_string(ret,"null");
            if(i < len -1){
                ret = append_string(ret,", ");
            }
            continue;
        }
        if(Args[i]->type->kind == BASIC){
            if(Args[i]->type->u.basic == TYPE_INT){
                ret = append_string(ret,"int");
            }else if(Args[i]->type->u.basic == TYPE_FLOAT){
                ret = append_string(ret,"float");
            }
        }else if(Args[i]->type->kind == ARRAY){
           ret = append_string(ret,"array");
        }else if(Args[i]->type->kind == STRUCTURE){
           ret = append_string(ret,"struct");
        }else{
            ;
        }

        if(i < len -1){
            ret = append_string(ret,", ");
        }
    }
    ret = append_string(ret,")");
    return ret;
}


node_st* handleExp(node_t* node){
    if(node == NULL || node == EMPTY_NODE){
        printf("...Exp Empty Error...");
        return NULL;
    }
    if(node->child_num == 3){
        node_t* operator = node->childs[1];
        if(strcmp(operator->name,"ASSIGNOP")==0){ //判断是否有左值错误
            int i = handleAssignopLeft(node->childs[0]);
            if(i != -1){ //没有左值错误
                node_st* nst1 = handleExp(node->childs[0]);
                node_st* nst2 = handleExp(node->childs[2]);
                if (nst1 == NULL || nst2 == NULL){
                    return NULL ;
                }
                Type t1 = nst1->type;
                Type t2 = nst2->type;
                int j = checkEqualType(t1,t2);
                if(j == -1){
                    my_error(5,operator->line,"Type mismatched for assignment");
                }
            }
        }else if(strcmp(operator->name,"PLUS")==0 || \
                 strcmp(operator->name,"MINUS")==0 || \
                 strcmp(operator->name,"STAR")==0 || \
                 strcmp(operator->name,"DIV")==0 ){
            node_st* nst1 = handleExp(node->childs[0]);
            node_st* nst2 = handleExp(node->childs[2]);
            if (nst1 == NULL || nst2 == NULL){
                    return NULL ;
            }
            Type t1 = nst1->type;
            Type t2 = nst2->type;
            int j = checkOperatorType(t1,t2);//算术运算只允许INT与FLOAT
                if(j == -1){
                    my_error(7,operator->line,"Type mismatched for operands");
                    return NULL;
                }
                //TODO 根据operator nst1.val = nst1.val operator nst2.val ? 
            
            return nst1;
        }else if(strcmp(operator->name,"AND")==0 || \
                strcmp(operator->name,"OR")==0 || \
                strcmp(operator->name,"RELOP")==0 ){
                node_st* nst1 = handleExp(node->childs[0]);
                node_st* nst2 = handleExp(node->childs[2]);
                if (nst1 == NULL || nst2 == NULL){
                    return NULL ;
                }
                Type t1 = nst1->type;
                Type t2 = nst2->type;
                int j = checkOperatorType(t1,t2);
                if(j == -1){
                    my_error(7,operator->line,"Type mismatched for operands");
                    return NULL;
                }
                //TODO 不知道要不要写，类型检查这块我感觉不要
            return nst1;
        }else if(strcmp(node->childs[0]->name,"LP")==0 && \
                 strcmp(node->childs[1]->name,"Exp")==0 && \
                 strcmp(node->childs[2]->name,"RP")==0){  //Exp -> LP Exp RP  括号运算
            return handleExp(operator);
        }else if(strcmp(node->childs[0]->name,"ID")==0 && \
                 strcmp(node->childs[1]->name,"LP")==0 && \
                 strcmp(node->childs[2]->name,"RP")==0){  //函数调用
            node_st* t = lookup(node->childs[0]->info,2,node->line);
            if(t == NULL ) return NULL;
            if(t->type->kind != FUNCTION){
                printf("Error type %d at Line %d: \"%s\"%s.\n", 11, node->childs[0]->line,node->childs[0]->info," is not a function");
            }else{
                if(t->type->u.local_sign_table->size != 0){//有参数
                    HashTable* ht = t->type->u.local_sign_table;//形参列表
                    node_st** sortedArray = (node_st**)malloc(ht->size * sizeof(node_st*));
                    if (sortedArray == NULL) {
                    // 分配失败，处理错误
                        fprintf(stderr, "Memory allocation failed\n");
                        return NULL;
                    }
                    traverse_and_sort(ht,sortedArray);
                    char* output1 = formString(sortedArray,ht->size,t->key);
                    printf("Error type %d at Line %d: Function \"%s\" is not applicable for arguments \"%s\".\n", 9, node->childs[0]->line,output1,"()");
                }
            }
        }else if(strcmp(node->childs[0]->name,"Exp")==0 && \
                 strcmp(node->childs[1]->name,"DOT")==0 && \
                 strcmp(node->childs[2]->name,"ID")==0) {//调用结构体内部变量
            node_st* t = handleExp(node->childs[0]);
            if(t == NULL ) return NULL;
            if(t->type->kind != STRUCTURE){
                my_error(13,node->line,"Illegal use of \".\"");
            }else{//进一步访问成员变量
                node_st* ret = get(t->type->u.local_sign_table,node->childs[2]->info);
                if(ret == NULL){ //在结构体成员中没找到对应符号
                    printf("Error type %d at Line %d: %s\"%s\".\n", 14, node->line,"Non-existent field ",node->childs[2]->info);
                }
                return ret;
            }
        }
    }else if(node->child_num == 1){
        if(strcmp(node->childs[0]->name,"ID")==0){
            node_t* ID = node->childs[0];
            node_st* t = lookup(ID->info,1,ID->line); //调用变量，直接查找表项
            return t;
        }else if(strcmp(node->childs[0]->name,"INT")==0){ //个人理解相当于没有表项的匿名变量
            node_st* t = malloc(sizeof(node_st));
            t->type = malloc(sizeof(struct Type_));
            t->type->kind = BASIC;
            t->type->u.basic = TYPE_INT;

            t->value = node->childs[0]->info;
            t->key = t->value;
            t->line = node->childs[0]->line;
            t->defined = true;
            return t;
        }else if(strcmp(node->childs[0]->name,"FLOAT")==0){
            node_st* t = malloc(sizeof(node_st));
            t->type = malloc(sizeof(struct Type_));
            t->type->kind = BASIC;
            t->type->u.basic = TYPE_FLOAT;

            t->value = node->childs[0]->info;
            t->key = t->value;
            t->line = node->childs[0]->line;
            t->defined = true;
            return t;
        }
    }else if(node->child_num == 2){
        if(strcmp(node->childs[0]->name,"NOT")==0){  
            node_st* nst = handleExp(node->childs[1]);//逻辑运算，运算结果并没有进行，如果后续有需要值的话这里面再加加
            if(nst == NULL ) return NULL;
            Type t = nst->type;
            if(t->kind == BASIC && t->u.basic == TYPE_INT){
                return nst;
            }else{
                my_error(7,node->line,"Type mismatched for operands");
            }
        }else if(strcmp(node->childs[0]->name,"MINUS")==0){//INT和FLOAT都可以
            node_st* nst = handleExp(node->childs[1]);
            if(nst == NULL ) return NULL;
            Type t = nst->type;
            if(t->kind == BASIC){
                return nst;
            }else{
                my_error(7,node->line,"Type mismatched for operands");
            }
        }
    }else if(node->child_num == 4){//一共有两种情况
        if(strcmp(node->childs[0]->name,"ID")==0){ //Exp -> ID LP Args RP 函数传参调用,重量级
            node_st* nst = lookup(node->childs[0]->info,2,node->childs[0]->line);
            if(nst != NULL && nst->type->kind == FUNCTION){
                HashTable* ht = nst->type->u.local_sign_table;//形参列表
                node_st** sortedArray = (node_st**)malloc(ht->size * sizeof(node_st*));
                if (sortedArray == NULL) {
                    // 分配失败，处理错误
                    fprintf(stderr, "Memory allocation failed\n");
                    return NULL;
                }
                traverse_and_sort(ht,sortedArray);
                node_st** ArgsList = (node_st**)malloc(10 * ht->size * sizeof(node_st*));
                if (ArgsList == NULL) {
                    // 分配失败，处理错误
                    fprintf(stderr, "Memory allocation failed\n");
                    return NULL;
                }
                int s = handleArgs(node->childs[2],ArgsList,0) + 1;
                char* o1 = malloc(strlen(nst->key) + 1);
                strcpy(o1,nst->key);
                char* o2 = malloc(1);
                strcpy(o2,"");
                char* output1 = formString(sortedArray,ht->size,o1);
                char* output2 = formString(ArgsList,s,o2);
                if(s != ht->size){ //输出错误信息
                    printf("Error type %d at Line %d: Function \"%s\" is not applicable for arguments \"%s\".\n", 9, node->childs[0]->line,output1,output2);
                }else{
                    int j = checkArgs(sortedArray,ArgsList,ht->size);
                    if(j == -1){
                        printf("Error type %d at Line %d: Function \"%s\" is not applicable for arguments \"%s\".\n", 9, node->childs[0]->line,output1,output2);
                    }else{
                        node_st* helper = malloc(sizeof(node_st));
                        helper->type = nst->type->retVal;
                        helper->line = nst->line;
                        return helper;
                    }
                }
            }else{
                if(nst == NULL) return NULL;
                printf("Error type %d at Line %d: \"%s\"%s.\n", 11, node->childs[0]->line,node->childs[0]->info," is not a function");
            }
        }else if(strcmp(node->childs[0]->name,"Exp")==0){ // Exp -> Exp LB Exp RB //数组调用
            node_st* nst = handleExp(node->childs[0]);
            if(nst == NULL ) return NULL;
            Type t1 = nst->type;
            if(t1->kind != ARRAY){ //对非数组变量用[]
                printf("Error type %d at Line %d: \"%s\"%s.\n", 10, node->childs[0]->line,nst->key," is not an array");
            }else{
                node_st* nst2 = handleExp(node->childs[2]);
                if(nst2 == NULL) return NULL;
                Type t2 = nst2->type;
                if(!(t2->kind == BASIC && t2->u.basic == TYPE_INT)){//方框内非整数
                    printf("Error type %d at Line %d: \"%s\"%s.\n", 12, node->childs[2]->line,nst2->key," is not an integer");
                }else{
                    node_st* helper = malloc(sizeof(node_st));
                    helper->type = t1->u.array.elem;
                    helper->line = nst2->line;
                    return helper;
                }
            }
        }
    }
    return NULL;
}

int checkEqualType(Type t1,Type t2){
    //先判断当前环境是不是结构体内
    if(t1->kind != t2->kind){
        return -1;
    }else{
        if(t1->kind == BASIC){
            if(t1->u.basic != t2->u.basic){
                return -1;
            }else{
                return 0;
            }
        }else if(t1->kind == ARRAY){
            return checkEqualType(t1->u.array.elem,t2->u.array.elem);
        }else if(t1->kind == FUNCTION){
            return -1;
        }else if(t1->kind == STRUCTURE){
            HashTable* ht1 = t1->u.local_sign_table;//形参列表
            HashTable* ht2 = t2->u.local_sign_table;
            if(ht1 == NULL && ht2 == NULL){
                return 0;
            }
            if(ht1 == NULL && ht2 != NULL){
                return -1;
            }
            if(ht1 != NULL && ht2 == NULL){
                return -1;
            }
            if(ht1->size!=ht2->size){
                return -1;
            }
            // if(strcmp(ht1->domain,ht2->domain) != 0){
            //     return -1;
            // }
            node_st** sortedArray1 = (node_st**)malloc(ht1->size * sizeof(node_st*));
                if (sortedArray1 == NULL) {
                    // 分配失败，处理错误
                    fprintf(stderr, "Memory allocation failed\n");
                    return -1;
                }
            traverse_and_sort(ht1,sortedArray1);
            node_st** sortedArray2 = (node_st**)malloc(ht2->size * sizeof(node_st*));
                if (sortedArray2 == NULL) {
                    // 分配失败，处理错误
                    fprintf(stderr, "Memory allocation failed\n");
                    return -1;
                }
            traverse_and_sort(ht2,sortedArray2);
            return checkArgs(sortedArray1,sortedArray2,ht1->size);
        }else{
            return -1;
        }
    }
}


int checkRetType(Type t,int line){//从符号表的栈中找到第一个嵌套的函数,具体而言，从上往下找域为一个函数名称的散列表
    int i = sign_table.top;
    HashTable* ht;
    char* key ;
    node_st* nst;
    while(i > -1){
        ht = sign_table.my_stack[i];
        key = ht->domain;

        if(strcmp(key,"Temp")==0){
            i--;
            continue;
        }
        nst = lookup(key,2,line);//在global中找到函数在符号表里定义表项
        if(nst != NULL && nst->type->kind == FUNCTION){
            break;
        }
        i--;
    }
    Type t2 = ht->retVal;
    return checkEqualType(t,t2);
}

void handleReturn(node_t* node){
    node_st* nst = handleExp(node->childs[1]);
    if(nst == NULL) return;
    int i = checkRetType(nst->type,nst->line);
    if(i == -1){ //返回类型不匹配
        my_error(8,node->childs[1]->line,"Type mismatched for return");
    }
}

void dfs(node_t* node){
    if (node == NULL || node == EMPTY_NODE) {
        return;
    }

    char* name = node->name;
    if((strcmp(name,"SEMI") == 0) || \
        (strcmp(name,"IF") == 0) || \
        (strcmp(name,"COMMA") == 0) || \
        (strcmp(name,"ELSE") == 0) || \
        (strcmp(name,"LP") == 0) || \
        (strcmp(name,"RP") == 0) || \
        (strcmp(name,"WHILE") == 0)){
        return;
    }

    if(strcmp(name,"ExtDef")==0){
        handleExtDef(node);
    }else if(strcmp(name,"Def")== 0){
        handleDef(node);
    }else if(strcmp(name,"Exp")==0){
        handleExp(node);
    }else if(strcmp(name,"CompSt")==0){
        handleCompSt(node);
    }else if((node->childs[0] != NULL && node->childs[0] != EMPTY_NODE)  && strcmp(node->childs[0]->name,"RETURN")==0){
        handleReturn(node);
    }else{
        for (int i = 0;i<node->child_num;++i) {
            dfs(node->childs[i]);
        }
    }
}


