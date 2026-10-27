/* ast.c - making and printing tree nodes (Puneeth) */
#include <stdio.h>
#include <stdlib.h>
#include "ast.h"

Node *new_node(Kind kind) {
    Node *n = calloc(1, sizeof(Node));   /* all fields start as 0 / NULL */
    if (!n) { fprintf(stderr, "out of memory\n"); exit(1); }
    n->kind = kind;
    return n;
}

void free_ast(Node *n) {
    while (n) {
        Node *next = n->next;
        free_ast(n->left); free_ast(n->right); free_ast(n->body);
        free_ast(n->params); free_ast(n->args);
        free(n->name); free(n); n = next;
    }
}

/* add item to the end of a linked list */
Node *append(Node *list, Node *item) {
    Node *p = list;
    if (list == NULL) return item;
    while (p->next != NULL) p = p->next;
    p->next = item;
    return list;
}

/* print an expression on one line; brackets show how the parser grouped it,
   e.g.  a + b * c  is printed as  a + (b * c) */
static void print_expr(Node *e, int top) {
    Node *a;
    switch (e->kind) {
    case N_NUM:   printf("%d", e->value); break;
    case N_VAR:   printf("%s", e->name); break;
    case N_CALL:  printf("%s(", e->name);
                  for (a = e->args; a != NULL; a = a->next) {
                      print_expr(a, 1);
                      if (a->next) printf(", ");
                  }
                  printf(")"); break;
    case N_BINOP: if (!top) printf("(");
                  print_expr(e->left, 0);
                  printf(" %s ", e->op);
                  print_expr(e->right, 0);
                  if (!top) printf(")");
                  break;
    default: break;
    }
}

/* print each statement on its own line; the indent shows what is inside what */
void print_ast(Node *n, int depth) {
    Node *p;
    for (; n != NULL; n = n->next) {
        if (n->kind == N_BLOCK) {          /* a block just holds statements */
            print_ast(n->body, depth);
            continue;
        }
        printf("%*s", depth * 4, "");      /* indent */
        switch (n->kind) {
        case N_FUNC:   printf("FUNCTION %s (", n->name);
                       for (p = n->params; p != NULL; p = p->next)
                           printf("%s%s", p->name, p->next ? ", " : "");
                       printf(")\n");
                       print_ast(n->body, depth + 1);
                       printf("\n"); break;
        case N_DECL:   printf("DECLARE %s\n", n->name); break;
        case N_ASSIGN: printf("ASSIGN %s = ", n->name); print_expr(n->left, 1); printf("\n"); break;
        case N_IF:     printf("IF "); print_expr(n->left, 1); printf("\n");
                       print_ast(n->body, depth + 1);
                       if (n->right) {
                           printf("%*sELSE\n", depth * 4, "");
                           print_ast(n->right, depth + 1);
                       }
                       break;
        case N_WHILE:  printf("WHILE "); print_expr(n->left, 1); printf("\n");
                       print_ast(n->body, depth + 1); break;
        case N_RETURN: printf("RETURN "); print_expr(n->left, 1); printf("\n"); break;
        default: break;
        }
    }
}
