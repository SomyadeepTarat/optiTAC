/* symtab.c - symbol table, semantic checks, activation records (Somyadeep)
 *
 * Two levels: a list of functions, and inside each function a list of
 * its params and local variables.
 *
 * Activation record of every function:
 *   fp+0  return address
 *   fp+1  control link (caller's fp)
 *   fp+2  reserved access link (no nested procedures in current scope)
 *   fp+3  return value
 *   fp+4  params, then locals, then temporaries
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symtab.h"

static Symbol *functions = NULL;   /* all functions */
static Symbol *current = NULL;     /* function being checked */
static int errors = 0;

static Symbol *new_symbol(char *name, char *kind) {
    Symbol *s = calloc(1, sizeof(Symbol));
    if (!s) { fprintf(stderr, "out of memory\n"); exit(1); }
    s->name = name;
    s->kind = kind;
    return s;
}

static Symbol *find(Symbol *list, char *name) {
    for (; list != NULL; list = list->next)
        if (strcmp(list->name, name) == 0) return list;
    return NULL;
}

Symbol *find_function(char *name) {
    return find(functions, name);
}

/* add a param or local to the current function */
static void add_var(char *name, char *kind) {
    Symbol *s, *last;
    if (find(current->vars, name)) {
        fprintf(stderr, "error: '%s' declared twice in %s\n", name, current->name);
        errors++;
        return;
    }
    s = new_symbol(name, kind);
    s->offset = FRAME_HEADER + current->nvars;
    current->nvars++;
    if (current->vars == NULL) current->vars = s;
    else {
        for (last = current->vars; last->next; last = last->next);
        last->next = s;
    }
}

static void check_var(char *name) {
    if (!find(current->vars, name)) {
        fprintf(stderr, "error: undeclared variable '%s' in %s\n", name, current->name);
        errors++;
    }
}

static void check_expr(Node *e) {
    Symbol *f;
    Node *a;
    int n = 0;
    if (e == NULL) return;
    if (e->kind == N_VAR) check_var(e->name);
    if (e->kind == N_CALL) {
        f = find_function(e->name);
        for (a = e->args; a; a = a->next) { check_expr(a); n++; }
        if (!f) {
            fprintf(stderr, "error: undeclared function '%s'\n", e->name);
            errors++;
        } else if (n != f->nparams) {
            fprintf(stderr, "error: %s() needs %d argument(s), got %d\n", e->name, f->nparams, n);
            errors++;
        }
    }
    if (e->kind == N_BINOP) {
        check_expr(e->left);
        check_expr(e->right);
    }
}

static void check_stmt(Node *s) {
    for (; s != NULL; s = s->next) {
        switch (s->kind) {
        case N_DECL:   add_var(s->name, "local"); break;
        case N_ASSIGN: check_var(s->name); check_expr(s->left); break;
        case N_IF:     check_expr(s->left); check_stmt(s->body); check_stmt(s->right); break;
        case N_WHILE:  check_expr(s->left); check_stmt(s->body); break;
        case N_RETURN: check_expr(s->left); break;
        case N_BLOCK:  check_stmt(s->body); break;
        default: break;
        }
    }
}

/* Conservative all-path return check for the current statement grammar. */
static int returns(Node *s) {
    for (; s; s = s->next) {
        if (s->kind == N_RETURN) return 1;
        if (s->kind == N_BLOCK && returns(s->body)) return 1;
        if (s->kind == N_IF && s->right && returns(s->body) && returns(s->right)) return 1;
        if (s->kind == N_WHILE && s->left->kind == N_NUM && s->left->value && returns(s->body)) return 1;
    }
    return 0;
}

void free_symtab(void) {
    while (functions) {
        Symbol *next = functions->next, *v = functions->vars;
        while (v) { Symbol *vn = v->next; free(v); v = vn; }
        free(functions); functions = next;
    }
    current = NULL; errors = 0;
}

int check(Node *program) {
    Node *f, *p;
    Symbol *s;
    free_symtab();

    /* pass 1: add every function and its params, so calls can come before definitions */
    for (f = program; f; f = f->next) {
        if (find_function(f->name)) {
            fprintf(stderr, "error: function '%s' defined twice\n", f->name);
            errors++;
            continue;
        }
        s = new_symbol(f->name, "function");
        if (functions == NULL) functions = s;          /* add at the end of the list */
        else {
            Symbol *last;
            for (last = functions; last->next; last = last->next);
            last->next = s;
        }
        current = s;
        for (p = f->params; p; p = p->next) add_var(p->name, "param");
        s->nparams = s->nvars;
    }

    if (errors) return errors;

    /* pass 2: check each function body */
    for (f = program; f; f = f->next) {
        current = find_function(f->name);
        check_stmt(f->body);
        if (!returns(f->body)) {
            fprintf(stderr, "error: function '%s' may reach end without return\n", f->name);
            errors++;
        }
    }

    if (!find_function("main")) {
        fprintf(stderr, "error: no main function\n");
        errors++;
    } else if (find_function("main")->nparams != 0) {
        fprintf(stderr, "error: main must have no parameters\n");
        errors++;
    }
    return errors;
}

void print_symtab(void) {
    Symbol *f, *v;
    for (f = functions; f; f = f->next) {
        printf("function %s (%d params)\n", f->name, f->nparams);
        for (v = f->vars; v; v = v->next)
            printf("    %-8s %-6s fp+%d\n", v->name, v->kind, v->offset);
    }
}

void print_records(void) {
    Symbol *f, *v;
    int i;
    for (f = functions; f; f = f->next) {
        printf("\n%s:\n", f->name);
        printf("    fp+0  return address\n");
        printf("    fp+1  control link\n");
        printf("    fp+2  access link (reserved: -1)\n");
        printf("    fp+3  return value\n");
        for (v = f->vars; v; v = v->next)
            printf("    fp+%d  %s %s\n", v->offset, v->kind, v->name);
        for (i = 1; i <= f->ntemps; i++)
            printf("    fp+%d  temp t%d\n", FRAME_HEADER + f->nvars + i - 1, i);
    }
}
