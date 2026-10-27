/* Structured three-address code; operands use frame slots, not ambiguous names. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir.h"
static IRFunction *current;
static void *allocate(size_t n) {
    void *p = calloc(1, n);
    if (!p) { fprintf(stderr, "out of memory\n"); exit(1); }
    return p;
}
static Operand constant(int n) { Operand o = {1, n}; return o; }
static Operand slot(int n) { Operand o = {0, n}; return o; }
static Operand temp(void) {
    return slot(FRAME_HEADER + current->symbol->nvars + current->symbol->ntemps++);
}
static Operand variable(const char *name) {
    Symbol *v;
    for (v = current->symbol->vars; v; v = v->next)
        if (!strcmp(v->name, name)) return slot(v->offset);
    fprintf(stderr, "internal error: unresolved variable %s\n", name); exit(1);
}
static void emit(Instruction i) {
    if (current->count == current->capacity) {
        size_t n = current->capacity ? current->capacity * 2 : 32;
        Instruction *p = realloc(current->code, n * sizeof(*p));
        if (!p) { fprintf(stderr, "out of memory\n"); exit(1); }
        current->code = p; current->capacity = n;
    }
    i.source_pc = current->count;
    current->code[current->count++] = i;
}
static Operand expression(Node *e) {
    Operand a, b, t;
    Instruction i = {0};
    Node *arg;
    int n = 0, k = 0;
    Operand *args;
    switch (e->kind) {
    case N_NUM: return constant(e->value);
    case N_VAR: return variable(e->name);
    case N_BINOP:
        a = expression(e->left); b = expression(e->right); t = temp();
        i.op = IR_BINARY; i.a = a; i.b = b; i.dest = t.value; i.name = e->op;
        emit(i); return t;
    case N_CALL:
        for (arg = e->args; arg; arg = arg->next) n++;
        args = allocate((size_t)(n ? n : 1) * sizeof(*args));
        /* Evaluate nested calls before pushing the outer call's parameters. */
        for (arg = e->args; arg; arg = arg->next) args[k++] = expression(arg);
        for (k = 0; k < n; k++) {
            i.op = IR_PARAM; i.a = args[k]; emit(i);
        }
        free(args); t = temp();
        i.op = IR_CALL; i.dest = t.value; i.name = e->name; i.argc = n;
        emit(i); return t;
    default: fprintf(stderr, "internal error: invalid expression\n"); exit(1);
    }
}
static void label(int n) { Instruction i = {0}; i.op = IR_LABEL; i.label = n; emit(i); }
static void jump(Opcode op, int n, Operand a) {
    Instruction i = {0}; i.op = op; i.label = n; i.a = a; emit(i);
}
static void statements(Node *s) {
    int l1, l2;
    Instruction i;
    for (; s; s = s->next) {
        memset(&i, 0, sizeof(i));
        switch (s->kind) {
        case N_ASSIGN:
            i.a = expression(s->left); i.op = IR_COPY;
            i.dest = variable(s->name).value; emit(i); break;
        case N_IF:
            i.a = expression(s->left); l1 = ++current->nlabels;
            jump(IR_FALSE, l1, i.a); statements(s->body);
            if (s->right) {
                l2 = ++current->nlabels; jump(IR_JUMP, l2, constant(0));
                label(l1); statements(s->right); label(l2);
            } else label(l1);
            break;
        case N_WHILE:
            l1 = ++current->nlabels; l2 = ++current->nlabels;
            label(l1); i.a = expression(s->left); jump(IR_FALSE, l2, i.a);
            statements(s->body); jump(IR_JUMP, l1, constant(0)); label(l2); break;
        case N_RETURN:
            i.a = expression(s->left); i.op = IR_RETURN; emit(i); break;
        case N_BLOCK: statements(s->body); break;
        default: break;
        }
    }
}
IRProgram *gen_tac(Node *nodes) {
    Node *f;
    size_t k = 0;
    IRProgram *p = allocate(sizeof(*p));
    for (f = nodes; f; f = f->next) p->count++;
    p->functions = allocate(p->count * sizeof(*p->functions));
    for (f = nodes; f; f = f->next) {
        Instruction trap = {0};
        current = &p->functions[k++]; current->symbol = find_function(f->name);
        current->symbol->ntemps = 0; statements(f->body);
        trap.op = IR_TRAP; emit(trap); /* never silently return an undefined value */
    }
    return p;
}
IRProgram *clone_ir(const IRProgram *p) {
    size_t k;
    IRProgram *q = allocate(sizeof(*q)); q->count = p->count;
    q->functions = allocate(q->count * sizeof(*q->functions));
    for (k = 0; k < p->count; k++) {
        q->functions[k] = p->functions[k];
        q->functions[k].code = allocate(p->functions[k].count * sizeof(Instruction));
        memcpy(q->functions[k].code, p->functions[k].code,
               p->functions[k].count * sizeof(Instruction));
        q->functions[k].capacity = q->functions[k].count;
    }
    return q;
}
void free_ir(IRProgram *p) {
    size_t k; if (!p) return;
    for (k = 0; k < p->count; k++) free(p->functions[k].code);
    free(p->functions); free(p);
}
static void operand_text(char *out, size_t n, const IRFunction *f, Operand o) {
    Symbol *v;
    if (o.immediate) { snprintf(out, n, "%d", o.value); return; }
    for (v = f->symbol->vars; v; v = v->next)
        if (v->offset == o.value) { snprintf(out, n, "%s@%d", v->name, v->offset); return; }
    snprintf(out, n, "t%d", o.value - FRAME_HEADER - f->symbol->nvars + 1);
}
void print_ir(const IRProgram *p, int quads) {
    size_t k, pc;
    for (k = 0; k < p->count; k++) {
        const IRFunction *f = &p->functions[k];
        printf("\n%s:\n", f->symbol->name);
        if (quads) printf("    #    (op, arg1, arg2, result)\n");
        for (pc = 0; pc < f->count; pc++) {
            Instruction i = f->code[pc]; char a[128], b[128], d[128];
            operand_text(a, sizeof(a), f, i.a); operand_text(b, sizeof(b), f, i.b);
            operand_text(d, sizeof(d), f, slot(i.dest));
            if (quads) {
                printf("    %-4zu ", pc);
                switch (i.op) {
                case IR_COPY: printf("(=, %s, -, %s)", a, d); break;
                case IR_BINARY: printf("(%s, %s, %s, %s)", i.name, a, b, d); break;
                case IR_LABEL: printf("(label, -, -, L%d)", i.label); break;
                case IR_JUMP: printf("(goto, -, -, L%d)", i.label); break;
                case IR_FALSE: printf("(ifFalse, %s, -, L%d)", a, i.label); break;
                case IR_PARAM: printf("(param, %s, -, -)", a); break;
                case IR_CALL: printf("(call, %s, %d, %s)", i.name, i.argc, d); break;
                case IR_RETURN: printf("(return, %s, -, -)", a); break;
                case IR_TRAP: printf("(trap, missing return, -, -)"); break;
                }
            } else {
                printf("    ");
                switch (i.op) {
                case IR_COPY: printf("%s = %s", d, a); break;
                case IR_BINARY: printf("%s = %s %s %s", d, a, i.name, b); break;
                case IR_LABEL: printf("L%d:", i.label); break;
                case IR_JUMP: printf("goto L%d", i.label); break;
                case IR_FALSE: printf("ifFalse %s goto L%d", a, i.label); break;
                case IR_PARAM: printf("param %s", a); break;
                case IR_CALL: printf("%s = call %s, %d", d, i.name, i.argc); break;
                case IR_RETURN: printf("return %s", a); break;
                case IR_TRAP: printf("trap missing return"); break;
                }
            }
            printf("\n");
        }
    }
}
