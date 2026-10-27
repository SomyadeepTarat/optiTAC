/* symtab.h - symbol table and activation records (Somyadeep) */
#ifndef SYMTAB_H
#define SYMTAB_H

#include "ast.h"

enum { FRAME_RETURN_PC, FRAME_CALLER, FRAME_ACCESS_LINK, FRAME_RESULT, FRAME_HEADER };

typedef struct Symbol {
    char *name;
    char *kind;              /* "function", "param" or "local" */
    int offset;              /* slot in the activation record: fp + offset */
    struct Symbol *next;

    /* only for functions: */
    struct Symbol *vars;     /* its params and locals */
    int nparams, nvars, ntemps;
} Symbol;

int check(Node *program);    /* returns number of errors */
Symbol *find_function(char *name);
void print_symtab(void);
void print_records(void);
void free_symtab(void);

#endif
