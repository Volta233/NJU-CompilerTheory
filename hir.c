#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include "hir.h"
// for using strdup in C99
char* strdup(const char*);

ir_t *add_code(ir_t *code1, ir_t *code2) {
    // ir generated from %empty node is NULL
    if (code1 == NULL) {
        return code2;
    }
    if (code2 == NULL) {
        return code1;
    }
    ir_t* cur = code1;
    while (cur->next != NULL) {
        cur = cur->next;
    }
    cur->next = code2;
    return code1;
}

ir_t *create_ir(ir_types_t ir_type, const char *dest_name, ...) {
    if (ir_type == IR_ASSIGN && dest_name == NULL) {
        // no need to generate ir for an unused assignment
        // TODO: handle this in liveness analysis
        // for this propose, pass temp insteading of NULL in generate_stmt_exp_semi
        return NULL;
    }
    int count = 0;
    va_list args; 
    va_start(args, dest_name);
    for (const char *src_name = va_arg(args, const char *); src_name != NULL; src_name = va_arg(args, const char *)) {
        ++count;
    }
    va_end(args);

    ir_t *ir = malloc(sizeof(ir_t) + sizeof(char *)*count);
    // dest_name can be NULL
    // if so, lhs value is ignored
    if (dest_name == NULL) {
        ir->dest_name = NULL;
    } else {
        ir->dest_name = strdup(dest_name);
    }
    ir->ir_type = ir_type;
    ir->num_of_srcs = count;
    ir->next = NULL;
    {
        int i = 0;
        va_start(args, dest_name);
        for (const char *src_name = va_arg(args, const char *); src_name != NULL; src_name = va_arg(args, const char *), ++i) {
            ir->src_names[i] = strdup(src_name);
        }
        va_end(args);
    }
    return ir;
}

void free_ir(ir_t *ir) {
    for (int i = 0; i < ir->num_of_srcs; ++i) {
        free(ir->src_names[i]);
    }
    free(ir->dest_name);
    free(ir);
}