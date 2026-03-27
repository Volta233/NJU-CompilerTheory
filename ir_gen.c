#include "ir_gen.h"
#include "hashmap.h"
#include "node.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/**
    Name specification

    1. Global variables cannot have duplicate names.
    2. Within the same function, variables and formal parameters cannot have duplicate names.
    3. Variables and formal parameters within a function can have the same name as global variables, but it is not recommended.
    4. Variables and formal parameters in different functions can have the same name, but it is not recommended.
 */
typedef struct var_list_node_s var_list_node_t;
struct var_list_node_s {
    char var[MAX_VAR_NAME_LEN];
    struct var_list_node_s *next;
};

typedef struct var_list_s var_list_t;
struct var_list_s {
    var_list_node_t *head;
};


const sym_info_t *lookup_sym_table(const sym_table_t *sym_table, const char *name) {
   sym_info_t info;
   strcpy(info.name, name);
   const sym_info_t *result = hashmap_get(sym_table->map,&info);
   return result;
}
void get_immediate_var_name(int immediate, char *buf) {
    if (buf == NULL) {
        return;
    }
    sprintf(buf, "#%d", immediate);
}

// forward declaration
static ir_t *generate_exp(const node_t *exp, const sym_table_t *sym_table, const char *place);
static ir_t *generate_stmt(const node_t *stmt, const sym_table_t *sym_table);
static ir_t *generate_args(const node_t *args, const sym_table_t *sym_table, /*out*/var_list_t *arg_list);
static sym_table_t *construct_local_sym_table(const node_t *ext_def);
static void free_sym_table(sym_table_t *sym_table);
static size_t get_array_item_num(const array_type_t *array_type);
static void free_array_type(array_type_t *array_type);
static array_type_t *get_array_name_and_type(const node_t *var_dec, /*out*/char *name, /*out*/array_type_t **);
static ir_t *generate_exp_exp_lb_exp_rb(const node_t *exp, const sym_table_t *sym_table, const char *place, char *ref);
static ir_t *generate_array_ref(const char *base, const array_type_t *array_type, char *indexes[], size_t len, char *ref);
static size_t get_array_dimension(const array_type_t *array_type);

// implements
static ir_t *generate_exp_int(const node_t *integer_exp, const sym_table_t *sym_table, const char *place) {
    const node_t *integer = integer_exp->childs[0];
    int immediate = atoi(integer->info); 
    char buf[MAX_VAR_NAME_LEN];
    get_immediate_var_name(immediate, buf);
    ir_t *ir = create_ir(IR_ASSIGN, place, buf, NULL);
    return ir;
}

static int node_childs_type_eq(const node_t *syntax_node, int n, ...) {
    va_list args;
    va_start(args, n);
    int i = 0;
    if (syntax_node->child_num != n) {
        return 0;
    }
    for (syntax_node_types_t t = va_arg(args, syntax_node_types_t);i<n;t=va_arg(args, syntax_node_types_t),++i) {
        if (syntax_node->childs[i]->node_type != t) {
            return 0;
        }
    }
    va_end(args);
    return 1;
}

static void new_temp(char *buf) {
    static size_t temp_cnt = 0;
    sprintf(buf, "t%zu", temp_cnt);
    ++temp_cnt;
}

static void new_label(char *buf) {
    static size_t label_cnt = 0;
    sprintf(buf, "label%zu", label_cnt);
    ++label_cnt;
}

static ir_t *generate_cond(const node_t *exp, const char *true_label, const char *false_label, const sym_table_t *sym_table);

// any bool exp can be converted to a set of if ... else ... statement
static ir_t *generate_cond_relop(const node_t *exp, const char *true_label, const char *false_label, const sym_table_t *sym_table) {
    // Exp1 RELOP Exp2
    char t1[MAX_VAR_NAME_LEN];
    char t2[MAX_VAR_NAME_LEN];
    new_temp(t1);
    new_temp(t2);
    const node_t *exp1 = exp->childs[0];
    const node_t *exp2 = exp->childs[2];
    const char *relop_op = exp->childs[1]->info;
    ir_t *code1 = generate_exp(exp1, sym_table, t1);
    ir_t *code2 = generate_exp(exp2, sym_table, t2);
    ir_t *code3 = create_ir(IR_IFTHENGOTO, NULL, t1, relop_op, t2, true_label, NULL);

    ir_t *code_goto_false = create_ir(IR_GOTO, NULL, false_label, NULL);
    return add_code(code1, add_code(code2, add_code(code3,  code_goto_false)));
}

static ir_t *generate_cond_not(const node_t *exp, const char *true_label, const char *false_label, const sym_table_t *sym_table) {
    // NOT Exp1
    const node_t *exp1 = exp->childs[1];
    return generate_cond(exp1, false_label, true_label, sym_table);
}

static ir_t *generate_cond_and(const node_t *exp, const char *true_label, const char *false_label, const sym_table_t *sym_table) {
    // Exp1 AND Exp2
    const node_t *exp1 = exp->childs[0];
    const node_t *exp2 = exp->childs[2];
    char label1[MAX_LABEL_NAME_LEN];
    new_label(label1);
    ir_t *code1 = generate_cond(exp1, label1, false_label, sym_table);
    ir_t *code2 = generate_cond(exp2, true_label, false_label, sym_table);
    ir_t *code_label1 = create_ir(IR_LABEL, NULL, label1, NULL);

    return add_code(code1, add_code(code_label1, code2));
}

static ir_t *generate_cond_or(const node_t *exp, const char *true_label, const char *false_label, const sym_table_t *sym_table) {
    // Exp1 OR Exp2
    const node_t *exp1 = exp->childs[0];
    const node_t *exp2 = exp->childs[2];
    char label1[MAX_LABEL_NAME_LEN];
    new_label(label1);
    ir_t *code1 = generate_cond(exp1, true_label, label1, sym_table);
    ir_t *code2 = generate_cond(exp2, true_label, false_label, sym_table);
    ir_t *code_label1 = create_ir(IR_LABEL, NULL, label1, NULL);

    return add_code(code1, add_code(code_label1, code2));
}

