#include "node.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

static const struct {
    const char *name;
    syntax_node_types_t type;
} synatx_name_type_mappings[] = {
{"ID",   Id},
{"PLUS",   Plus},
{"SEMI",   Semi},
{"COMMA",   Comma},
{"ASSIGNOP",   AssignOp},
{"RELOP",   Relop},
{"MINUS",   Minus},
{"STAR",   Star},
{"DIV",   Div},
{"AND",   And},
{"OR",   Or},
{"DOT",   Dot},
{"NOT",   Not},
{"LP",   Lp},
{"RP",   Rp},
{"LB",   Lb},
{"RB",   Rb},
{"LC",   Lc},
{"RC",   Rc},
{"STRUCT",   Struct},
{"IF",   If},
{"ELSE",   Else},
{"RETURN",   Return},
{"WHILE",   While},
{"TYPE",   TypeN},
{"INT",   Int},
{"FLOAT",   Float},
{"Program",   Program},
{"ExtDefList",   ExtDefList },
{"ExtDef",   ExtDef },
{"ExtDecList",   ExtDecList },
{"Specifier",   Specifier},
{"StructSpecifier",   StructSpecifier },
{"OptTag",   OptTag },
{"Tag",   Tag },
{"VarDec",   VarDec },
{"FunDec",   FunDec },
{"VarList",   VarList },
{"ParamDec",   ParamDec },
{"CompSt",   CompSt },
{"StmtList",   StmtList },
{"Stmt",   Stmt },
{"DefList",   DefList },
{"Def",   Def },
{"DecList",   DecList },
{"Dec",   Dec },
{"Exp",   Exp },
{"Args",   Args},
{NULL, -1},
};

syntax_node_types_t get_syntax_node_type(const char *type_name) {
    for (int i = 0; synatx_name_type_mappings[i].name != NULL; ++i) {
        if (strcmp(synatx_name_type_mappings[i].name, type_name) == 0) {
            return synatx_name_type_mappings[i].type;
        }
    }
    return -1;
}
node_t* create_node(char *name, char *info, int line, ...) {
    // count the number of child nodes
    int count = 0;
    va_list args;
    va_start(args, line);
    for (node_t *child = va_arg(args, node_t*); child != NULL;child = va_arg(args, node_t*)) {
        ++count;
    }
    va_end(args);
    // initialize
    node_t *node = malloc(sizeof(node_t) + count*sizeof(node_t*));
    strcpy(node->name, name);
    node->node_type = get_syntax_node_type(name);
    assert(node->node_type != -1);
    strcpy(node->info, info);
    node->line = line;
    node->child_num = count;
    {
        int i = 0;
        va_start(args, line);
        for (node_t *child = va_arg(args, node_t*); child != NULL;child = va_arg(args, node_t*),++i) {
            node->childs[i] = child;
        }
        va_end(args);
    }
    return node;
}

void print_node(node_t *node, int dep) {
    if (node == NULL) {
        return;
    }
    if (node->child_num > 0) {
        // syntax unit
        if (node->childs[0] == EMPTY_NODE) {
            // in case of producing empty
            return;
        } else {
            for (int i = 0;i<dep;++i) {
                printf("  ");
            }
            printf("%s (%d)", node->name, node->line);
        }
    } else {
        for (int i = 0;i<dep;++i) {
            printf("  ");
        }
        // lexical unit
        printf("%s", node->name);
        char *name = node->name;
        if (strcmp(name, "ID") == 0) {
            printf(": %s", node->info);
        } else if (strcmp(name, "TYPE") == 0) {
            printf(": %s", node->info);
        } else if (strcmp(name, "INT") == 0) {
            printf(": %d", atoi(node->info));
        } else if (strcmp(name, "FLOAT") == 0) {
            printf(": %f", strtof(node->info, NULL));
        }
    }
    printf("\n");
    for (int i = 0;i<node->child_num;++i) {
        print_node(node->childs[i], dep + 1);
    }
}

void print_tree(node_t *tree) {
    print_node(tree, 0);
}

void destroy_node(node_t *node) {
    if (node == NULL) {
        return;
    }
    for (int i = 0;i<node->child_num;++i) {
        if (node->childs[i] != EMPTY_NODE) {
            destroy_node(node->childs[i]);
        }
    }
    free(node);
} 
void destroy_tree(node_t *tree) {
    destroy_node(tree);
}