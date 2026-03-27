# 目标文件
TARGET = parser

LEXICALL = lexical.l

LEXICALC = lex.yy.c

LEXICAL_TARGET = $(LEXICALC)

SYNTAXY = syntax.y

SYNTAXC = syntax.tab.c

SYNTAXH = syntax.tab.h

SYNTAX_TARGET = $(SYNTAXC) $(SYNTAXH)

LIBS = hashmap.o

# 源文件
SRCS = main.c node.c sign_table.c hir.c ir_gen.c reg.c code_gen.c basicblock.c $(LIBS) $(SYNTAXC) 

# 编译器
CC = gcc

# 编译选项
CFLAGS = -lfl -std=c99 -g -fsanitize=address

# 默认目标
all: $(TARGET)

LIBS : hashmap.c
	$(CC) -c $< -o $@
# 生成目标文件
$(TARGET): $(SRCS) $(LEXICAL_TARGET)
	$(CC) $(SRCS) $(CFLAGS) -o $(TARGET)

$(SYNTAXC) : $(SYNTAXY)
	bison -d $< -o $@

$(LEXICALC) : $(LEXICALL)
	flex $<

# 清理生成的文件
clean:
	rm -f $(TARGET) $(SYNTAX_TARGET) $(LEXICAL_TARGET) $(LIBS)

submit: all
	zip -r submit.zip .. -x "../Code/test/*" -x "../.git/*" -x "../draft/*" -x "../Code/lab4/*"

# 伪目标，防止与同名文件冲突
.PHONY: all clean