static ir_t *generate_cond_other(const node_t *exp, const char *true_label, const char *false_label, const sym_table_t *sym_table) {
    char t1[MAX_VAR_NAME_LEN];
    new_temp(t1);
    char zero[MAX_VAR_NAME_LEN];
    get_immediate_var_name(0, zero);
    ir_t *code1 = generate_exp(exp, sym_table, t1);
    ir_t *code2 = create_ir(IR_IFTHENGOTO, NULL, t1, "!=", zero, true_label, NULL);

    ir_t *code_goto_false = create_ir(IR_GOTO, NULL, false_label, NULL);
    return add_code(code1, add_code(code2,  code_goto_false));
}

static ir_t *generate_cond(const node_t *exp, const char *true_label, const char *false_label, const sym_table_t *sym_table) {
    if (node_childs_type_eq(exp, 3, Exp, Relop, Exp)) {
        return generate_cond_relop(exp, true_label, false_label, sym_table);
    } else if (node_childs_type_eq(exp, 2, Not, Exp)) {
        return generate_cond_not(exp, true_label, false_label, sym_table);
    } else if (node_childs_type_eq(exp, 3, Exp, And, Exp)) {
        return generate_cond_and(exp, true_label, false_label, sym_table);
    } else if (node_childs_type_eq(exp, 3, Exp, Or, Exp)) {
        return generate_cond_or(exp, true_label, false_label, sym_table);
    } else {
        return generate_cond_other(exp, true_label, false_label, sym_table);
    }
    return NULL;
}

static ir_t *generate_exp_call_no_arg(const node_t *exp, const sym_table_t *sym_table, const char *place) {
    // ID LP RP
    const char *func_id = exp->childs[0]->info;
    // const sym_info_t *sym_info = lookup_sym_table(sym_table, func_id);
    const char *func = func_id;
    if (strcmp(func, "read") == 0) {
        return create_ir(IR_READ, place, NULL);
    }
    return create_ir(IR_CALL, place, func, NULL);
}

var_list_t *create_var_list() {
    var_list_t *var_list = malloc(sizeof(var_list_t));
    var_list->head = NULL;
    return var_list;
}

void insert_var_list_head(var_list_t *var_list, const char *var) {
    var_list_node_t *node = malloc(sizeof(var_list_node_t));
    strcpy(node->var, var);
    node->next = var_list->head;
    var_list->head = node;
}

void destroy_var_list(var_list_t *var_list) {
    var_list_node_t *cur = var_list->head;
    while (cur) {
        var_list_node_t *t = cur;
        cur = cur->next;
        free(t);
    }
    free(var_list);
}

size_t size_var_list(const var_list_t *var_list) {
    var_list_node_t *cur = var_list->head;
    size_t cnt = 0;
    while (cur) {
        ++cnt;
        cur = cur->next;
    }
    return cnt;
}


static ir_t *generate_args_single_exp(const node_t *args, const sym_table_t *sym_table, /*out*/var_list_t *arg_list) {
    // Exp
    char t1[MAX_VAR_NAME_LEN];
    new_temp(t1);
    const node_t *exp = args->childs[0];
    ir_t *code1 = generate_exp(exp, sym_table, t1);
    insert_var_list_head(arg_list, t1);
    return code1;
}

static ir_t *generate_args_multi_exp(const node_t *args, const sym_table_t *sym_table, /*out*/var_list_t *arg_list) {
    // Exp COMMA Args
    ir_t *code1 = generate_args_single_exp(args, sym_table, arg_list);
    const node_t *args1 = args->childs[2];
    ir_t *code2 = generate_args(args1, sym_table, arg_list);
    return add_code(code1, code2);
}

// evaluate arg1, ..., argn and assign t1 := arg1, ..., tn := argn
// store [t1, ..., tn] in arg_list
static ir_t *generate_args(const node_t *args, const sym_table_t *sym_table, /*out*/var_list_t *arg_list) {
    // Args
   if (node_childs_type_eq(args, 1, Exp)) {
        return generate_args_single_exp(args, sym_table, arg_list);
   } else if (node_childs_type_eq(args, 3, Exp, Comma, Args)) {
        return generate_args_multi_exp(args, sym_table, arg_list);
   } else {
        assert(0 && "unknown args");
   }
}


static ir_t *generate_exp_call_with_args(const node_t *exp, const sym_table_t *sym_table, const char *place) {
    // ID LP Args RP
    const char *func_id = exp->childs[0]->info;
    // const sym_info_t *sym_info = lookup_sym_table(sym_table, func_id);
    const char *func = func_id;
    // step 1, caculate expressions in Args to generate a list of 
    // temporary variables t1, ..., tn
    var_list_t *arg_list = create_var_list();
    const node_t *args = exp->childs[2];
    ir_t *code1 = generate_args(args, sym_table, arg_list);
    if (strcmp(func, "write") == 0) {
        ir_t *write_arg0 = create_ir(IR_WRITE, NULL, arg_list->head->var, NULL);
        char zero[MAX_VAR_NAME_LEN];
        get_immediate_var_name(0, zero);
        ir_t *place_set_zero = create_ir(IR_ASSIGN, place, zero, NULL);
        destroy_var_list(arg_list);
        return add_code(code1, add_code(write_arg0, place_set_zero));
    } 
    // step 2, generate ARG ir for every temporary variable
    // ARG t1
    // ...
    // ARG tn
    var_list_node_t *cur = arg_list->head;
    int first_arg_ir = 1;
    ir_t *code_args = NULL;
    ir_t *prev_code = NULL;
    while (cur) {
        ir_t *code = create_ir(IR_ARG, NULL, cur->var, NULL);
        if (first_arg_ir) {
            code_args = code;
            prev_code = code;
            first_arg_ir = 0;
        } else {
            prev_code->next = code;
            prev_code = code;
        }
        cur = cur->next;
    }
    ir_t *code_call = create_ir(IR_CALL, place, func, NULL);
    destroy_var_list(arg_list);
    return add_code(code1, add_code(code_args, code_call));
}

static ir_t *generate_exp_id(const node_t *id_exp, const sym_table_t *sym_table, const char *place) {
    // ID
    const node_t *id = id_exp->childs[0];
    const char *addr = NULL;
    const sym_info_t *sym_info = lookup_sym_table(sym_table, id->info);
    char buf[MAX_VAR_NAME_LEN];
    if (sym_info && sym_info->array_type) {
        // is array type
        sprintf(buf, "&%s", id->info);
        addr = buf;
    } else {
        addr = id->info;
    }
    ir_t *ir = create_ir(IR_ASSIGN, place, addr, NULL);
    return ir;
}

