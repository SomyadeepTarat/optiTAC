#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir.h"
int eval_binary(const char *op, int a, int b, int *result) {
    int64_t value;
    if (!strcmp(op, "+")) value = (int64_t)a + b;
    else if (!strcmp(op, "-")) value = (int64_t)a - b;
    else if (!strcmp(op, "*")) value = (int64_t)a * b;
    else if (!strcmp(op, "/")) {
        if (!b || (a == INT_MIN && b == -1)) return 0;
        value = a / b;
    } else if (!strcmp(op, "<")) value = a < b;
    else if (!strcmp(op, ">")) value = a > b;
    else if (!strcmp(op, "<=")) value = a <= b;
    else if (!strcmp(op, ">=")) value = a >= b;
    else if (!strcmp(op, "==")) value = a == b;
    else if (!strcmp(op, "!=")) value = a != b;
    else return 0;
    if (value < INT_MIN || value > INT_MAX) return 0;
    *result = (int)value; return 1;
}

OptStats opt_stats;
static void *checked_calloc(size_t n, size_t size) {
    void *p = calloc(n ? n : 1, size);
    if (!p) { fprintf(stderr, "out of memory in optimizer\n"); exit(1); }
    return p;
}
static int equal(Operand a, Operand b) {
    return a.immediate == b.immediate && a.value == b.value;
}
static int writes(const Instruction *i) {
    return i->op == IR_COPY || i->op == IR_BINARY || i->op == IR_CALL;
}
static int boundary(const Instruction *i) {
    return i->op == IR_JUMP || i->op == IR_FALSE ||
           i->op == IR_RETURN || i->op == IR_TRAP;
}
static int temporary(const IRFunction *fn, Operand a) {
    return !a.immediate && a.value >= FRAME_HEADER + fn->symbol->nvars;
}
static int safe_operand(const IRFunction *fn, Operand a) {
    return a.immediate || temporary(fn, a);
}
static int literal(Operand a, int n) { return a.immediate && a.value == n; }
static int uses(Instruction *i, Operand **a, Operand **b) {
    *a = *b = NULL;
    switch (i->op) {
    case IR_BINARY: *b = &i->b; /* fall through */
    case IR_COPY: case IR_FALSE: case IR_PARAM: case IR_RETURN: *a = &i->a; break;
    default: break;
    }
    return *a != NULL;
}
static int commutative(const char *op) {
    return !strcmp(op, "+") || !strcmp(op, "*") ||
           !strcmp(op, "==") || !strcmp(op, "!=");
}
/* Facts and available expressions exist only within a basic block. Copies are
   invalidated when either endpoint changes; expression results are also killed. */
