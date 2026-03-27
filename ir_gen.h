#ifndef __IR_GEN_H__
#define __IR_GEN_H__
#include <stdint.h>
#include "node.h"
#include "hashmap.h"
#include "hir.h"

typedef struct sym_info_s sym_info_t;
typedef struct array_type_s array_type_t;
typedef struct sym_table_s sym_table_t;

struct sym_info_s {
    char name[MAX_VAR_NAME_LEN];
    // if array_type is NULL then this id is a ptr
    array_type_t *array_type;
};

struct array_type_s {
    size_t dimension;
    array_type_t *type;
};

struct sym_table_s {
    struct hashmap *map;
};


typedef struct syntax_tree_visitor_s syntax_tree_visitor_t;

struct syntax_tree_visitor_s {
    const node_t *tree;
    ir_t *code;
    sym_table_t *sym_table;
    FILE *out;
};

void visit_tree(syntax_tree_visitor_t *visitor);

const sym_info_t *lookup_sym_table(const sym_table_t *sym_table, const char *name);
const ir_t *generate_ir(const node_t *syntax_tree, const sym_table_t *sym_table);

#endif