static ir_t *generate_exp_assign_op(const node_t *assign, const sym_table_t *sym_table, const char *place) {
    // Exp1 ASSIGNOP Exp2
   const node_t * exp1 = assign->childs[0];
   const node_t * exp2 = assign->childs[2];
   const char *addr = NULL;
   char buf[MAX_VAR_NAME_LEN];
   ir_t *code_array_ref = NULL;
   if (node_childs_type_eq(exp1, 1, Id) && node_childs_type_eq(exp2, 1, Id)) {
        const sym_info_t *info1 = lookup_sym_table(sym_table, exp1->childs[0]->info);
        const sym_info_t *info2 = lookup_sym_table(sym_table, exp2->childs[0]->info);
        if (info1 && info1->array_type && info2 && info2->array_type) {
            // array1 = array2
            ir_t *code_array_assign = NULL;
            char array1_base[MAX_VAR_NAME_LEN];
            char array2_base[MAX_VAR_NAME_LEN];
            sprintf(array1_base, "&%s",info1->name);
            sprintf(array2_base, "&%s",info2->name);
            size_t array1_size = get_array_item_num(info1->array_type);
            size_t array2_size = get_array_item_num(info2->array_type);
            size_t assign_size = ((array1_size<array2_size)?array1_size:array2_size);
            // treat all arrays as 1-dimension array when doing array assignment
            for (size_t i = 0;i<assign_size;++i) {
                char ref1[MAX_VAR_NAME_LEN];
                char ref2[MAX_VAR_NAME_LEN];
                char imm[MAX_VAR_NAME_LEN];
                get_immediate_var_name(i, imm);
                char *p_imm = imm;
                ir_t *code_ref_array1 = generate_array_ref(array1_base, NULL, &p_imm, 1, ref1);
                ir_t *code_ref_array2 = generate_array_ref(array2_base, NULL, &p_imm, 1, ref2);
                ir_t *code_assign = create_ir(IR_ASSIGN, ref1, ref2, NULL);
                code_array_assign = add_code(code_array_assign, add_code(code_ref_array1, add_code(code_ref_array2, code_assign)));
            }
            return code_array_assign;
        }
   }
   if (node_childs_type_eq( exp1, 1, Id)) {
       addr = exp1->childs[0]->info;
   } else if (node_childs_type_eq(exp1, 4, Exp, Lb, Exp, Rb)) {
       code_array_ref = generate_exp_exp_lb_exp_rb(exp1, sym_table, NULL, buf);
       addr = buf;
   }
   // const sym_info_t *sym_info = lookup_sym_table(sym_table, exp1->childs[0]->info);
   char t1[MAX_VAR_NAME_LEN];
   new_temp(t1);
   ir_t *code1 = generate_exp(exp2, sym_table, t1);
   ir_t *code2 = create_ir(IR_ASSIGN, addr, t1, NULL);
   ir_t *code3 = create_ir(IR_ASSIGN, place, addr, NULL);
   return add_code(code_array_ref, add_code(add_code(code1, code2), code3));
}

static ir_t *generate_exp_plus(const node_t *plus_exp, const sym_table_t *sym_table, const char *place) {
    // Exp1 PLUS Exp2
    char t1[MAX_VAR_NAME_LEN];
    new_temp(t1);
    char t2[MAX_VAR_NAME_LEN];
    new_temp(t2);
    const node_t *exp1 = plus_exp->childs[0];
    const node_t *exp2 = plus_exp->childs[2];
    ir_t *code1 = generate_exp(exp1, sym_table, t1);
    ir_t *code2 = generate_exp(exp2, sym_table, t2);
    ir_t *code3 = create_ir(IR_PLUS, place, t1, t2, NULL);
    return add_code(add_code(code1, code2), code3);
}


static ir_t *generate_exp_sub(const node_t *plus_exp, const sym_table_t *sym_table, const char *place) {
    // Exp1 PLUS Exp2
    char t1[MAX_VAR_NAME_LEN];
    new_temp(t1);
    char t2[MAX_VAR_NAME_LEN];
    new_temp(t2);
    const node_t *exp1 = plus_exp->childs[0];
    const node_t *exp2 = plus_exp->childs[2];
    ir_t *code1 = generate_exp(exp1, sym_table, t1);
    ir_t *code2 = generate_exp(exp2, sym_table, t2);
    ir_t *code3 = create_ir(IR_SUB, place, t1, t2, NULL);
    return add_code(add_code(code1, code2), code3);
}

static ir_t *generate_exp_multi(const node_t *exp, const sym_table_t *sym_table, const char *place) {
    // Exp1 STAR Exp2
    char t1[MAX_VAR_NAME_LEN];
    new_temp(t1);
    char t2[MAX_VAR_NAME_LEN];
    new_temp(t2);
    const node_t *exp1 = exp->childs[0];
    const node_t *exp2 = exp->childs[2];
    ir_t *code1 = generate_exp(exp1, sym_table, t1);
    ir_t *code2 = generate_exp(exp2, sym_table, t2);
    ir_t *code3 = create_ir(IR_MULTI, place, t1, t2, NULL);
    return add_code(add_code(code1, code2), code3);
}

static ir_t *generate_exp_div(const node_t *exp, const sym_table_t *sym_table, const char *place) {
    // Exp1 STAR Exp2
    char t1[MAX_VAR_NAME_LEN];
    new_temp(t1);
    char t2[MAX_VAR_NAME_LEN];
    new_temp(t2);
    const node_t *exp1 = exp->childs[0];
    const node_t *exp2 = exp->childs[2];
    ir_t *code1 = generate_exp(exp1, sym_table, t1);
    ir_t *code2 = generate_exp(exp2, sym_table, t2);
    ir_t *code3 = create_ir(IR_DIV, place, t1, t2, NULL);
    return add_code(add_code(code1, code2), code3);
}
static ir_t *generate_exp_minus(const node_t *minus_exp, const sym_table_t *sym_table, const char *place) {
    // MINUS Exp1
    char t1[MAX_VAR_NAME_LEN];
    new_temp(t1);
    const node_t *exp1 = minus_exp->childs[1];
    ir_t *code1 = generate_exp(exp1, sym_table, t1);
    char zero[MAX_VAR_NAME_LEN];
    get_immediate_var_name(0, zero);
    ir_t *code2 = create_ir(IR_SUB, place, zero, t1, NULL);
    return add_code(code1, code2);
}

