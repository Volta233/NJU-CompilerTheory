#ifndef __CODE_GEN_H__
#define __CODE_GEN_H__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir_gen.h"
#include "reg.h"
#include "basicblock.h"

void code_gen(syntax_tree_visitor_t *visitor);
int atoi(const char *str);


#endif