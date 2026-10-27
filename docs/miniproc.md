# MiniProc

A teaching compiler for an integer language with procedures. The pipeline is
Flex lexer → Bison parser/AST → semantic checks and symbols → activation-record
layout → structured TAC/quadruples → optimizer → stack VM.

## Build and run

macOS/Linux (C compiler and Make required):

```sh
make
./build/mpc examples/factorial.mp
./build/mpc --run examples/max.mp
./build/mpc --run --trace examples/factorial.mp
make test
make fuzz
make sanitize
```

The examples return **1144** (120 + 1024) and **7**, respectively. The compiler's
exit status is 0 on success and 1 on errors; the MiniProc result is printed,
rather than used as the shell exit status.

`examples/novelty.mp` returns **100** and exercises all eight optimizer transformations.
Run `./build/mpc --run --stats examples/novelty.mp` to compare instruction counts
and actual VM steps. The sample currently reduces 36 instructions to 19 and
1,915 VM steps to 1,210; these are measurements of this implementation, not the
report's illustrative counts for a different version of the example.

`examples/optimizer.mp` returns **154** and gives a small demo of constant
folding plus a loop. Compare it with `--phase tac --compile-only` and
`--phase optimized --compile-only` to see the folded `seed = 14` assignment.

Windows: run `build.bat` with GCC available, then
`build\mpc.exe examples\factorial.mp`. The executable is built from the included sources. Windows compilation has not been
executed in the macOS workspace.

Generated scanner/parser C files are included, so normal builds do not require
Flex or Bison. After changing the grammar/scanner run `make regen` and `make`.
The Makefile accepts `CC` and `CFLAGS` overrides. `make test` requires Python 3;
`make sanitize` requires compiler support for AddressSanitizer and UBSan.

## Driver options

| Option | Behavior |
| --- | --- |
| No options | Print every compiler phase, then execute optimized TAC |
| `--run` | Print only the execution result |
| `--trace` | Show each call/return, depth, frame size, parameters and caller link |
| `--no-opt` | Execute original TAC for comparison |
| `--stats` | Show static instruction counts, VM steps and optimizer counters |
| `--compile-only` | Compile and show selected phases without executing |
| `--phase NAME` | Show `tokens`, `ast`, `symbols`, `records`, `tac`, `quads`, `optimized`, or `all` |
| `--max-steps N` | Set positive VM instruction budget (default 1,000,000) |
| `--help` | Show usage |

For example:

```sh
./build/mpc --phase quads --compile-only examples/max.mp
./build/mpc --phase optimized --compile-only examples/factorial.mp
./build/mpc --run --no-opt examples/factorial.mp
```

Phase selection controls presentation; the driver still validates the whole
source. When both `--run` and `--phase` are supplied, the last sets presentation.

## Language and semantics

- Only `int` functions, parameters and local variables; `main()` has no parameters.
- Declarations (`int x;`), assignments, blocks, `if`/`else`, `while`, and `return`.
- Integer literals, variable references, function calls, parentheses and
  `+ - * / < > <= >= == !=`. Comparisons produce 0 or 1; nonzero conditions are true.
- Binary arithmetic/comparisons use the precedence in `native/grammar/parser.y`. Division
  truncates toward zero. To express negative numbers use subtraction, e.g. `0 - 3`.
- `//` comments and whitespace are supported. Other characters are lexical errors.
- Functions are globally visible, so forward calls and mutual recursion work.
  Arguments and binary operands are evaluated left to right.
- Variables use **function scope**, matching the existing language. Blocks group
  statements; they do not create another variable scope. Duplicate names within
  a function (including its parameters) are rejected. Different functions may
  reuse names. Declarations must precede uses in the compiler's source traversal.
- Calls must name an existing function and supply exactly its parameter count.
- A conservative semantic check requires an unconditional return, an if/else
  whose branches both return, or a literal-nonzero loop whose body returns.
  Value-dependent return proofs are not attempted. A final IR trap also protects
  against functions falling through without returning.
- Locals have no default value. Reading an unassigned variable is a VM error.
  Overflow and division by zero are errors, including when constant expressions
  could otherwise be folded. Integer values use the target C `int` range (32 bits
  on the tested system); intermediate checked arithmetic uses `int64_t`.

As listed under planned extensions in the supplied MiniProc progress report, `void`, reference
parameters, nested procedures and block scopes remain outside this version.
Arrays, pointers, floats, strings and native machine-code generation are also
outside the supplied language scope.

## Stage verification and implementation