static ir_t *generate_exp_lp_exp_rp(const node_t *exp, const sym_table_t *sym_table, const char *place) {
    // LP Exp1 RP
    const node_t *exp1 = exp->childs[1];
    ir_t *code = generate_exp(exp1, sym_table, place);
    return code;
}

static ir_t *generate_array_ref(const char *base, const array_type_t *array_type, char *indexes[], size_t len, char *ref) {
    char t[MAX_VAR_NAME_LEN];
    new_temp(t);
    const array_type_t *origin_array_type = array_type;
    ir_t *code_cal_offset = NULL;
    {
        char buf[MAX_VAR_NAME_LEN];
        get_immediate_var_name(0, buf);
        code_cal_offset = create_ir(IR_ASSIGN, t, buf, NULL);
    }
    for (size_t i = 0; i < len; ++i) {
        // t1 = places[i] * array_size
        // t = t + t1
        char t1[MAX_VAR_NAME_LEN];
        new_temp(t1);
        char imm[MAX_VAR_NAME_LEN];
        get_immediate_var_name(get_array_item_num(array_type), imm);
        ir_t *code1 = create_ir(IR_MULTI, t1, indexes[i], imm, NULL);
        code1 = add_code(code1, create_ir(IR_PLUS, t, t, t1, NULL));
        code_cal_offset = add_code(code_cal_offset, code1);
        array_type = array_type?array_type->type:NULL;
    }
    char size_imm[MAX_VAR_NAME_LEN];
    get_immediate_var_name(4, size_imm);
    ir_t *code_mul_size = create_ir(IR_MULTI, t, t, size_imm, NULL);
    char addr[MAX_VAR_NAME_LEN];
    new_temp(addr);
    ir_t *code_addr = create_ir(IR_PLUS, addr, base, t, NULL);
    if (get_array_dimension(origin_array_type) == len - 1) {
        sprintf(ref, "*%s", addr);
    } else {
        strcpy(ref, addr);
    }
     
    
    return add_code(code_cal_offset, add_code(code_mul_size, code_addr));
}

static ir_t *generate_exp_exp_lb_exp_rb(const node_t *exp, const sym_table_t *sym_table, const char *place, char *ref) {
    // Exp1 LB Exp2 RB
    // the result of exp is *txx
    // [Exp, Exp, ..., Exp]
    size_t cnt = 0;
    const array_type_t *array_type = NULL;
    const char *id = NULL;
    for (const node_t *t = exp; ; ) {
        ++cnt;
        t = t->childs[0];
        if (!node_childs_type_eq(t, 4, Exp, Lb, Exp, Rb)) {
            assert(node_childs_type_eq(t, 1, Id));
            id = t->childs[0]->info;
            const sym_info_t *id_info = lookup_sym_table(sym_table, id);
            assert(id_info != NULL && "Cannot find id definition");
            array_type = id_info->array_type;
            break;
        }
    }
    const node_t **exps = malloc(sizeof(const node_t*)*cnt);
    char **places = malloc(cnt * sizeof(char*));
    for (size_t i = 0;i<cnt;++i) {
        places[i] = malloc(MAX_VAR_NAME_LEN*sizeof(char));
    }
    ir_t *code_cal_exps = NULL;
    size_t i = 0;
    for (const node_t *t = exp; ; ) {
        ++i;
        exps[cnt-i] = t->childs[2];
        t = t->childs[0];
        if (!node_childs_type_eq(t, 4, Exp, Lb, Exp, Rb)) {
            assert(node_childs_type_eq(t, 1, Id));
            break;
        }
    }
    for (size_t i = 0; i < cnt; ++i) {
        new_temp(places[i]);
        code_cal_exps = add_code(code_cal_exps, generate_exp(exps[i], sym_table, places[i]));
    }

    char base[MAX_VAR_NAME_LEN];
    strcpy(base, id);
    if (array_type) {
        char buf[MAX_VAR_NAME_LEN];
        sprintf(buf, "&%s", base);
        strcpy(base, buf);
        array_type = array_type->type;
    }
    char value[MAX_VAR_NAME_LEN];
    ir_t *code_array_ref = generate_array_ref(base, array_type, places, cnt, value);
    ir_t* code_assign = create_ir(IR_ASSIGN, place, value, NULL);
    free(exps);
    for (size_t i = 0;i<cnt;++i) {
        free(places[i]);
    }
    free(places);
    if (ref) {
        // write the reference to ref
        strcpy(ref, value);
    }
    return add_code(code_cal_exps, add_code(code_array_ref, code_assign));
}
static ir_t *generate_exp_cond(const node_t *exp,const sym_table_t* sym_table, const char *place) {
    char label1[MAX_LABEL_NAME_LEN];
    char label2[MAX_LABEL_NAME_LEN];
    new_label(label1);
    new_label(label2);
    char imm_zero[MAX_VAR_NAME_LEN];
    get_immediate_var_name(0, imm_zero);
    ir_t *code0 = create_ir(IR_ASSIGN, place, imm_zero, NULL);
    ir_t *code1 = generate_cond(exp, label1, label2, sym_table);
    ir_t *code2 = create_ir(IR_LABEL, NULL, label1, NULL);
    char imm_one[MAX_VAR_NAME_LEN];
    get_immediate_var_name(1, imm_one);
    ir_t *code_assign_one = create_ir(IR_ASSIGN, place, imm_one, NULL);
    ir_t *code_label2 = create_ir(IR_LABEL, NULL, label2, NULL);
    return add_code(code0, add_code(code1, add_code(code2, add_code(code_assign_one, code_label2))));
}
static ir_t *generate_exp(const node_t *exp, const sym_table_t *sym_table, const char *place) {
    if (node_childs_type_eq(exp, 1, Int)) {
        return generate_exp_int(exp, sym_table, place);
    } else if (node_childs_type_eq(exp, 1, Id)) {
        return generate_exp_id(exp, sym_table, place);
    } else if (node_childs_type_eq(exp, 3, Exp, AssignOp, Exp)) {
        return generate_exp_assign_op(exp, sym_table, place);
    } else if (node_childs_type_eq(exp, 3, Exp, Plus, Exp)) {
        return generate_exp_plus(exp, sym_table, place);
    } else if (node_childs_type_eq(exp, 3, Exp, Minus, Exp)) {
        return generate_exp_sub(exp, sym_table, place);
    } else if (node_childs_type_eq(exp, 3, Exp, Star, Exp)) {
       return generate_exp_multi(exp, sym_table, place);
    } else if (node_childs_type_eq(exp, 3, Exp, Div, Exp)) {
        return generate_exp_div(exp, sym_table, place);
    } else if (node_childs_type_eq(exp, 2, Minus, Exp)) {
        return generate_exp_minus(exp, sym_table, place);
    } else if (node_childs_type_eq(exp, 3, Id, Lp, Rp)) {
        return generate_exp_call_no_arg(exp, sym_table, place);
    } else if (node_childs_type_eq(exp, 4, Id, Lp, Args, Rp)) {
        return generate_exp_call_with_args(exp, sym_table, place);
    } else if (node_childs_type_eq(exp, 3, Lp, Exp, Rp)) {
        return generate_exp_lp_exp_rp(exp, sym_table, place);
    } else if (node_childs_type_eq(exp, 4, Exp, Lb, Exp, Rb)) {
        return generate_exp_exp_lb_exp_rb(exp, sym_table, place, NULL);
    } else if (node_childs_type_eq(exp, 3, Exp, Relop, Exp) ||
               node_childs_type_eq(exp, 2, Not, Exp) ||
               node_childs_type_eq(exp, 3, Exp, And, Exp) ||
               node_childs_type_eq(exp, 3, Exp, Or, Exp)
    ) {
       return generate_exp_cond(exp, sym_table, place);
    }  else {
        for (size_t i = 0;i<exp->child_num;++i) {
            printf("%s ", exp->childs[i]->name);
        }
        printf("\n");
        fflush(stdout);
        assert(0 && "Unknown exp");
    }
    return NULL;
}

