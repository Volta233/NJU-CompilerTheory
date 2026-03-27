#ifndef __HIR_H__
#define __HIR_H__
#include <stdint.h>

#define MAX_VAR_NAME_LEN 64
#define MAX_LABEL_NAME_LEN 64
typedef struct ir_s ir_t;
typedef enum ir_types {
    IR_ASSIGN, // dest := src0
    IR_PLUS,   // dest := src0 + src1
    IR_SUB,  // dest := src0 - src1
    IR_MULTI,  // dest := src0 * src1
    IR_DIV,  // dest := src0 / src1
    IR_RETURN, // RETURN src0 (no dest)
    IR_LABEL, // LABEL src0 (no dest)
    IR_GOTO, // GOTO src0 (no dest)
    IR_IFTHENGOTO,     // IF src0 src1(op) src2 GOTO src3
    IR_READ,   // READ dest (no src)
    IR_WRITE,  // WRITE src0 (no dest)
    IR_CALL,   // CALL src0 (no dest)
    IR_ARG,    // ARG src0 (no dest)
    IR_FUNCTION, // FUNCTION src0 (no dest)
    IR_PARAM,  // PARAM src0 (no dest)
    IR_DEC,    // DEC src0 src1(size) (no dest)
} ir_types_t;
struct ir_s {
    ir_types_t ir_type;  
    uint32_t num_of_srcs;
    char *dest_name;
    ir_t *next;
    char *src_names[0];
};


ir_t *add_code(ir_t *code1, ir_t *code2);
ir_t *create_ir(ir_types_t ir_type, const char *dest_name, ...);
void free_ir(ir_t *ir);
#endif