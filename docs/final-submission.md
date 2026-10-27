# Final submission

Start with `npm ci`, then `npm run dev`. Open the displayed local URL and select Workbench. The native compiler is rebuilt automatically. For terminal use, run `make` and `./build/mpc --stats examples/novelty.mp`.

## Token report

| Lexeme / role | Display category |
|---|---|
| `main` followed by `(` | ENTRY_POINT_FUNCTION |
| Other names followed by `(` | FUNCTION_IDENTIFIER |
| Variable and parameter names | IDENTIFIER |
| `int if else while return` | KEYWORD |
| Integer constants | INTEGER_LITERAL |
| `+ - * /` | ARITHMETIC_OPERATOR |
| `=` | ASSIGNMENT_OPERATOR |
| `< > <= >= == !=` | RELATIONAL_OPERATOR |
| `( ) { } , ;` | PUNCTUATION |

`main` is a function identifier with a special entry-point role, not a reserved keyword. The report uses one-token lookahead to show function roles; the parser still receives ID tokens. A variable named `main` remains an identifier. Whitespace and supported line comments do not affect function recognition.

The native language supports integers, functions, recursion, branches and loops. The Explore engine has a separate straight-line int/float language; use Workbench for full MiniProc programs. Unsupported syntax produces diagnostics rather than being claimed as supported C.

## Reproduce validation

- `npm run check`: browser tests, native tests, production build and lint.
- `make fuzz`: 1,500 differential optimizer cases.
- `make sanitize`: native regression suite with AddressSanitizer and UndefinedBehaviorSanitizer.

Fresh complete compiler reports for every bundled example are in `docs/evidence/`. They include tokens, AST, symbol table, activation records, TAC, quadruples, optimized TAC, execution and statistics.

Generated scanner and parser sources are included, so normal builds require no Flex or Bison. Grammar regeneration uses `make regen` and requires modern Bison (3+) and Flex; macOS system Bison 2.3 is too old.

The submission archive excludes prebuilt platform executables, dependencies, temporary files and historical presentation artifacts. Build from the included sources.