static ir_t *generate_stmt_exp_semi(const node_t *stmt, const sym_table_t *sym_table) {
    // Exp SEMI
    const node_t *exp = stmt->childs[0];
    char t[MAX_VAR_NAME_LEN];
    new_temp(t);
    return generate_exp(exp, sym_table, t);
}

static ir_t *generate_def_list(const node_t *def_list, const sym_table_t *sym_table) {
    // generate DEC instructions for local vars
    // NOTE: only arrays & structures need DEC instruction
    // there is no need to declare basic type variables
    if (def_list == EMPTY_NODE) {
        return NULL;
    }
    ir_t* code = NULL;
    // DefList -> Def DefList | %empty
    while (def_list->childs[0] != EMPTY_NODE) {
        const node_t *def = def_list->childs[0];
        // Def -> Specifier DecList SEMI 
        {
            const node_t *dec_list = def->childs[1];
            // DecList -> Dec COMMA DecList | Dec
            while (1) {
                const node_t *dec = dec_list->childs[0];
                // handle a single dec
                // Dec -> VarDec | VarDec ASSIGNOP Exp
                const node_t *var_dec = dec->childs[0];
                char name[MAX_VAR_NAME_LEN];
                array_type_t *array_type = NULL; 
                get_array_name_and_type(var_dec, name, &array_type);
                if (array_type !=  NULL) {
                    char size_val[MAX_VAR_NAME_LEN];
                    sprintf(size_val, "%zu", get_array_item_num(array_type)*4);
                    ir_t *code_dec = create_ir(IR_DEC, NULL, name, size_val, NULL);
                    code = add_code(code, code_dec);
                    free_array_type(array_type); 
                }

                if (node_childs_type_eq(dec, 3, VarDec, AssignOp, Exp)) {
                    const node_t *exp = dec->childs[2];
                    ir_t *code_exp = generate_exp(exp, sym_table, name);
                    code = add_code(code, code_exp);
                } 

                if (node_childs_type_eq(dec_list, 3, Dec, Comma, DecList)) {
                    dec_list = dec_list->childs[2];
                } else {
                    break;
                }

            }

        }
        def_list = def_list->childs[1];
    }
    return code;
}

static ir_t *generate_stmt_list(const node_t *stmt_list, const sym_table_t *sym_table) {
    // Stmt StmtList1
    // %empty
    const node_t *stmt = stmt_list->childs[0];
    if (stmt == EMPTY_NODE) {
        return NULL;
    }
    const node_t *stmt_list1 = stmt_list->childs[1];
    ir_t *code1 = generate_stmt(stmt, sym_table);
    ir_t *code2 = generate_stmt_list(stmt_list1, sym_table);
    return add_code(code1, code2);
}

static ir_t *generate_comp_st(const node_t *comp_st, const sym_table_t *sym_table) {
    // LC DefList StmtList RC
    const node_t *def_list = comp_st->childs[1];
    const node_t *stmt_list = comp_st->childs[2];
    ir_t *code1 = generate_def_list(def_list, sym_table);
    ir_t *code2 = generate_stmt_list(stmt_list, sym_table);
    return add_code(code1, code2);
}

static ir_t *generate_stmt_comp_st(const node_t *stmt, const sym_table_t *sym_table) {
    // CompSt
    const node_t *comp_st = stmt->childs[0];
    return generate_comp_st(comp_st, sym_table);
}

static ir_t *generate_stmt_return(const node_t *stmt, const sym_table_t *sym_table) {
    // RETURN Exp SEMI
    char t1[MAX_VAR_NAME_LEN];
    new_temp(t1);
    const node_t *exp = stmt->childs[1];
    ir_t *code1 = generate_exp(exp, sym_table, t1);
    ir_t *code2 = create_ir(IR_RETURN, NULL, t1, NULL);
    return add_code(code1, code2);
}

static ir_t *generate_stmt_if(const node_t *if_stmt, const sym_table_t *sym_table) {
    char label1[MAX_LABEL_NAME_LEN];
    char label2[MAX_LABEL_NAME_LEN];
    new_label(label1);
    new_label(label2);
    const node_t *exp = if_stmt->childs[2];
    const node_t *stmt = if_stmt->childs[4];
    ir_t *code1 = generate_cond(exp, label1, label2, sym_table);
    ir_t *code2 = generate_stmt(stmt, sym_table);
    ir_t *code_label1 = create_ir(IR_LABEL, NULL, label1, NULL);
    ir_t *code_label2 = create_ir(IR_LABEL, NULL, label2, NULL);
    return add_code(add_code(code1, code_label1), add_code(code2, code_label2));
}

