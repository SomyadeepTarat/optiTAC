/* Iterative interpreter: MiniProc recursion uses heap frames, not the C stack. */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "ir.h"
#define MAX_DEPTH 1024
#define MAX_FRAME_SLOTS 1000000

static void diagnostic(int quiet, const char *format, ...) {
    va_list args;
    if (quiet) return;
    va_start(args, format); vfprintf(stderr, format, args); va_end(args);
}
typedef struct Frame {
    const IRFunction *function;
    struct Frame *caller;
    int *slots, *arguments;
    unsigned char *initialized;
    size_t pc, nargs, arg_capacity;
    int destination, depth;
} Frame;
static void destroy(Frame *f) {
    free(f->slots); free(f->initialized); free(f->arguments); free(f);
}
static Frame *create(const IRFunction *fn, Frame *caller, int dest) {
    size_t n = (size_t)(FRAME_HEADER + fn->symbol->nvars + fn->symbol->ntemps);
    Frame *f;
    if (n > MAX_FRAME_SLOTS) return NULL;
    f = calloc(1, sizeof(*f));
    if (!f) return NULL;
    f->slots = calloc(n, sizeof(*f->slots)); f->initialized = calloc(n, 1);
    if (!f->slots || !f->initialized) { destroy(f); return NULL; }
    f->function = fn; f->caller = caller; f->destination = dest;
    f->depth = caller ? caller->depth + 1 : 0;
    f->slots[FRAME_RETURN_PC] = caller ? (int)caller->pc : -1;
    f->slots[FRAME_CALLER] = caller ? caller->depth : -1;
    f->slots[FRAME_ACCESS_LINK] = -1;
    f->initialized[FRAME_RETURN_PC] = f->initialized[FRAME_CALLER] = 1;
    f->initialized[FRAME_ACCESS_LINK] = 1;
    return f;
}
static const IRFunction *lookup(const IRProgram *p, const char *name) {
    size_t k;
    for (k = 0; k < p->count; k++)
        if (!strcmp(p->functions[k].symbol->name, name)) return &p->functions[k];
    return NULL;
}
static int read_operand(Frame *f, Operand o, int *result, int quiet) {
    if (o.immediate) { *result = o.value; return 1; }
    if (!f->initialized[o.value]) {
        Symbol *v;
        for (v = f->function->symbol->vars; v; v = v->next)
            if (v->offset == o.value) {
                diagnostic(quiet, "runtime error: uninitialized variable '%s'\n", v->name);
                return 0;
            }
        diagnostic(quiet, "runtime error: uninitialized temporary\n"); return 0;
    }
    *result = f->slots[o.value]; return 1;
}
static int target(Frame *f, int label, int quiet) {
    size_t pc;
    for (pc = 0; pc < f->function->count; pc++)
        if (f->function->code[pc].op == IR_LABEL && f->function->code[pc].label == label) {
            f->pc = pc; return 1;
        }
    diagnostic(quiet, "runtime error: invalid jump label\n"); return 0;
}
static void trace_frame(Frame *f) {
    Symbol *v;
    printf("[trace] call depth=%d %s return_pc=%d control_link=%d slots=%d",
           f->depth, f->function->symbol->name, f->slots[FRAME_RETURN_PC],
           f->slots[FRAME_CALLER], FRAME_HEADER + f->function->symbol->nvars + f->function->symbol->ntemps);
    for (v = f->function->symbol->vars; v; v = v->next)
        if (!strcmp(v->kind, "param")) printf(" %s=%d", v->name, f->slots[v->offset]);
    printf("\n");
}
int run_vm(const IRProgram *p, int trace, size_t max_steps, int *result, size_t *executed_steps, int quiet) {
    const IRFunction *entry = lookup(p, "main");
    Frame *f;
    size_t steps = 0;
    int a, b, value, ok = 0;
    if (!entry) { diagnostic(quiet, "runtime error: no main function\n"); return 0; }
    f = create(entry, NULL, 0);
    if (!f) { diagnostic(quiet, "runtime error: cannot allocate frame\n"); return 0; }
    if (trace && !quiet) trace_frame(f);
    while (f) {
        Instruction i;
        if (steps++ >= max_steps) { diagnostic(quiet, "runtime error: instruction limit exceeded\n"); break; }
        if (f->pc >= f->function->count) { diagnostic(quiet, "runtime error: missing return\n"); break; }
        i = f->function->code[f->pc++];
        switch (i.op) {
        case IR_COPY:
            if (!read_operand(f, i.a, &value, quiet)) goto done;
            f->slots[i.dest] = value; f->initialized[i.dest] = 1; break;
        case IR_BINARY:
            if (!read_operand(f, i.a, &a, quiet) || !read_operand(f, i.b, &b, quiet)) goto done;
            if (!eval_binary(i.name, a, b, &value)) {
                diagnostic(quiet, "runtime error: %s in %s (%d %s %d)\n",
                        !strcmp(i.name, "/") && b == 0 ? "division by zero" : "integer overflow",
                        f->function->symbol->name, a, i.strength_reduced ? "*" : i.name, i.strength_reduced ? 2 : b); goto done;
            }
            f->slots[i.dest] = value; f->initialized[i.dest] = 1; break;
        case IR_LABEL: break;
        case IR_JUMP: if (!target(f, i.label, quiet)) goto done; break;
        case IR_FALSE:
            if (!read_operand(f, i.a, &a, quiet)) goto done;
            if (!a && !target(f, i.label, quiet)) goto done;
            break;
        case IR_PARAM:
            if (!read_operand(f, i.a, &a, quiet)) goto done;
            if (f->nargs == f->arg_capacity) {
                size_t n = f->arg_capacity ? f->arg_capacity * 2 : 8;
                int *args = realloc(f->arguments, n * sizeof(*args));
                if (!args) { diagnostic(quiet, "runtime error: cannot allocate arguments\n"); goto done; }
                f->arguments = args; f->arg_capacity = n;
            }
            f->arguments[f->nargs++] = a; break;
        case IR_CALL: {
            const IRFunction *fn = lookup(p, i.name);
            Frame *child;
            size_t k;
            if (!fn || f->nargs != (size_t)i.argc || i.argc != fn->symbol->nparams) {
                diagnostic(quiet, "runtime error: invalid call\n"); goto done;
            }
            if (f->depth + 1 >= MAX_DEPTH) { diagnostic(quiet, "runtime error: call stack limit exceeded\n"); goto done; }
            child = create(fn, f, i.dest);
            if (!child) { diagnostic(quiet, "runtime error: cannot allocate frame\n"); goto done; }
            for (k = 0; k < f->nargs; k++) {
                child->slots[FRAME_HEADER + k] = f->arguments[k];
                child->initialized[FRAME_HEADER + k] = 1;
            }
            f->nargs = 0; f = child;
            if (trace && !quiet) trace_frame(f);
            break;
        }
        case IR_RETURN: {
            Frame *caller = f->caller;
            if (!read_operand(f, i.a, &value, quiet)) goto done;
            f->slots[FRAME_RESULT] = value; f->initialized[FRAME_RESULT] = 1;
            if (trace && !quiet) printf("[trace] return depth=%d %s => %d\n", f->depth, f->function->symbol->name, value);
            if (caller) {
                caller->slots[f->destination] = f->slots[FRAME_RESULT];
                caller->initialized[f->destination] = 1;
                caller->pc = (size_t)f->slots[FRAME_RETURN_PC];
            } else { *result = value; ok = 1; }
            destroy(f); f = caller; break;
        }
        case IR_TRAP:
            diagnostic(quiet, "runtime error: function %s reached end without return\n", f->function->symbol->name);
            goto done;
        }
    }
done:
    if (!ok && f) {
        Frame *s;
        diagnostic(quiet, "MiniProc stack (most recent call first):\n");
        for (s = f; s; s = s->caller)
            diagnostic(quiet, "  %s at instruction %zu (depth %d)\n", s->function->symbol->name, s->pc ? s->function->code[s->pc - 1].source_pc : 0, s->depth);
    }
    while (f) { Frame *caller = f->caller; destroy(f); f = caller; }
    if (executed_steps) *executed_steps = steps;
    return ok;
}
