/* parser.y - grammar rules that build the syntax tree (Puneeth) */
%{
#include <stdio.h>
#include <stdlib.h>
#include "ast.h"

int yylex(void);
extern int yylineno;
Node *program;

void yyerror(const char *msg) {
    fprintf(stderr, "line %d: %s\n", yylineno, msg);
}

Node *binop(char *op, Node *l, Node *r) {
    Node *n = new_node(N_BINOP);
    n->op = op; n->left = l; n->right = r;
    return n;
}
%}

%union { int num; char *str; Node *node; }
%destructor { free($$); } <str>
%destructor { free_ast($$); } <node>

%token <num> NUM
%token <str> ID
%token INT IF ELSE WHILE RETURN EQ NE LE GE

%left EQ NE
%left '<' '>' LE GE
%left '+' '-'
%left '*' '/'
%nonassoc NO_ELSE
%nonassoc ELSE

%type <node> funcs func params param_list block stmts stmt expr args arg_list

%%

program : funcs                 { program = $1; } ;

funcs   : func                  { $$ = $1; }
        | funcs func            { $$ = append($1, $2); } ;

func    : INT ID '(' params ')' block
                                { $$ = new_node(N_FUNC); $$->name = $2; $$->params = $4; $$->body = $6; } ;

params  : /* none */            { $$ = NULL; }
        | param_list            { $$ = $1; } ;

param_list : INT ID             { $$ = new_node(N_PARAM); $$->name = $2; }
        | param_list ',' INT ID { Node *p = new_node(N_PARAM); p->name = $4; $$ = append($1, p); } ;

block   : '{' stmts '}'         { $$ = new_node(N_BLOCK); $$->body = $2; } ;

stmts   : /* none */            { $$ = NULL; }
        | stmts stmt            { $$ = append($1, $2); } ;

stmt    : INT ID ';'            { $$ = new_node(N_DECL); $$->name = $2; }
        | ID '=' expr ';'       { $$ = new_node(N_ASSIGN); $$->name = $1; $$->left = $3; }
        | IF '(' expr ')' stmt %prec NO_ELSE
                                { $$ = new_node(N_IF); $$->left = $3; $$->body = $5; }
        | IF '(' expr ')' stmt ELSE stmt
                                { $$ = new_node(N_IF); $$->left = $3; $$->body = $5; $$->right = $7; }
        | WHILE '(' expr ')' stmt
                                { $$ = new_node(N_WHILE); $$->left = $3; $$->body = $5; }
        | RETURN expr ';'       { $$ = new_node(N_RETURN); $$->left = $2; }
        | block                 { $$ = $1; } ;

expr    : expr '+' expr         { $$ = binop("+", $1, $3); }
        | expr '-' expr         { $$ = binop("-", $1, $3); }
        | expr '*' expr         { $$ = binop("*", $1, $3); }
        | expr '/' expr         { $$ = binop("/", $1, $3); }
        | expr '<' expr         { $$ = binop("<", $1, $3); }
        | expr '>' expr         { $$ = binop(">", $1, $3); }
        | expr LE expr          { $$ = binop("<=", $1, $3); }
        | expr GE expr          { $$ = binop(">=", $1, $3); }
        | expr EQ expr          { $$ = binop("==", $1, $3); }
        | expr NE expr          { $$ = binop("!=", $1, $3); }
        | '(' expr ')'          { $$ = $2; }
        | NUM                   { $$ = new_node(N_NUM); $$->value = $1; }
        | ID                    { $$ = new_node(N_VAR); $$->name = $1; }
        | ID '(' args ')'       { $$ = new_node(N_CALL); $$->name = $1; $$->args = $3; } ;

args    : /* none */            { $$ = NULL; }
        | arg_list              { $$ = $1; } ;

arg_list : expr                 { $$ = $1; }
        | arg_list ',' expr     { $$ = append($1, $3); } ;

%%