static ir_t *generate_stmt_if_else(const node_t *stmt, const sym_table_t *sym_table) {
    // IF LP Exp RP Stmt1 ELSE Stmt2
    char label1[MAX_LABEL_NAME_LEN];
    char label2[MAX_LABEL_NAME_LEN];
    char label3[MAX_LABEL_NAME_LEN];
    new_label(label1);
    new_label(label2);
    new_label(label3);
    const node_t *exp = stmt->childs[2];
    const node_t *stmt1 = stmt->childs[4];
    const node_t *stmt2  = stmt->childs[6];

    ir_t *code1 = generate_cond(exp, label1, label2, sym_table);
    ir_t *code2 = generate_stmt(stmt1, sym_table);
    ir_t *code3 = generate_stmt(stmt2, sym_table);
    ir_t *code_label1 = create_ir(IR_LABEL, NULL, label1, NULL);
    ir_t *code_label2 = create_ir(IR_LABEL, NULL, label2, NULL);
    ir_t *code_label3 = create_ir(IR_LABEL, NULL, label3, NULL);
    ir_t *code_goto_label3 = create_ir(IR_GOTO, NULL, label3, NULL);
    return add_code(
        add_code(
            add_code(
                add_code(code1, code_label1), 
                add_code(code2, code_goto_label3)),
            add_code(code_label2, code3)), code_label3);
}

static ir_t *generate_stmt_while(const node_t *stmt, const sym_table_t *sym_table) {
    char label1[MAX_LABEL_NAME_LEN];
    char label2[MAX_LABEL_NAME_LEN];
    char label3[MAX_LABEL_NAME_LEN];
    new_label(label1);
    new_label(label2);
    new_label(label3);
    const node_t *exp = stmt->childs[2];
    const node_t *stmt1 = stmt->childs[4];
    ir_t *code1 = generate_cond(exp, label2, label3, sym_table);
    ir_t *code2 = generate_stmt(stmt1, sym_table);
    ir_t *code_label1 = create_ir(IR_LABEL, NULL, label1, NULL);
    ir_t *code_label2 = create_ir(IR_LABEL, NULL, label2, NULL);
    ir_t *code_label3 = create_ir(IR_LABEL, NULL, label3, NULL);
    ir_t *code_goto_label1 = create_ir(IR_GOTO, NULL, label1, NULL);
    return add_code(
        code_label1, add_code(
            code1, add_code(
                code_label2, add_code(
                    code2, add_code(code_goto_label1, code_label3)))));
}

static ir_t *generate_stmt(const node_t *stmt, const sym_table_t *sym_table) {
    if (node_childs_type_eq(stmt, 2, Exp, Semi)) {
        return generate_stmt_exp_semi(stmt, sym_table);
    } else if (node_childs_type_eq(stmt, 1, CompSt)) {
        return generate_stmt_comp_st(stmt, sym_table);
    } else if (node_childs_type_eq(stmt, 3, Return, Exp, Semi)) {
        return generate_stmt_return(stmt, sym_table);
    } else if (node_childs_type_eq(stmt, 5, If, Lp, Exp, Rp, Stmt)) {
        return generate_stmt_if(stmt, sym_table);
    } else if (node_childs_type_eq(stmt, 7, If, Lp, Exp, Rp, Stmt, Else, Stmt)) {
        return generate_stmt_if_else(stmt, sym_table);
    } else if (node_childs_type_eq(stmt, 5, While, Lp, Exp, Rp, Stmt)) {
        return generate_stmt_while(stmt, sym_table);
    } else {
        //perror("unknown stmt");
        //exit(EXIT_FAILURE);
    }
    return NULL;
}

static ir_t *generate_param_dec(const node_t *param_dec, const sym_table_t *sym_table) {
    // Specifier VarDec
    const node_t *var_dec = param_dec->childs[1];
    if (node_childs_type_eq(var_dec, 1, Id)) {
        ir_t *code = create_ir(IR_PARAM, NULL, var_dec->childs[0]->info, NULL);
        return code;
    } else if (node_childs_type_eq(var_dec, 4, VarDec, Lb, Int, Rb)) {
        // array param
        const node_t *var_dec1 = var_dec->childs[0];
        assert(node_childs_type_eq(var_dec1, 1, Id));
        ir_t *code = create_ir(IR_PARAM, NULL, var_dec1->childs[0]->info, NULL);
        return code;
    }
    return NULL;
}

static ir_t *generate_var_list(const node_t *var_list, const sym_table_t *sym_table) {
    // ParamDec COMMA VarList 
    // ParamDec
    if (node_childs_type_eq(var_list, 3, ParamDec, Comma, VarList)) {
        const node_t *param_dec = var_list->childs[0];
        const node_t *var_list1 = var_list->childs[2];
        ir_t *code = generate_param_dec(param_dec, sym_table);
        return add_code(code, generate_var_list(var_list1, sym_table));
    } else if (node_childs_type_eq(var_list, 1, ParamDec)) {
        const node_t *param_dec = var_list->childs[0];
        ir_t *code = generate_param_dec(param_dec, sym_table);
        return code;
    } else {
        assert(0 && "UNKNOWN VAR LIST");
    }
}

static ir_t *generate_func_dec_no_arg(const node_t *fun_dec, const sym_table_t *sym_table) {
    // ID LP RP
    const char *func = fun_dec->childs[0]->info;
    ir_t *code = create_ir(IR_FUNCTION, NULL, func, NULL);
    return code;
}

static ir_t *generate_func_dec_with_arg(const node_t *fun_dec, const sym_table_t *sym_table) {
    // ID LP VarList RP
    const char *func = fun_dec->childs[0]->info;
    ir_t *code1 = create_ir(IR_FUNCTION, NULL, func, NULL);
    const node_t *var_list = fun_dec->childs[2];
    ir_t *code2 = generate_var_list(var_list, sym_table);
    return add_code(code1, code2);
}

static ir_t *generate_func_dec(const node_t *fun_dec, const sym_table_t *sym_table) {
    if (node_childs_type_eq(fun_dec, 3, Id, Lp, Rp)) {
        return generate_func_dec_no_arg(fun_dec, sym_table);
    } else if (node_childs_type_eq(fun_dec, 4, Id, Lp, VarList, Rp)) {
        return generate_func_dec_with_arg(fun_dec, sym_table);
    } else {
        assert(0 && "UNKNOWN FUNC DEC");
    }
}

