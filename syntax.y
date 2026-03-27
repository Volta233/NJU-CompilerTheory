%{
#include "node.h"
#include "sign_table.h"
#include "lex.yy.c"    
node_t *root;
stack_st sign_table;
int error_cnt = 0;
int lexical_error = 0;
void yyerror(const char *msg) {
    ++error_cnt;
    if (!lexical_error) {
    printf("Error type B at Line %d: %s.\n", yylineno, msg);
    }
}
%}

/*declared types*/
/*this type is called yylval*/
%union {
    node_t *node_val;
}

/*declared tokens*/
%token <node_val>  ID
%token <node_val> PLUS
%token <node_val> SEMI
%token <node_val> COMMA
%token <node_val> ASSIGNOP
%token <node_val> RELOP
%token <node_val> MINUS
%token <node_val> STAR
%token <node_val> DIV
%token <node_val> AND
%token <node_val> OR
%token <node_val> DOT
%token <node_val> NOT
%token <node_val> LP
%token <node_val> RP
%token <node_val> LB
%token <node_val> RB
%token <node_val> LC
%token <node_val> RC
%token <node_val> STRUCT
%token <node_val> IF
%token <node_val> ELSE
%token <node_val> RETURN
%token <node_val> WHILE
%token <node_val> TYPE
%token <node_val>  INT
%token <node_val>  FLOAT

/*associativity and precedence descriptions*/
%right ASSIGNOP // =
%left OR // ||
%left AND // &&
%left RELOP // < > <= >= ==
%left MINUS PLUS //- +
%left DIV STAR // / *
%right UMINUS NOT // - !
%left DOT LP RP LB RB // . ( ) [ ]

%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

/* declared types*/
%type <node_val> Program ExtDefList ExtDef ExtDecList Specifier 
%type <node_val> StructSpecifier OptTag Tag VarDec FunDec VarList ParamDec CompSt StmtList Stmt DefList Def DecList Dec Exp Args
%%
/*declared syntaxs*/

/*high-level definitions*/
Program
    : ExtDefList {$$=create_node("Program", "", @$.first_line, $1, NULL); root=$$;}
    | error {}
    ;

ExtDefList
    : ExtDef ExtDefList {$$=create_node("ExtDefList", "", @$.first_line, $1, $2, NULL);}
    | %empty {$$=create_node("ExtDefList", "", @$.first_line, EMPTY_NODE, NULL);}
    | error {}
    ;

ExtDef
    : Specifier ExtDecList SEMI {$$=create_node("ExtDef", "",@$.first_line, $1, $2, $3, NULL);}
    | Specifier SEMI {$$=create_node("ExtDef", "",@$.first_line, $1, $2, NULL);}
    | Specifier FunDec CompSt {$$=create_node("ExtDef", "",@$.first_line, $1, $2, $3, NULL);}
    | error {}
    ;

ExtDecList
    : VarDec {$$=create_node("ExtDecList", "", @$.first_line, $1, NULL);}
    | VarDec COMMA ExtDecList {$$=create_node("ExtDecList", "", @$.first_line, $1, $2, $3, NULL);}
    | error {}
    ;
/*specifiers*/

Specifier
    : TYPE {$$=create_node("Specifier", "", @$.first_line, $1, NULL);}
    | StructSpecifier {$$=create_node("Specifier", "", @$.first_line, $1, NULL);}
    | error ID {}
    ;

StructSpecifier
    : STRUCT OptTag LC DefList RC {$$=create_node("StructSpecifier", "", @$.first_line, $1, $2, $3, $4, $5, NULL);}
    | STRUCT Tag {$$=create_node("StructSpecifier", "", @$.first_line, $1, $2, NULL);}
    | error RC {}
    ;

OptTag
    : ID {$$=create_node("OptTag", "", @$.first_line, $1, NULL);}
    | %empty {$$=create_node("OptTag", "", @$.first_line, EMPTY_NODE, NULL);}
    | error {}
    ;

Tag
    : ID {$$=create_node("Tag", "", @$.first_line, $1, NULL);}
    | error INT ID {}
    ;

/*declarators*/

VarDec
    : ID {$$=create_node("VarDec", "", @$.first_line, $1, NULL);}
    | VarDec LB INT RB {$$=create_node("VarDec", "", @$.first_line, $1, $2, $3, $4, NULL);}
    ;

FunDec
    : ID LP VarList RP {$$=create_node("FunDec", "", @$.first_line, $1, $2, $3, $4, NULL);}
    | ID LP RP {$$=create_node("FunDec", "", @$.first_line, $1, $2, $3, NULL);}
    | error RP {}
    | error RB {}
    | error INT ID {}
    ;

VarList
    : ParamDec COMMA VarList {$$=create_node("VarList", "", @$.first_line, $1, $2, $3, NULL);}
    | ParamDec {$$=create_node("VarList", "", @$.first_line, $1, NULL);}
    | error {}
    ;

ParamDec
    : Specifier VarDec {$$=create_node("ParamDec", "", @$.first_line, $1, $2, NULL);}
    | error {}
    ;

