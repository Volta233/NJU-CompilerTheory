#include "code_gen.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

char* get_const_val(char* src){
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

static bool flag;
static int stack_size;
static char* the_main = "main";
static char* tmp = "#tmp";

static void write_code(const ir_t *ir,FILE *f, int lineno){
    switch(ir->ir_type) {
        case IR_LABEL:{
            if(lineno > cur_bb->exit.lineno){
                // printf("trigger work in %d for %d bb\n", lineno, cur_bb->order_no);
                trigger_when_leave_bb(f, lineno);
                flag = true;
            }
            fprintf(f, "%s :\n", ir->src_names[0]);
            break;
        }
        case IR_ASSIGN:{
            if(ir->src_names[0][0] == '#' && ir->dest_name[0] != '*'){
                char *const_val_x = get_const_val(ir->src_names[0]);
                char* reg_x = get_reg(ir->dest_name, lineno, f);
                fprintf(f,"  li %s, %s\n", reg_x, const_val_x);
                set_dirty_for_var(reg_x);
                free(const_val_x);
            }else if(ir->src_names[0][0] == '*' && ir->dest_name[0] != '*'){
                char* reg_x = get_reg(ir->dest_name, lineno, f);
                char* ptr_val = get_const_val(ir->src_names[0]);
                char* reg_y = get_reg(ptr_val, lineno, f);
                fprintf(f,"  lw %s, 0(%s)\n",reg_x, reg_y);
                set_dirty_for_var(reg_x);
                free(ptr_val);
            }else if(ir->src_names[0][0] == '*' && ir->dest_name[0] == '*'){
                // 数组赋值，由于这块对应于IR中两个寄存器中地址的较量，因此得好好想想怎么迁移数据
                char* ptr_val_y = get_const_val(ir->src_names[0]);
                char* ptr_val_x = get_const_val(ir->dest_name);
                char* reg_x = get_reg(ptr_val_x, lineno, f);
                char* reg_y = get_reg(ptr_val_y, lineno, f);
                // 甚至都不需要获得reg，也不需要发出汇编指令，只要到时候修改offset就可以了
                spilling_var_t* loc_dest = get_DEC_loc(ptr_val_x);
                spilling_var_t* loc_src = get_DEC_loc(ptr_val_y);
                // loc_dest->offset = loc_src->offset;
                char* tmp_reg = get_reg(tmp, lineno, f);
                fprintf(f,"  lw %s, 0(%s)\n",tmp_reg, reg_y);
                fprintf(f,"  sw %s, 0(%s)\n",tmp_reg, reg_x);
                set_reg_free(tmp_reg);
                free(ptr_val_x);
                free(ptr_val_y);
            }else if(ir->dest_name[0] == '*'){
                char* ptr_val = get_const_val(ir->dest_name);
                char* reg_x = get_reg(ptr_val, lineno, f);
                if(ir->src_names[0][0] != '#'){
                    char* reg_y = get_reg(ir->src_names[0], lineno, f);
                    fprintf(f,"  sw %s, 0(%s)\n",reg_y, reg_x);
                }else{
                    char *const_val_x = get_const_val(ir->src_names[0]);
                    fprintf(f,"  sw %s, 0(%s)\n",const_val_x, reg_x);
                    free(const_val_x);
                }
                free(ptr_val);
            }else{
                char* reg_x = get_reg(ir->dest_name, lineno, f);
                char* reg_y = get_reg(ir->src_names[0], lineno, f);
                fprintf(f,"  move %s, %s\n",reg_x, reg_y);
                if(!is_needed(ir->src_names[0],lineno,cur_bb)){
                    set_reg_free(reg_y);
                }
                set_dirty_for_var(reg_x);
            }
            break;
        }
        case IR_PLUS:{
            if(ir->src_names[1][0] == '#' && ir->src_names[0][0] != '#'){
                char *const_val_y = get_const_val(ir->src_names[1]);
                char* reg_x = get_reg(ir->dest_name, lineno, f);
                char* reg_y = get_reg(ir->src_names[0], lineno, f);
                if(!is_needed(ir->src_names[0],lineno,cur_bb)){
                    set_reg_free(reg_y);
                }
                fprintf(f,"  addi %s, %s, %s\n",reg_x, reg_y,const_val_y);
                set_dirty_for_var(reg_x);
                free(const_val_y);
            }else if(ir->src_names[0][0] == '#' && ir->src_names[1][0] != '#'){
                char* reg_const = get_reg(ir->src_names[0], lineno, f);
                char *const_val_x = get_const_val(ir->src_names[0]);
                fprintf(f,"  li %s, %s\n",reg_const, const_val_x);
                char* reg_x = get_reg(ir->dest_name, lineno, f);
                char* reg_y = get_reg(ir->src_names[1], lineno, f);
                if(!is_needed(ir->src_names[1],lineno,cur_bb)){
                    set_reg_free(reg_y);
                }
                fprintf(f,"  add %s, %s, %s\n",reg_x, reg_const, reg_y);
                set_dirty_for_var(reg_x);
                set_reg_free(reg_const);
                free(const_val_x);
            }else if(ir->src_names[0][0] == '#' && ir->src_names[1][0] == '#'){
                char *const_val_x = get_const_val(ir->src_names[0]);
                char *const_val_y = get_const_val(ir->src_names[1]);
                char* reg_x = get_reg(ir->dest_name, lineno, f);
                fprintf(f,"  addi %s, %s, %s\n",reg_x, const_val_x, const_val_y);
                set_dirty_for_var(reg_x);
                free(const_val_x);
                free(const_val_y);
            }else if(ir->src_names[0][0] == '&'){
                char* DEC_name = get_const_val(ir->src_names[0]);
                char* reg_x = get_reg(ir->dest_name, lineno, f);
                char* reg_y = get_reg(ir->src_names[0], lineno, f);
                char* reg_z = get_reg(ir->src_names[1], lineno, f);
                binding_var(ir->dest_name, DEC_name);
                if(!is_needed(ir->src_names[1],lineno,cur_bb)){
                    set_reg_free(reg_z);
                }
                if(!is_needed(ir->src_names[0],lineno,cur_bb)){
                    set_reg_free(reg_y);
                }
                fprintf(f,"  add %s, %s, %s\n",reg_x, reg_y,reg_z);
                set_dirty_for_var(reg_x);
                free(DEC_name);
            }else{
                char* reg_x = get_reg(ir->dest_name, lineno, f);
                char* reg_y = get_reg(ir->src_names[0], lineno, f);
                char* reg_z = get_reg(ir->src_names[1], lineno, f);
                if(!is_needed(ir->src_names[1],lineno,cur_bb)){
                    set_reg_free(reg_z);
                }
                if(!is_needed(ir->src_names[0],lineno,cur_bb)){
                    set_reg_free(reg_y);
                }
                fprintf(f,"  add %s, %s, %s\n",reg_x, reg_y,reg_z);
                set_dirty_for_var(reg_x);
            }
            break;
        }
        case IR_SUB:{
            if(ir->src_names[1][0] == '#' && ir->src_names[0][0] != '#'){
                char *const_val_y = get_const_val(ir->src_names[1]);
                char* reg_x = get_reg(ir->dest_name, lineno, f);
                char* reg_y = get_reg(ir->src_names[0], lineno, f);
                if(!is_needed(ir->src_names[0],lineno,cur_bb)){
                    set_reg_free(reg_y);
                }
                fprintf(f,"  addi %s, %s, -%s\n",reg_x, reg_y,const_val_y);
                set_dirty_for_var(reg_x);
                free(const_val_y);
            }else if(ir->src_names[0][0] == '#' && ir->src_names[1][0] != '#'){
                char* reg_const = get_reg(ir->src_names[0], lineno, f);
                char *const_val_x = get_const_val(ir->src_names[0]);
                fprintf(f,"  li %s, %s\n",reg_const, const_val_x);
                char* reg_x = get_reg(ir->dest_name, lineno, f);
                char* reg_y = get_reg(ir->src_names[1], lineno, f);
                if(!is_needed(ir->src_names[1],lineno,cur_bb)){
                    set_reg_free(reg_y);
                }
                fprintf(f,"  sub %s, %s, %s\n",reg_x, reg_const, reg_y);
                set_dirty_for_var(reg_x);
                set_reg_free(reg_const);
                free(const_val_x);
            }else if(ir->src_names[0][0] == '#' && ir->src_names[1][0] == '#'){
                char *const_val_x = get_const_val(ir->src_names[0]);
                char *const_val_y = get_const_val(ir->src_names[1]);
                char* reg_x = get_reg(ir->dest_name, lineno, f);
                fprintf(f,"  addi %s, -%s, -%s\n",reg_x, const_val_x, const_val_y);
                set_dirty_for_var(reg_x);
                free(const_val_x);
                free(const_val_y);
            }else{
                char* reg_x = get_reg(ir->dest_name, lineno, f);
                char* reg_y = get_reg(ir->src_names[0], lineno, f);
                char* reg_z = get_reg(ir->src_names[1], lineno, f);
                if(!is_needed(ir->src_names[1],lineno,cur_bb)){
                    set_reg_free(reg_z);
                }
                if(!is_needed(ir->src_names[0],lineno,cur_bb)){
                    set_reg_free(reg_y);
                }
                fprintf(f,"  sub %s, %s, %s\n", reg_x, reg_y, reg_z);
                set_dirty_for_var(reg_x);
            }
            break;
        }
        case IR_MULTI:{
                char* reg_x = get_reg(ir->dest_name, lineno, f);
                char* reg_y = get_reg(ir->src_names[0], lineno, f);
                char* reg_z = get_reg(ir->src_names[1], lineno, f);
                if(ir->src_names[1][0] == '#'){
                    char *const_val_x = get_const_val(ir->src_names[1]);
                    fprintf(f,"  li %s, %s\n", reg_z, const_val_x);
                    free(const_val_x);
                }
                if(ir->src_names[0][0] == '#'){
                    char *const_val_y = get_const_val(ir->src_names[0]);
                    fprintf(f,"  li %s, %s\n", reg_y, const_val_y);
                    free(const_val_y);
                }
                if(ir->src_names[1][0] == '#' || !is_needed(ir->src_names[1],lineno,cur_bb)){
                    set_reg_free(reg_z);
                }
                if(ir->src_names[0][0] == '#' || !is_needed(ir->src_names[0],lineno,cur_bb)){
                    set_reg_free(reg_y);
                }
                fprintf(f,"  mul %s, %s, %s\n", reg_x, reg_y, reg_z);
                set_dirty_for_var(reg_x);
            break;
        }
        case IR_DIV:{
            char* reg_x = get_reg(ir->dest_name, lineno, f);
            char* reg_y = get_reg(ir->src_names[0], lineno, f);
            char* reg_z = get_reg(ir->src_names[1], lineno, f);
            if(!is_needed(ir->src_names[1],lineno,cur_bb)){
                set_reg_free(reg_z);
            }
            if(!is_needed(ir->src_names[0],lineno,cur_bb)){
                set_reg_free(reg_y);
            }
            fprintf(f, "  div %s, %s\n", reg_y, reg_z);
            fprintf(f, "  mflo %s\n", reg_x);
            set_dirty_for_var(reg_x);
            break;
        }
        case IR_GOTO:{
            // printf("trigger work in %d for %d bb\n", lineno, cur_bb->order_no);
            trigger_when_leave_bb(f, lineno);
            flag = true;
            fprintf(f, "  j %s\n", ir->src_names[0]);
            break;
        }
        case IR_IFTHENGOTO:{
            char* reg_x;
            char* reg_y;
            bool x, y;
            if(ir->src_names[0][0] != '#'){
                x = false;
                reg_x = get_reg(ir->src_names[0], lineno, f);
            }else if(ir->src_names[0][0] == '#'){
                x = true;
                reg_x = get_const_val(ir->src_names[0]);
            }
            
            if(ir->src_names[2][0] != '#'){
                y = false;
                reg_y = get_reg(ir->src_names[2], lineno, f);
            }else if(ir->src_names[2][0] == '#'){
                y = true;
                reg_y = get_const_val(ir->src_names[2]);
            }
            // printf("trigger work in %d for %d bb\n", lineno, cur_bb->order_no);
            trigger_when_leave_bb(f, lineno);
            flag = true;
            if(strcmp(ir->src_names[1],"==")==0){
                fprintf(f,"  beq %s, %s, %s\n", reg_x, reg_y, ir->src_names[3]);
            }else if(strcmp(ir->src_names[1],"!=")==0){
                fprintf(f,"  bne %s, %s, %s\n", reg_x, reg_y, ir->src_names[3]);
            }else if(strcmp(ir->src_names[1],">")==0){
                fprintf(f,"  bgt %s, %s, %s\n", reg_x, reg_y, ir->src_names[3]);
            }else if(strcmp(ir->src_names[1],"<")==0){
                fprintf(f,"  blt %s, %s, %s\n", reg_x, reg_y, ir->src_names[3]);
            }else if(strcmp(ir->src_names[1],">=")==0){
                fprintf(f,"  bge %s, %s, %s\n", reg_x, reg_y, ir->src_names[3]);
            }else if(strcmp(ir->src_names[1],"<=")==0){
                fprintf(f,"  ble %s, %s, %s\n", reg_x, reg_y, ir->src_names[3]);
            }
            if(!x){
                set_reg_free(reg_x);
            }
            if(!y){
                set_reg_free(reg_y);
            }
            if(x) free(reg_x);
            if(y) free(reg_y);
            break;
        }
        case IR_RETURN:{
            char* reg_x = get_reg(ir->src_names[0], lineno, f);
            fprintf(f, "  move $v0, %s\n", reg_x);
            // 触发一次trigger
            trigger_when_leave_bb(f, lineno);
            // 恢复esp
            fprintf(f, "  move $sp, $fp\n");
            fprintf(f, "  jr $ra\n\n");
            break;
        }
        case IR_ARG:{
            // 构建ARGS加入活动记录
            insert_arg(ir->src_names[0]);
            break;
        }
        case IR_CALL:{
            // 先离开当前基本块，此时已经将所有修改过的变量压栈
            if(lineno > cur_bb->exit.lineno){
                trigger_when_leave_bb(f, lineno);
                cur_bb = cur_bb->next;
            }
            // 将当前存储的ARGS压栈
            int spn = *get_spilling_num();
            int offset2 = spn * 4;
            // fprintf(f,"  addi $sp, $fp, -%d \n", offset2); // 确保$sp的值正确
            fprintf(f,"  addi $s1, $fp, -%d \n", offset2); 
            fprintf(f,"  slt $s3, $s1, $s2 \n");
            fprintf(f,"  movn $s2, $s1, $s3 \n");
            fprintf(f,"  move $sp, $s2\n");
            for(int i = 0; i < frame->args_num;i++){
                fprintf(f,"  addi $sp, $sp, -4 \n");
            }
            spilling_var_t* v = frame->args_head;
            char* reg_a;
            int offset;
            for(int j = frame->args_num; j > 0;j--){
                if(v == NULL) break;
                reg_a = get_reg(v->name,lineno,f);
                offset = 4*(j-1);
                fprintf(f,"  sw %s, %d($sp) \n", reg_a, offset);
                v = v->next;
            }
            // 将fp内容压栈
            fprintf(f,"  addi $sp, $sp, -4 \n");
            fprintf(f,"  sw $fp, 0($sp) \n");
            // 将返回地址压栈
            fprintf(f,"  addi $sp, $sp, -4 \n");
            fprintf(f,"  sw $ra, 0($sp) \n");
            // 保存s1和s2 寄存器
            fprintf(f,"  addi $sp, $sp, -4 \n");
            fprintf(f,"  sw $s1, 0($sp) \n");
            fprintf(f,"  addi $sp, $sp, -4 \n");
            fprintf(f,"  sw $s2, 0($sp) \n");
            if (!ir->dest_name) {
                fprintf(f, "  jal _%s\n", ir->src_names[0]);
            } else {
                char* reg_x = get_reg(ir->dest_name, lineno, f);
                fprintf(f, "  jal _%s\n", ir->src_names[0]);
                fprintf(f, "  move %s, $v0\n", reg_x);
                set_dirty_for_var(reg_x);
            }
            // 恢复s1 s2
            fprintf(f,"  lw $s2, 0($sp) \n");
            fprintf(f,"  addi $sp, $sp, 4 \n");
            fprintf(f,"  lw $s1, 0($sp) \n");
            fprintf(f,"  addi $sp, $sp, 4 \n");
            // 恢复返回地址
            fprintf(f,"  lw $ra, 0($sp) \n");
            fprintf(f,"  addi $sp, $sp, 4 \n");
            // 恢复fp
            fprintf(f,"  lw $fp, 0($sp) \n");
            fprintf(f,"  addi $sp, $sp, 4 \n");
            // 恢复esp
            for(int i = 0; i < frame->args_num;i++){
                fprintf(f,"  addi $sp, $sp, 4 \n");
            }
            // 重置args
            free_frames();
            init_frames();
            break;
        }
        case IR_READ:{
            int spn = *get_spilling_num();
            int offset = spn * 4;
            fprintf(f,"  addi $s1, $fp, -%d \n", offset); 
            fprintf(f,"  slt $s3, $s1, $s2 \n");
            fprintf(f,"  movn $s2, $s1, $s3 \n");
            fprintf(f,"  move $sp, $s2\n");
            fprintf(f,"  addi $sp, $sp, -4 \n");
            fprintf(f,"  sw $ra, 0($sp) \n");
            fprintf(f,"  jal read \n");
            fprintf(f,"  lw $ra, 0($sp) \n");
            fprintf(f,"  addi $sp, $sp, 4 \n");
            char* reg_x = get_reg(ir->dest_name, lineno, f);
            fprintf(f,"  move %s, $v0\n",reg_x);
            set_dirty_for_var(reg_x);
            break;
        }
        case IR_WRITE:{
            char* reg_x = get_reg(ir->src_names[0], lineno, f);
            fprintf(f,"  move $a0, %s \n",reg_x);
            int spn = *get_spilling_num();
            int offset = spn * 4;
            fprintf(f,"  addi $s1, $fp, -%d \n", offset); 
            fprintf(f,"  slt $s3, $s1, $s2 \n");
            fprintf(f,"  movn $s2, $s1, $s3 \n");
            fprintf(f,"  move $sp, $s2\n");
            fprintf(f,"  addi $sp, $sp, -4 \n");
            fprintf(f,"  sw $ra, 0($sp) \n");
            fprintf(f,"  jal write \n");
            fprintf(f,"  lw $ra, 0($sp) \n");
            fprintf(f,"  addi $sp, $sp, 4 \n");
            break;
        }
        case IR_FUNCTION:{
            // 作为被调用者开头
            stack_size = 0;
            set_spilling_num(0);
            spilling_table->top++;
            free_frames();
            init_frames();
            if(strcmp(ir->src_names[0],the_main)==0){
                fprintf(f,"%s:\n", ir->src_names[0]);
                fprintf(f,"_%s:\n", ir->src_names[0]);
            }else{
                fprintf(f,"_%s:\n", ir->src_names[0]);
            }
            fprintf(f,"  move $fp, $sp\n");
            fprintf(f,"  move $s1, $fp\n");
            fprintf(f,"  move $s2, $fp\n");
            break;
        }
        case IR_PARAM:{
            char* reg_x = get_reg(ir->src_names[0], lineno, f);
            int offset = 16 + stack_size*4;
            stack_size++;
            fprintf(f, "  lw %s, %d($fp) \n", reg_x, offset);
            set_dirty_for_var(reg_x);
            break;
        }
        case IR_DEC:{
            // 获得偏移
            int offset = atoi(ir->src_names[1]);
            spilling_var_t* loc = insert_spilling_array(ir->src_names[0],offset);
            // fprintf(f,"addi $sp, $sp, -%s \n", ir->src_names[1]);
            create_DEC_var(ir->src_names[0], loc);
            break;
        }
        default:{
            assert(0 && "THERE IS AN UNHANDLED IR");
        }
    }
}

void init_outputfile(FILE* f){
    fprintf(f,".data\n");
    fprintf(f,"_prompt: .asciiz \"Enter an integer:\"\n");
    fprintf(f,"_ret: .asciiz \"\\n\"\n");
    fprintf(f,".globl main\n");
    fprintf(f,".text\n");
    fprintf(f,"read:\n");
    fprintf(f,"  li $v0, 4 \n");
    fprintf(f,"  la $a0, _prompt \n");
    fprintf(f,"  syscall \n");
    fprintf(f,"  li $v0, 5 \n"); 
    fprintf(f,"  syscall \n");
    fprintf(f,"  jr $ra  \n\n");
    fprintf(f,"write:\n");
    fprintf(f,"  li $v0, 1 \n");
    fprintf(f,"  syscall \n");
    fprintf(f,"  li $v0, 4 \n");
    fprintf(f,"  la $a0, _ret \n");
    fprintf(f,"  syscall \n");
    fprintf(f,"  move $v0, $0 \n");
    fprintf(f,"  jr $ra \n\n");
}

void code_gen(syntax_tree_visitor_t *visitor){
    init_regs();
    init_spilling_table();
    init_frames();
    init_DEC_mem();
    init_outputfile(visitor->out);
    divide_bb(visitor);
    construct_var_table();
    ir_t *cur = visitor->code;
    int lineno = 0;
    stack_size = 0; // 用来取参数的offset设置
    flag = false;
    cur_bb = BBs_entry;
    // printf("begin writing code...\n");
    if(cur_bb == NULL) printf("the cur_bb is NULL\n");
    while (cur != NULL) {
        // 这一块完全的屎山，别管
        if(flag){
            // printf("cur_bb is %d\n", cur_bb->order_no);
            cur_bb = cur_bb->next;
            flag = false;
        }
        write_code(cur, visitor->out,lineno);
        if(cur->ir_type == IR_CALL){
            trigger_when_leave_bb(visitor->out, lineno);
            flag = true;  //给下一步跳出call指令基本块使用
        }
        if(cur->ir_type == IR_FUNCTION && lineno > cur_bb->exit.lineno){
            // 出函数后函数内修改的脏位变量不需要写回内存，所以直接进入下一块
            // printf("cur_bb is %d\n", cur_bb->order_no);
            cur_bb = cur_bb->next;
        }
        cur = cur->next;
        lineno++;
    }
    free_bbs();
    free_regs();
    free_DEC_mem();
    free_spilling_table();
}