static ir_t *generate_ext_def_func_dec(const node_t *ext_def, const sym_table_t *sym_table) {
    // Specifier FunDec CompSt
    const node_t *fun_dec = ext_def->childs[1];
    const node_t *comp_st = ext_def->childs[2];
    ir_t *code_dec = generate_func_dec(fun_dec, sym_table);
    ir_t *code_comp_st = generate_comp_st(comp_st, sym_table);
    return add_code(code_dec, code_comp_st);
}

static ir_t *generate_ext_def(const node_t *ext_def, const sym_table_t *sym_table) {
    if (node_childs_type_eq(ext_def, 3, Specifier, FunDec, CompSt)) {
        return generate_ext_def_func_dec(ext_def, sym_table);
    } else {
        assert(0 && "UNKNOWN EXT DEF");
    }
}


static void visit_node(syntax_tree_visitor_t *visitor, const node_t *node) {
    if (node == EMPTY_NODE) {
        return;
    }
    syntax_node_types_t type = node->node_type;
    switch(type) {
        case ExtDef:{
            sym_table_t *sym_table = construct_local_sym_table(node);
            ir_t *code = generate_ext_def(node, sym_table);
            visitor->code = add_code(visitor->code, code);
            free_sym_table(sym_table);
            break;
        }
        default: {
            for (size_t i = 0;i<node->child_num;++i) {
                visit_node(visitor, node->childs[i]);
            }
        }
    }
}

// TODO: DIVIDE, ADDR, LOAD, STORE, PARAM

static void write_ir(const ir_t *ir, FILE *f) {
    switch(ir->ir_type) {
        case IR_LABEL:{
            fprintf(f, "LABEL %s :\n", ir->src_names[0]);
            break;
        }
        case IR_ASSIGN:{
            fprintf(f, "%s := %s\n", ir->dest_name, ir->src_names[0]);
            break;
        }
        case IR_PLUS:{
            fprintf(f, "%s := %s + %s\n", ir->dest_name, ir->src_names[0], ir->src_names[1]);
            break;
        }
        case IR_SUB:{
            fprintf(f, "%s := %s - %s\n", ir->dest_name, ir->src_names[0], ir->src_names[1]);
            break;
        }
        case IR_MULTI:{
            fprintf(f, "%s := %s * %s\n", ir->dest_name, ir->src_names[0], ir->src_names[1]);
            break;
        }
        case IR_DIV:{
            fprintf(f, "%s := %s / %s\n", ir->dest_name, ir->src_names[0], ir->src_names[1]);
            break;
        }
        case IR_GOTO:{
            fprintf(f, "GOTO %s\n", ir->src_names[0]);
            break;
        }
        case IR_IFTHENGOTO:{
            fprintf(f, "IF %s %s %s GOTO %s\n", ir->src_names[0], ir->src_names[1], ir->src_names[2], ir->src_names[3]);
            break;
        }
        case IR_RETURN:{
            fprintf(f, "RETURN %s\n", ir->src_names[0]);
            break;
        }
        case IR_ARG:{
            fprintf(f, "ARG %s\n", ir->src_names[0]);
            break;
        }
        case IR_CALL:{
            if (!ir->dest_name) {
                fprintf(f, "CALL %s\n", ir->src_names[0]);
            } else {
                fprintf(f, "%s := CALL %s\n", ir->dest_name, ir->src_names[0]);
            }
            break;
        }
        case IR_READ:{
            fprintf(f, "READ %s\n", ir->dest_name);
            break;
        }
        case IR_WRITE:{
            fprintf(f, "WRITE %s\n", ir->src_names[0]);
            break;
        }
        case IR_FUNCTION:{
            fprintf(f, "FUNCTION %s :\n", ir->src_names[0]);
            break;
        }
        case IR_PARAM:{
            fprintf(f, "PARAM %s\n", ir->src_names[0]);
            break;
        }
        case IR_DEC:{
            fprintf(f, "DEC %s %s\n", ir->src_names[0], ir->src_names[1]);
            break;
        }
        default:{
            assert(0 && "THERE IS AN UNHANDLED IR");
        }
    }
}
void visit_tree(syntax_tree_visitor_t *visitor) {
    visit_node(visitor, visitor->tree);
    {
        // ir_t *cur = visitor->code;
        // while (cur != NULL) {
        //     write_ir(cur, visitor->out);
        //     cur = cur->next;
        // }
        code_gen(visitor);
    }
    {
        ir_t *cur = visitor->code;
        while (cur != NULL) {
            ir_t *t = cur;
            cur = cur->next;
            free_ir(t);
        }
    }
}

/**
Assumptions:
    1. only local variables 
    2. int *ptr only in param while int[] arrays only in local vars

Solutions:
    1. construct local symbol table for functions only
    2. provide infomation that id -> type
    3. type -> (type, ptr) or (dimension, type) in CompSt from ExtDef
    4. (type, ptr) -> (int, ptr) in VarLists from FunDec
    5. id->(int, ptr) -- id[x] -> *(id + sizeof(int)*x)
    6. id->(dimension, type) -- *(&id + sizeof(type)*dimension)
 */

int sym_info_compare(const void *a, const void *b, void *udata) {
    const sym_info_t *ua = a;
    const sym_info_t *ub = b;
    return strcmp(ua->name, ub->name);
}

uint64_t sym_info_hash(const void *item, uint64_t seed0, uint64_t seed1) {
    const sym_info_t *info = item;
    return hashmap_sip(info->name, strlen(info->name), seed0, seed1);
}

static void free_sym_info(sym_info_t *info);

static void elfree_sym_info(void *info) {
    free_sym_info(info);
}

static sym_table_t *allocate_sym_table() {

    sym_table_t *sym_table = malloc(sizeof(sym_table_t));
    sym_table->map = hashmap_new(sizeof(sym_info_t),
     0,
      0,
       0,
        sym_info_hash,
         sym_info_compare,
          elfree_sym_info,
           NULL);
    return sym_table;
}

static void free_sym_table(sym_table_t *sym_table) {
    hashmap_free(sym_table->map);
    free(sym_table);
}

