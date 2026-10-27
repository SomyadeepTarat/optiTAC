/* ast.h - the syntax tree (Puneeth) */
#ifndef AST_H
#define AST_H

typedef enum {
    N_FUNC,     /* int f(params) { body }   */
    N_PARAM,    /* int a                    */
    N_BLOCK,    /* { statements }           */
    N_DECL,     /* int x;                   */
    N_ASSIGN,   /* x = expr;                */
    N_IF,       /* if (left) body else right */
    N_WHILE,    /* while (left) body        */
    N_RETURN,   /* return left;             */
    N_NUM,      /* 5                        */
    N_VAR,      /* x                        */
    N_CALL,     /* f(args)                  */
    N_BINOP     /* left op right            */
} Kind;

typedef struct Node {
    Kind kind;
    char *name;            /* variable or function name */
    int value;             /* for N_NUM */
    char *op;              /* for N_BINOP: "+", "<", ... */
    struct Node *left, *right;
    struct Node *body;     /* list of statements */
    struct Node *params;   /* list of parameters */
    struct Node *args;     /* list of arguments */
    struct Node *next;     /* next item in a list */
} Node;

Node *new_node(Kind kind);
Node *append(Node *list, Node *item);
void print_ast(Node *n, int depth);
void free_ast(Node *n);

#endif