static int local_pass(IRFunction *fn) {
    size_t slots = FRAME_HEADER + fn->symbol->nvars + fn->symbol->ntemps;
    Operand *facts = checked_calloc(slots, sizeof(*facts));
    unsigned char *known = checked_calloc(slots, 1);
    Instruction *available = checked_calloc(fn->count, sizeof(*available));
    size_t pc, n = 0, k;
    int changed = 0;
    for (pc = 0; pc < fn->count; pc++) {
        Instruction *i = &fn->code[pc];
        Operand *a, *b, *operands[2];
        int v, j;
        if (i->op == IR_LABEL) { memset(known, 0, slots); n = 0; }
        uses(i, &a, &b); operands[0] = a; operands[1] = b;
        for (j = 0; j < 2; j++) {
            Operand *o = operands[j];
            if (o && !o->immediate && known[o->value]) {
                Operand replacement = facts[o->value];
                if (!equal(*o, replacement)) {
                    *o = replacement; changed = 1;
                    if (o->immediate) opt_stats.constants++; else opt_stats.copies++;
                }
            }
        }
        if (i->op == IR_BINARY) {
            Operand replacement = i->a;
            int rewrite = 0;
            if (i->a.immediate && i->b.immediate && eval_binary(i->name, i->a.value, i->b.value, &v)) {
                replacement.immediate = 1; replacement.value = v;
                rewrite = 1; opt_stats.constants++;
            } else if ((!strcmp(i->name, "+") && literal(i->b, 0)) ||
                       (!strcmp(i->name, "-") && literal(i->b, 0)) ||
                       ((!strcmp(i->name, "*") || !strcmp(i->name, "/")) && literal(i->b, 1))) {
                rewrite = 1; opt_stats.algebraic++;
            } else if ((!strcmp(i->name, "+") && literal(i->a, 0)) ||
                       (!strcmp(i->name, "*") && literal(i->a, 1))) {
                replacement = i->b; rewrite = 1; opt_stats.algebraic++;
            } else if (!strcmp(i->name, "*") &&
                       ((literal(i->b, 0) && temporary(fn, i->a)) ||
                        (literal(i->a, 0) && temporary(fn, i->b)))) {
                replacement.immediate = 1; replacement.value = 0;
                rewrite = 1; opt_stats.algebraic++;
            }
            if (rewrite) {
                i->op = IR_COPY; i->a = replacement; i->strength_reduced = 0; changed = 1;
            } else {
                /* x*2 and x+x have identical checked overflow conditions. */
                if (!strcmp(i->name, "*") && literal(i->b, 2)) {
                    i->name = "+"; i->b = i->a; i->strength_reduced = 1;
                    opt_stats.strength++; changed = 1;
                }
                for (k = 0; k < n; k++) {
                    Instruction *e = &available[k];
                    if (!strcmp(i->name, e->name) &&
                        ((equal(i->a, e->a) && equal(i->b, e->b)) ||
                         (commutative(i->name) && equal(i->a, e->b) && equal(i->b, e->a)))) {
                        i->op = IR_COPY; i->a.immediate = 0; i->a.value = e->dest;
                        i->strength_reduced = 0;
                        opt_stats.common++; changed = 1; break;
                    }
                }
            }
        }
        if (writes(i)) {
            for (k = 0; k < slots; k++)
                if (known[k] && (k == (size_t)i->dest ||
                    (!facts[k].immediate && facts[k].value == i->dest))) known[k] = 0;
            for (k = 0; k < n;) {
                Instruction *e = &available[k];
                if (e->dest == i->dest || (!e->a.immediate && e->a.value == i->dest) ||
                    (!e->b.immediate && e->b.value == i->dest)) available[k] = available[--n];
                else k++;
            }
            if (i->op == IR_COPY && (i->a.immediate || i->a.value != i->dest)) {
                known[i->dest] = 1; facts[i->dest] = i->a;
            }
            if (i->op == IR_BINARY && (i->a.immediate || i->a.value != i->dest) &&
                (i->b.immediate || i->b.value != i->dest)) available[n++] = *i;
        }
        if (boundary(i)) { memset(known, 0, slots); n = 0; }
    }
    free(facts); free(known); free(available);
    return changed;
}
static void compact(IRFunction *fn, const unsigned char *keep) {
    size_t read, write = 0;
    for (read = 0; read < fn->count; read++) if (keep[read]) fn->code[write++] = fn->code[read];
    fn->count = write;
}
static int branches(IRFunction *fn) {
    size_t pc;
    int changed = 0;
    unsigned char *keep = checked_calloc(fn->count, 1);
    memset(keep, 1, fn->count);
    for (pc = 0; pc < fn->count; pc++) {
        Instruction *i = &fn->code[pc];
        if (i->op == IR_FALSE && i->a.immediate) {
            if (i->a.value) keep[pc] = 0; else i->op = IR_JUMP;
            opt_stats.branches++; changed = 1;
        }
        if (i->op == IR_JUMP && pc + 1 < fn->count &&
            fn->code[pc+1].op == IR_LABEL && fn->code[pc+1].label == i->label) {
            keep[pc] = 0; opt_stats.branches++; changed = 1;
        }
    }
    compact(fn, keep); free(keep); return changed;
}
/* Label indices are rebuilt after each compaction. */
static size_t *label_map(const IRFunction *fn) {
    size_t *labels = checked_calloc((size_t)fn->nlabels + 1, sizeof(*labels)), pc;
    for (pc = 0; pc < fn->count; pc++)
        if (fn->code[pc].op == IR_LABEL) labels[fn->code[pc].label] = pc;
    return labels;
}
static int reachable(IRFunction *fn) {
    size_t *labels = label_map(fn), *work = checked_calloc(fn->count, sizeof(*work));
    unsigned char *keep = checked_calloc(fn->count, 1), *referenced = checked_calloc((size_t)fn->nlabels+1, 1);
    size_t n = 0, pc;
    int changed = 0;
    if (fn->count) { work[n++] = 0; keep[0] = 1; }
    while (n) {
        Instruction *i;
        size_t next[2], k, count = 0;
        pc = work[--n]; i = &fn->code[pc];
        if (i->op == IR_JUMP || i->op == IR_FALSE) {
            next[count++] = labels[i->label]; referenced[i->label] = 1;
        }
        if (i->op != IR_JUMP && i->op != IR_RETURN && i->op != IR_TRAP && pc+1 < fn->count)
            next[count++] = pc+1;
        for (k = 0; k < count; k++) if (!keep[next[k]]) { keep[next[k]] = 1; work[n++] = next[k]; }
    }
    for (pc = 0; pc < fn->count; pc++) {
        if (fn->code[pc].op == IR_LABEL && !referenced[fn->code[pc].label]) keep[pc] = 0;
        if (!keep[pc]) { opt_stats.unreachable++; changed = 1; }
    }
    compact(fn, keep); free(labels); free(work); free(keep); free(referenced); return changed;
}
/* Backward liveness over all CFG edges, including loop back-edges. Only
   non-failing definitions may disappear: dead arithmetic must still trap. */
