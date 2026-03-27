#ifndef __NODE_H__
#define __NODE_H__
#include <stdio.h>
#include <string.h>

#define MAX_NAME_LEN 64
#define MAX_INFO_LEN 64

typedef enum syntax_node_types {
  Id,
  Plus,
  Semi,
  Comma,
  AssignOp,
  Relop,
  Minus,
  Star,
  Div,
  And,
  Or,
  Dot,
  Not,
  Lp,
  Rp,
  Lb,
  Rb,
  Lc,
  Rc,
  Struct,
  If,
  Else,
  Return,
  While,
  TypeN,
  Int,
  Float,
  Program,
  ExtDefList ,
  ExtDef ,
  ExtDecList ,
  Specifier,
  StructSpecifier ,
  OptTag ,
  Tag ,
  VarDec ,
  FunDec ,
  VarList ,
  ParamDec ,
  CompSt ,
  StmtList ,
  Stmt ,
  DefList ,
  Def ,
  DecList ,
  Dec ,
  Exp ,
  Args
} syntax_node_types_t;

/**
 * @brief A structure representing a node in a syntax tree.
 *
 * The `node_t` structure is used to store information about a node in a syntax tree,
 * including its name, additional information, line number, the number of child nodes,
 * and a flexible array member for storing pointers to child nodes.
 *
 * @note The flexible array member `childs[]` allows the structure to be used
 *       in scenarios where the number of child nodes is not known at compile time.
 *
 * @warning The structure should be used with caution, as improper use of the
 *          flexible array member may lead to memory allocation issues.
 */
typedef struct node_s {
    /**
     * @brief The name of the syntax tree node.
     *
     * This field stores the name of the node, which can be used for identification
     * or other purposes.
     */
    char name[MAX_NAME_LEN];

    /**
     * Enmulator type decided by name
     */
    syntax_node_types_t node_type;
    /**
     * @brief Additional information associated with the syntax tree node.
     *
     * This field stores additional information or metadata related to the node.
     */
    char info[MAX_INFO_LEN];

    /**
     * @brief The line number where the syntax tree node is created.
     *
     * This field stores the line number in the source code where the node is created.
     */
    int line;

    /**
     * @brief The number of child nodes in the syntax tree.
     *
     * This field stores the number of child nodes that this node has.
     */
    int child_num;

    /**
     * @brief A flexible array member for storing pointers to child nodes.
     *
     * This field allows the structure to dynamically allocate memory for child nodes.
     * The number of child nodes is determined at runtime.
     */
    struct node_s *childs[];
} node_t;

/**
  * node for empty
 */
#define EMPTY_NODE ((node_t*)-1)
/**
 * @brief Creates a new node with the given name, info, line number, and variable number of child nodes.
 *
 * This function dynamically allocates memory for a new node and initializes its fields.
 * The node can have a variable number of child nodes, which are passed as a variable argument list.
 * The child nodes are terminated by a NULL pointer.
 *
 * @param[in] name The name of the node.
 * @param[in] info Additional information associated with the node.
 * @param[in] line The line number where the node is created.
 * @param[in] ... A variable number of child nodes, terminated by a NULL pointer.
 *
 * @return A pointer to the newly created node.
 *
 * @note The caller is responsible for freeing the memory allocated for the node and its children.
 *
 * @warning The function assumes that the memory for the node and its children is properly managed.
 *          Improper use may lead to memory leaks or undefined behavior.
 */
node_t* create_node(char *name, char *info, int line, ...);
extern node_t *root;

void destroy_tree(node_t *node);

void print_tree(node_t *node);
#endif