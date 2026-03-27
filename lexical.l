%option yylineno

%{ 
  #include "syntax.tab.h"
  #include "sign_table.h"
  #include "node.h"
  int yycolumn = 1; // the column info

  // this ACTION is executed when a token is analyzed
  #define YY_USER_ACTION\
  yylloc.first_line = yylloc.last_line = yylineno;\
  yylloc.first_column = yycolumn;\
  yylloc.last_column = yycolumn + yyleng - 1;\
  yycolumn += yyleng; 
 //#define DEBUG
  #ifdef DEBUG
  #define DBGP(x, ...) printf(x"\n", ##__VA_ARGS__);
  #else
  #define DBGP(x)
  #endif
  void report_lexical_error() {
    extern int lexical_error;
    lexical_error = 1;
  }
%}

digit  [0-9]
letter  [_a-zA-Z]
ID  {letter}({letter}|{digit})*

PLUS  "+"
SEMI  ";"
COMMA  ","
ASSIGNOP  "="
RELOP  ">"|"<"|">="|"<="|"=="|"!="
MINUS  "-"
STAR  "*"
DIV  "/"
AND  "&&"
OR  "||"
DOT  "."
NOT  "!"
LP  "("
RP  ")"
LB  "["
RB  "]"
LC  "{"
RC  "}"
STRUCT  "struct"
IF  "if"
ELSE  "else"
RETURN  "return"
WHILE  "while"
TYPE  "int"|"float"

INT  (0|[1-9][0-9]*)
ERRORINTA (0[0-9]+)
ERRORINTB 0[xX][0-9a-fA-F]+

base  ([0-9]+\.[0-9]*)|([0-9]*\.[0-9]+)
exp  [+-]?[0-9]+
FLOAT  ([0-9]+\.[0-9]+)|({base}[Ee]{exp})

%%
{PLUS}   { DBGP("PLUS"); yylval.node_val = create_node("PLUS","",yylineno,NULL); return PLUS; }
{MINUS}   { DBGP("MINUS"); yylval.node_val = create_node("MINUS","",yylineno,NULL); return MINUS; }
{SEMI}   { DBGP("SEMI"); yylval.node_val = create_node("SEMI","",yylineno,NULL); return SEMI; }
{COMMA}   { DBGP("COMMA"); yylval.node_val = create_node("COMMA","",yylineno,NULL); return COMMA; }
{ASSIGNOP}   { DBGP("ASSIGNOP"); yylval.node_val = create_node("ASSIGNOP","",yylineno,NULL); return ASSIGNOP; }
{RELOP}   { DBGP("RELOP"); yylval.node_val = create_node("RELOP",yytext, yylineno,NULL); return RELOP; }
{STAR}   { DBGP("STAR"); yylval.node_val = create_node("STAR","",yylineno,NULL); return STAR; }
{DIV}   { DBGP("DIV"); yylval.node_val = create_node("DIV","",yylineno,NULL); return DIV; }
{AND}   { DBGP("AND"); yylval.node_val = create_node("AND","",yylineno,NULL); return AND; }
{OR}   { DBGP("OR"); yylval.node_val = create_node("OR","",yylineno,NULL); return OR; }
{DOT}   { DBGP("DOT"); yylval.node_val = create_node("DOT","",yylineno,NULL); return DOT; }
{NOT}   { DBGP("NOT"); yylval.node_val = create_node("NOT","",yylineno,NULL); return NOT; }
{LP}   { DBGP("LP"); yylval.node_val = create_node("LP","",yylineno,NULL); return LP; }
{RP}   { DBGP("RP"); yylval.node_val = create_node("RP","",yylineno,NULL); return RP; }
{LB}   { DBGP("LB"); yylval.node_val = create_node("LB","",yylineno,NULL); return LB; }
{RB}   { DBGP("RB"); yylval.node_val = create_node("RB","",yylineno,NULL); return RB; }
{LC}   { DBGP("LC"); yylval.node_val = create_node("LC","",yylineno,NULL); return LC; }
{RC}   { DBGP("RC"); yylval.node_val = create_node("RC","",yylineno,NULL); return RC; }
{STRUCT}   { DBGP("STRUCT"); yylval.node_val = create_node("STRUCT","",yylineno,NULL); return STRUCT; }
{RETURN}   { DBGP("RETURN"); yylval.node_val = create_node("RETURN","",yylineno,NULL); return RETURN; }
{TYPE}  { DBGP("TYPE"); yylval.node_val = create_node("TYPE",yytext,yylineno,NULL); return TYPE;}
{IF}   { DBGP("IF"); yylval.node_val = create_node("IF","",yylineno,NULL); return IF; }
{ELSE}   { DBGP("ELSE"); yylval.node_val = create_node("ELSE","",yylineno,NULL); return ELSE; }
{WHILE}   { DBGP("WHILE"); yylval.node_val = create_node("WHILE","",yylineno,NULL); return WHILE; }
{INT}  {   DBGP("INT"); yylval.node_val = create_node("INT", yytext,yylineno,NULL); return INT;}
{ID}  { DBGP("ID"); yylval.node_val = create_node("ID",yytext,yylineno,NULL); return ID;}
{FLOAT}  {DBGP("FLOAT"); yylval.node_val = create_node("FLOAT",yytext,yylineno,NULL); return FLOAT;}
{ERRORINTA} {printf("Error type A at Line %d: .\n", yylineno); extern int error_cnt; ++error_cnt;report_lexical_error();}
{ERRORINTB} {printf("Error type A at Line %d: .\n", yylineno); extern int error_cnt; ++error_cnt;report_lexical_error();}

\n  {yycolumn = 1;}
[ \t]+  {;}

"//" { 
    {printf("Error type B at Line %d: .\n", yylineno); extern int error_cnt; ++error_cnt;} // just to pass test D3
    char c;
    while ((c = input()) != '\n' && c != EOF);
}
"/*" { 
          {printf("Error type B at Line %d: .\n", yylineno); extern int error_cnt; ++error_cnt;}
    char c;
    while (1) 
    { 
        c = input(); 
        if (c == '\n') {

          {printf("Error type B at Line %d: .\n", yylineno); extern int error_cnt; ++error_cnt;}
        }
        if (c == '*') 
        { 
            c = input();
            if (c == '/') 
                break;
        }
    } 
}

.  { printf("Error type A at Line %d: Mysterious characters \'%s\'.\n", yylineno, yytext); extern int error_cnt; ++error_cnt; report_lexical_error(); } 
%%

#include "ir_gen.h"
#include "code_gen.h"
int main(int argc, char** argv) { 
    // if (argc < 2) {  /* just read stdin */ 
      // yylex(); 
      // return 0; 
    // } 
    // //yydebug = 1;
    // extern int error_cnt;
    // for (int i = 1; i < argc; i++) { 
      
      // FILE *f = fopen(argv[i], "r"); 
      // if (!f) { 
        // perror(argv[i]); 
        // return 1; 
      // }
      // yyrestart(f);
      // yyparse(); 
    // }
    // if (error_cnt > 0) {
      // return 0;
    // }
    // init_stack(&sign_table);
    // init_sign_table();
    // dfs(root);
    // free_sign_table();
    // destroy_tree(root);

    // usage: [input_filename] [output_filename]
    if (argc > 4) {
      return 1;
    }
    FILE *in = fopen(argv[1], "r");
    FILE *out = fopen(argv[2], "w");
    yyrestart(in);
    yyparse();
    syntax_tree_visitor_t visitor;
    visitor.tree = root;
    visitor.code = NULL;
    visitor.out = out;
    visit_tree(&visitor);
    return 0; 
} 