| Stage | Initial audit | Completed implementation and evidence |
| --- | --- | --- |
| 1. Lexer | Present | Token output, identifiers, literals, keywords/operators, comments; added literal range validation; lexical-error tests |
| 2. Parser and AST | Present | Grammar/precedence, control flow, calls and recursive AST; syntax, dangling-else and expression tests; cleanup added |
| 3. Semantics / symbols | Present for function scope | Declaration/function duplication, undeclared names and argument counts; added entry signature and return checks; positive and negative tests |
| 4. Activation records | Static layout present | Layout aligned with PPT; temporaries included after generation; actual independent frames and caller restoration implemented in VM |
| 5. IR generation / TAC | Printed text only | Structured instructions, typed constant/slot operands and quadruple output; removed fixed 50-argument limit and variable/temp-name ambiguity |
| 6. Optimiser | Absent | Eight transformations to a fixpoint; checked arithmetic and error preservation; original IR preserved; differential fuzzing |
| 7. Run-time VM | Absent | Parameters, locals, temporaries, jumps, comparisons, calls and returns; recursion and mutual recursion; stack trace and runtime diagnostics |
| 8. Driver / phase output | Partial | Integrated pipeline, phase selection, optimizer toggle, compile-only, execution, trace, statistics and instruction budget; portable build and tests |

## IR and optimization

`ir.h` defines each instruction as an opcode, up to two operands, destination,
label and call metadata. A slot operand refers directly to its function frame;
a literal operand carries an integer. Printed locals use `name@offset`, while
temporaries use `tN`, so a source variable named `t1` cannot alias a compiler temp.
Calls evaluate all argument expressions before emitting the outer call's `param`
instructions. This keeps nested calls from consuming another call's arguments.

`optimize.c` runs eight transformations per function until no changes remain,
with a maximum of 20 rounds:

1. Block-local constant propagation and checked constant folding.
2. Copy propagation, invalidated when either slot is redefined.
3. Algebraic identities (`x+0`, `0+x`, `x-0`, `x*1`, `1*x`, `x/1`), plus multiplication by zero for compiler temporaries.
4. Strength reduction (`x*2` to `x+x`), with the same overflow condition.
5. Block-local common-subexpression elimination, including commutative expressions.
6. Constant branch folding and removal of jumps to the next label.
7. CFG reachability and removal of unreachable instructions and unused labels.
8. Dead-store elimination using backward liveness to a fixpoint across CFG edges.

Facts and available expressions are cleared at labels and control-flow boundaries.
Calls have private frames, so they invalidate their result slot without discarding
unrelated caller facts. Removing unused labels allows later rounds to combine
blocks. Frame offsets and slot counts remain stable throughout optimization.

Error preservation takes priority over deleting instructions. Reading a source
variable can fail, so `x*0` is retained when `x` could be uninitialized. Dead
arithmetic that can divide by zero or overflow is retained, even when its value is
unused. Dead-store removal is limited to copies and comparisons of constants or
compiler temporaries, which are initialized before use by generated IR. Failed
constant folds remain executable instructions. Diagnostics retain original TAC
instruction positions and the original multiplication spelling after strength
reduction, allowing full error-output comparison with `--no-opt`.

`--stats` shows static counts and per-transformation counters. On successful
execution it also runs the other IR silently to measure both VM step counts,
using the same instruction budget. With `--no-opt` the selected execution is the
original IR, while statistics still show raw-to-optimized counts. Compile-only
statistics omit VM steps. If the comparison run exceeds the budget, dynamic
comparison is reported as unavailable. A failed selected execution reports its
normal runtime diagnostic and exits unsuccessfully.

## Activation records and VM

| Offset | Contents |
| --- | --- |
| `fp+0` | Return instruction index in caller, or -1 for entry |
| `fp+1` | Control link: caller frame index/depth, or -1 for entry |
| `fp+2` | Reserved access link, -1 (no nested procedures) |
| `fp+3` | Return value |
| `fp+4` onward | Parameters, then locals, then temporaries |

The VM uses heap-allocated frames linked to their callers, and an iterative
instruction dispatcher. Every call copies arguments into a new frame. A return
writes its return slot, transfers it to the caller's destination, restores the
saved instruction index, and releases the callee frame. Recursion does not rely
on recursion in the C interpreter. Frame indices in trace output identify the
currently active stack; a depth may be reused after a frame returns.

Runtime limits: 1,024 active frames, 1,000,000 slots per frame and the configurable
instruction budget. Runtime failures show the active MiniProc stack, most recent
call first. The trace feature uses `--trace`; it is optional so normal demos remain
readable.

## Validation

`tests/native/test_pipeline.py` contains 31 test methods with multiple cases, covering
language semantics, recursion, frame isolation, optimizer transformations,
copy/CSE invalidation, loop-carried liveness, exact runtime error equivalence,
statistics, limits, and all driver phases. `make test` runs the suite, and
`make sanitize` runs it under AddressSanitizer and UBSan.

`make fuzz` runs 1,500 reproducible generated programs with arithmetic,
comparisons, conditionals, bounded loops, function calls, and potentially
uninitialized variables. The fuzzer compares exit status, stdout and full stderr
between optimized and original execution, and rejects process crashes and
sanitizer findings. On mismatch it saves `fuzz-failure.mp` for reproduction.
Run a custom campaign with:

```sh
python3 tests/native/fuzz_equiv.py ./build/mpc --count 1500 --seed 307
```

The supplied progress report describes the target optimizer and validation;
this checkout now implements those features. Its future language extensions
(`void`, references, nested procedures and block scopes), SSA, global value
numbering and native code generation remain future work as specified there.
Windows build verification requires a Windows environment.