/*statements*/

CompSt
    : LC DefList StmtList RC {$$=create_node("CompSt", "", @$.first_line, $1, $2, $3, $4, NULL);}
    | error RC {}
    ;

StmtList
    : Stmt StmtList {$$=create_node("StmtList", "", @$.first_line, $1, $2, NULL);}
    | %empty {$$=create_node("StmtList", "", @$.first_line, EMPTY_NODE, NULL);}
    | error {}
    ;

Stmt
    : Exp SEMI {$$=create_node("Stmt", "", @$.first_line, $1, $2, NULL);}
    | CompSt {$$=create_node("Stmt", "", @$.first_line, $1, NULL);}
    | RETURN Exp SEMI {$$=create_node("Stmt", "", @$.first_line, $1, $2, $3, NULL);}
    | IF LP Exp RP Stmt %prec LOWER_THAN_ELSE // eliminate conflict if ... if ... else ... to if ... (if ... else ...)
        {$$=create_node("Stmt", "", @$.first_line, $1, $2, $3, $4, $5, NULL);}
    | IF LP Exp RP Stmt ELSE Stmt {$$=create_node("Stmt", "", @$.first_line, $1, $2, $3, $4, $5, $6, $7, NULL);}
    | WHILE LP Exp RP Stmt {$$=create_node("Stmt", "", @$.first_line, $1, $2, $3, $4, $5, NULL);}
    | error SEMI {}
    | error LC {}
    | error LP {}
    | WHILE LP error Stmt {}
    | WHILE error RP Stmt {}
    | WHILE error Stmt {}
    | error {}
    ;

/* Local Definitions */


DefList
    : Def DefList {$$=create_node("DefList", "", @$.first_line, $1, $2, NULL);}
    | %empty {$$=create_node("DefList", "", @$.first_line, EMPTY_NODE, NULL);}
    | error {}
    ;

Def
    : Specifier DecList SEMI {$$=create_node("Def", "", @$.first_line, $1, $2, $3, NULL);}
    | error SEMI {}
    ;

DecList
    : Dec {$$=create_node("DecList", "", @$.first_line, $1, NULL);}
    | Dec COMMA DecList {$$=create_node("DecList", "", @$.first_line, $1, $2, $3, NULL);}
    | error {}
    ;

Dec
    : VarDec {$$=create_node("Dec", "", @$.first_line, $1, NULL);}
    | VarDec ASSIGNOP Exp {$$=create_node("Dec", "", @$.first_line, $1, $2, $3, NULL);}
    | error {}
    ;


/* Expressions */

Exp 
    : Exp ASSIGNOP Exp {$$=create_node("Exp", "", @$.first_line, $1, $2, $3, NULL);}
    | Exp AND Exp {$$=create_node("Exp", "", @$.first_line, $1, $2, $3, NULL);}
    | Exp OR Exp {$$=create_node("Exp", "", @$.first_line, $1, $2, $3, NULL);}
    | Exp RELOP Exp {$$=create_node("Exp", "", @$.first_line, $1, $2, $3, NULL);}
    | Exp PLUS Exp {$$=create_node("Exp", "", @$.first_line, $1, $2, $3, NULL);}
    | Exp MINUS Exp {$$=create_node("Exp", "", @$.first_line, $1, $2, $3, NULL);}
    | Exp STAR Exp {$$=create_node("Exp", "", @$.first_line, $1, $2, $3, NULL);}
    | Exp DIV Exp {$$=create_node("Exp", "", @$.first_line, $1, $2, $3, NULL);}
    | LP Exp RP {$$=create_node("Exp", "", @$.first_line, $1, $2, $3, NULL);}
    | MINUS Exp %prec UMINUS // %prec UMINUS Specify that the precedence of this production is equivalent to that of a terminal symbol UMINUS.
        {$$=create_node("Exp", "", @$.first_line, $1, $2, NULL);}
    | NOT Exp {$$=create_node("Exp", "", @$.first_line, $1, $2, NULL);}
    | ID LP Args RP {$$=create_node("Exp", "", @$.first_line, $1, $2, $3, $4, NULL);}
    | ID LP RP {$$=create_node("Exp", "", @$.first_line, $1, $2, $3, NULL);}
    | Exp LB Exp RB {$$=create_node("Exp", "", @$.first_line, $1, $2, $3, $4, NULL);}
    | Exp DOT ID {$$=create_node("Exp", "", @$.first_line, $1, $2, $3, NULL);}
    | ID {$$=create_node("Exp", "", @$.first_line, $1, NULL);}
    | INT {$$=create_node("Exp", "", @$.first_line, $1, NULL);}
    | FLOAT {$$=create_node("Exp", "", @$.first_line, $1, NULL);}
    | error {}
    ;

Args
    : Exp {$$=create_node("Args", "", @$.first_line, $1, NULL);}
    | Exp COMMA Args {$$=create_node("Args", "", @$.first_line, $1, $2, $3, NULL);}
    | error {}
    ;

%%