static int dead_stores(IRFunction *fn) {
    size_t slots = FRAME_HEADER + fn->symbol->nvars + fn->symbol->ntemps;
    size_t *labels = label_map(fn), pc, k;
    unsigned char *in, *out, *scratch, *keep;
    int again, changed = 0;
    if (fn->count && slots > SIZE_MAX / fn->count) { fprintf(stderr, "optimizer liveness too large\n"); exit(1); }
    in = checked_calloc(fn->count, slots); out = checked_calloc(fn->count, slots);
    scratch = checked_calloc(slots, 1); keep = checked_calloc(fn->count, 1);
    do {
        again = 0;
        for (pc = fn->count; pc-- > 0;) {
            Instruction *i = &fn->code[pc]; Operand *a, *b;
            unsigned char *row = &out[pc*slots];
            memset(row, 0, slots);
            if (i->op == IR_JUMP || i->op == IR_FALSE)
                memcpy(row, &in[labels[i->label]*slots], slots);
            if (i->op != IR_JUMP && i->op != IR_RETURN && i->op != IR_TRAP && pc+1 < fn->count)
                for (k = 0; k < slots; k++) row[k] |= in[(pc+1)*slots+k];
            memcpy(scratch, row, slots);
            if (writes(i)) scratch[i->dest] = 0;
            uses(i, &a, &b);
            if (a && !a->immediate) scratch[a->value] = 1;
            if (b && !b->immediate) scratch[b->value] = 1;
            if (memcmp(scratch, &in[pc*slots], slots)) {
                memcpy(&in[pc*slots], scratch, slots); again = 1;
            }
        }
    } while (again);
    for (pc = 0; pc < fn->count; pc++) {
        Instruction *i = &fn->code[pc];
        int safe = i->op == IR_COPY && safe_operand(fn, i->a);
        if (i->op == IR_BINARY && strcmp(i->name, "+") && strcmp(i->name, "-") &&
            strcmp(i->name, "*") && strcmp(i->name, "/") &&
            safe_operand(fn, i->a) && safe_operand(fn, i->b)) safe = 1;
        keep[pc] = !(safe && !out[pc*slots+i->dest]);
        if (!keep[pc]) { changed = 1; opt_stats.dead++; }
    }
    compact(fn, keep); free(labels); free(in); free(out); free(scratch); free(keep);
    return changed;
}
void optimize(IRProgram *p) {
    size_t f;
    memset(&opt_stats, 0, sizeof(opt_stats));
    for (f = 0; f < p->count; f++) {
        size_t round;
        for (round = 1; round <= 20; round++) {
            int changed = local_pass(&p->functions[f]);
            changed |= branches(&p->functions[f]);
            changed |= reachable(&p->functions[f]);
            changed |= dead_stores(&p->functions[f]);
            if (round > opt_stats.passes) opt_stats.passes = round;
            if (!changed) break;
        }
    }
}