static void add_entries_in_fun_dec(const node_t *fun_dec, sym_table_t *sym_table) {
    if (!node_childs_type_eq(fun_dec, 4, Id, Lp, VarList, Rp)) {
        return;
    }
    const node_t *var_list = fun_dec->childs[2];
    // VarList -> ParamDec COMMA VarList | ParamDec
    while (1) {
        const node_t *param_dec = var_list->childs[0];
        // ParamDec -> Specifier VarDec
        const node_t *specifier = param_dec->childs[0];
        if (!node_childs_type_eq(specifier, 1, TypeN)) {
            // struct param, error!
            assert(0 && "struct param!");
        }
        const node_t *var_dec = param_dec->childs[1];
        if (node_childs_type_eq(var_dec, 1, Id)) {
            // do nothing
        } else if (node_childs_type_eq(var_dec, 4, VarDec, Lb, Int, Rb)) {
            // this param is a ptr, repersenting the start of a 1 dimension array
            sym_info_t info;
            const node_t *var_dec1 = var_dec->childs[0];
            const node_t *id = var_dec1->childs[0];
            info.array_type = NULL;
            strcpy(info.name, id->info);
            hashmap_set(sym_table->map, &info);
        }

        if(node_childs_type_eq(var_list, 3, ParamDec, Comma, VarList)) {
            var_list = var_list->childs[2];
        } else {
            break;
        }
    }
}

static array_type_t *get_array_name_and_type(const node_t *var_dec, /*out*/char *name, /*out*/array_type_t **p_array_type) {
    // VarDec -> VarDec1 LB INT RB | ID
    if (node_childs_type_eq(var_dec, 1, Id)) {
        const node_t *id = var_dec->childs[0];
        strcpy(name, id->info);
        return NULL;
    } else if (!node_childs_type_eq(var_dec, 4, VarDec, Lb, Int, Rb)) {
        assert(0);
    }
    const node_t *var_dec1 = var_dec->childs[0];
    array_type_t *array_type = malloc(sizeof(array_type_t));
    const node_t *node_int = var_dec->childs[2];
    array_type->dimension = atoi(node_int->info);
    array_type->type = NULL;
    if (node_childs_type_eq(var_dec1, 1, Id)) {
        array_type_t *retv = NULL;
        get_array_name_and_type(var_dec1, name, &retv);
        assert(retv == NULL);
        if (p_array_type) {
            *p_array_type = array_type;
        }
        return array_type;
    }
    array_type_t *retv = NULL;
    array_type_t *parent_array_type = get_array_name_and_type(var_dec1, name, &retv);
    assert(parent_array_type->type == NULL);
    parent_array_type->type = array_type;
    if (p_array_type) {
        *p_array_type = retv;
    }
    return array_type;
}

static size_t get_array_item_num(const array_type_t *array_type) {
    // (8, (6, NULL)) -> 8*6 = 48 (items in [8][6])
    // (6, NULL) -> 6 (items in [6])
    // NULL -> 1 (means 1 item)
    if (array_type == NULL) {
        return 1;
    }
    return array_type->dimension * get_array_item_num(array_type->type);
}

static size_t get_array_dimension(const array_type_t *array_type) {
    // (8, (6, NULL)) -> (means 2d)
    // (6, NULL) -> 6 (means 1d)
    // NULL -> 0 (means 0d)
    if (array_type == NULL) {
        return 0;
    }
    return 1 + get_array_dimension(array_type->type);
}
static void free_array_type(array_type_t *array_type) {
    while (array_type) {
        array_type_t * t = array_type->type;
        free(array_type);
        array_type = t;
    }
}

static void free_sym_info(sym_info_t *info) {
    free_array_type(info->array_type);
    // free(info);
}

static void add_entries_in_comp_st(const node_t *comp_st, sym_table_t *sym_table) {
    // CompSt -> LC DefList StmtList RC 
    {
        const node_t *def_list = comp_st->childs[1];
        // DefList -> Def DefList | %empty
        while (def_list->childs[0] != EMPTY_NODE) {
            const node_t *def = def_list->childs[0];
            // Def -> Specifier DecList SEMI 
            {
                const node_t *dec_list = def->childs[1];
                // DecList -> Dec COMMA DecList | Dec
                while (1) {
                    const node_t *dec = dec_list->childs[0];
                    // handle a single dec
                    // Dec -> VarDec ...
                    const node_t *var_dec = dec->childs[0];
                    sym_info_t info;
                    array_type_t *array_type = NULL;
                    get_array_name_and_type(var_dec, info.name, &array_type);
                    if (array_type) {
                        // not a basic variable (i.e int a; a is a basic variable)
                        info.array_type = array_type;
                        hashmap_set(sym_table->map, &info);
                    }


                    if (node_childs_type_eq(dec_list, 3, Dec, Comma, DecList)) {
                        dec_list = dec_list->childs[2];
                    } else {
                        break;
                    }

                }

            }
            def_list = def_list->childs[1];
        }
    }
    // const node_t *stmt_list = comp_st->childs[2];
    // while (stmt_list->childs[0] != EMPTY_NODE) {
        // const node_t *stmt = stmt_list->childs[0];
        // if (node_childs_type_eq(stmt, 1, CompSt)) {
            // const node_t *comp_st1 = stmt->childs[0];
            // assert(comp_st1->node_type == CompSt);
            // add_entries_in_comp_st(comp_st1, sym_table);
        // }
        // stmt_list = stmt_list->childs[1];
    // }
}

static void construct_local_sym_table_dfs(const node_t *node, sym_table_t *sym_table) {
    if (node == EMPTY_NODE || node == NULL) {
        return;
    }
    if (node->node_type == FunDec) {
        add_entries_in_fun_dec(node, sym_table);
    } else if (node->node_type == CompSt) {
        add_entries_in_comp_st(node, sym_table);
    }
    for (size_t i = 0; i < node->child_num; ++i) {
        construct_local_sym_table_dfs(node->childs[i], sym_table);
    }
}

static sym_table_t *construct_local_sym_table(const node_t *ext_def) {
    assert(node_childs_type_eq(ext_def, 3, Specifier, FunDec, CompSt));
    const node_t *fun_dec = ext_def->childs[1];
    const node_t *comp_st = ext_def->childs[2];
    sym_table_t *sym_table = allocate_sym_table();
    // add_entries_in_fun_dec(fun_dec, sym_table);
    // add_entries_in_comp_st(comp_st, sym_table);
    construct_local_sym_table_dfs(ext_def, sym_table);
    return sym_table;
 }
