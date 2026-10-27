#ifndef IR_H
#define IR_H
#include <stddef.h>
#include "symtab.h"
typedef struct { int immediate; int value; } Operand;
typedef enum { IR_COPY, IR_BINARY, IR_LABEL, IR_JUMP, IR_FALSE,
               IR_PARAM, IR_CALL, IR_RETURN, IR_TRAP } Opcode;
typedef struct {
    Opcode op;
    Operand a, b;
    int dest, label, argc;
    const char *name;
    size_t source_pc; /* Stable location for diagnostics after compaction. */
    int strength_reduced; /* Preserve the original multiplication diagnostic. */
} Instruction;
typedef struct {
    Symbol *symbol;
    Instruction *code;
    size_t count, capacity;
    int nlabels;
} IRFunction;
typedef struct { IRFunction *functions; size_t count; } IRProgram;
IRProgram *gen_tac(Node *program);
IRProgram *clone_ir(const IRProgram *program);
void free_ir(IRProgram *program);
void print_ir(const IRProgram *program, int quadruples);
typedef struct {
    size_t passes, constants, copies, algebraic, strength, common;
    size_t branches, unreachable, dead;
} OptStats;
extern OptStats opt_stats;
void optimize(IRProgram *program);
/* Shared checked arithmetic keeps folding and execution equivalent. */
int eval_binary(const char *op, int a, int b, int *result);
int run_vm(const IRProgram *program, int trace, size_t max_steps, int *result, size_t *executed_steps, int quiet);
#endif
