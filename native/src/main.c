/* Integrated driver: each displayed phase uses the same compilation pipeline. */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir.h"
extern FILE *yyin;
extern Node *program;
int yyparse(void);
void print_tokens(FILE *f);
int yylex_destroy(void);
static void usage(FILE *out) {
    fprintf(out, "usage: mpc [options] file.mp\n"
        "  --run          execute and print only the result\n"
        "  --trace        show VM calls, frames and returns\n"
        "  --no-opt       execute original TAC\n"
        "  --stats        show optimizer counts and VM step comparison\n"
        "  --compile-only show phases without executing\n"
        "  --phase NAME   tokens|ast|symbols|records|tac|quads|optimized|all\n"
        "  --max-steps N  VM instruction limit (default 1000000)\n"
        "  --help         show this help\n");
}
static int show(const char *phase, const char *name) {
    return !strcmp(phase, "all") || !strcmp(phase, name);
}
int main(int argc, char **argv) {
    const char *path = NULL, *phase = "all";
    int stats = 0;
    int trace = 0, no_opt = 0, compile_only = 0, result = 0, status = 1, k;
    size_t max_steps = 1000000, executed = 0;
    IRProgram *raw = NULL, *optimized = NULL;
    for (k = 1; k < argc; k++) {
        if (!strcmp(argv[k], "--help")) { usage(stdout); return 0; }
        if (!strcmp(argv[k], "--run")) phase = "none";
        else if (!strcmp(argv[k], "--trace")) trace = 1;
        else if (!strcmp(argv[k], "--stats")) stats = 1;
        else if (!strcmp(argv[k], "--no-opt")) no_opt = 1;
        else if (!strcmp(argv[k], "--compile-only")) compile_only = 1;
        else if (!strcmp(argv[k], "--phase") && k + 1 < argc) {
            phase = argv[++k];
            if (strcmp(phase, "tokens") && strcmp(phase, "ast") && strcmp(phase, "symbols") &&
                strcmp(phase, "records") && strcmp(phase, "tac") && strcmp(phase, "quads") &&
                strcmp(phase, "optimized") && strcmp(phase, "all")) {
                fprintf(stderr, "unknown phase: %s\n", phase); return 1;
            }
        } else if (!strcmp(argv[k], "--max-steps") && k + 1 < argc) {
            char *end; unsigned long long n;
            errno = 0; n = strtoull(argv[++k], &end, 10);
            if (errno || !*argv[k] || *end || argv[k][0] == '-' || !n || n > SIZE_MAX) {
                fprintf(stderr, "invalid instruction limit\n"); return 1;
            }
            max_steps = (size_t)n;
        } else if (argv[k][0] == '-') { fprintf(stderr, "unknown or incomplete option: %s\n", argv[k]); usage(stderr); return 1; }
        else if (path) { fprintf(stderr, "expected one source file\n"); return 1; }
        else path = argv[k];
    }
    if (!path) { usage(stderr); return 1; }
    yyin = fopen(path, "r");
    if (!yyin) { fprintf(stderr, "cannot open %s: %s\n", path, strerror(errno)); return 1; }
    if (show(phase, "tokens")) { printf("===== TOKENS =====\n"); print_tokens(yyin); }
    if (yyparse()) goto done;
    if (show(phase, "ast")) { printf("\n===== SYNTAX TREE =====\n"); print_ast(program, 0); }
    if (check(program)) goto done;
    if (show(phase, "symbols")) { printf("\n===== SYMBOL TABLE =====\n"); print_symtab(); }
    raw = gen_tac(program);
    if (show(phase, "records")) { printf("\n===== ACTIVATION RECORDS =====\n"); print_records(); }
    if (show(phase, "tac")) { printf("\n===== THREE-ADDRESS CODE =====\n"); print_ir(raw, 0); }
    if (show(phase, "quads")) { printf("\n===== QUADRUPLES =====\n"); print_ir(raw, 1); }
    optimized = clone_ir(raw); optimize(optimized);
    if (show(phase, "optimized")) { printf("\n===== OPTIMIZED TAC =====\n"); print_ir(optimized, 0); }
    if (!compile_only) {
        if (!strcmp(phase, "all")) printf("\n===== VM EXECUTION =====\n");
        if (!run_vm(no_opt ? raw : optimized, trace, max_steps, &result, &executed, 0)) goto done;
        printf("Result: %d\n", result);
    }
    if (stats) {
        size_t f, before = 0, after = 0, other_steps = 0;
        int other_result = 0, comparison = 0;
        for (f = 0; f < raw->count; f++) {
            before += raw->functions[f].count; after += optimized->functions[f].count;
        }
        printf("\n===== OPTIMIZER STATISTICS =====\n");
        printf("static instructions: %zu -> %zu (%.1f%% fewer)\n", before, after,
               before ? 100.0 * ((double)before - after) / before : 0.0);
        if (!compile_only) {
            comparison = run_vm(no_opt ? optimized : raw, 0, max_steps, &other_result, &other_steps, 1);
            if (comparison && other_result != result) {
                fprintf(stderr, "internal error: optimized and original results disagree\n");
                goto done;
            }
            if (comparison) {
                size_t raw_steps = no_opt ? executed : other_steps;
                size_t opt_steps = no_opt ? other_steps : executed;
                printf("dynamic VM steps: %zu -> %zu (%.1f%% fewer)\n", raw_steps, opt_steps,
                       raw_steps ? 100.0 * ((double)raw_steps - opt_steps) / raw_steps : 0.0);
            } else printf("dynamic VM steps: comparison unavailable within instruction budget\n");
        }
        printf("passes to fixpoint: %zu\nconstants folded: %zu\nalgebraic rewrites: %zu\n"
               "strength reductions: %zu\ncommon subexprs: %zu\ncopies propagated: %zu\n"
               "branches folded: %zu\nunreachable removed: %zu\ndead stores removed: %zu\n",
               opt_stats.passes, opt_stats.constants, opt_stats.algebraic, opt_stats.strength,
               opt_stats.common, opt_stats.copies, opt_stats.branches, opt_stats.unreachable, opt_stats.dead);
    }
    status = 0;
done:
    fclose(yyin); yylex_destroy(); free_ir(raw); free_ir(optimized);
    free_symtab(); free_ast(program);
    return status;
}
