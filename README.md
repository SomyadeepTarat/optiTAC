# OptiTAC — Compiler Town + MiniProc

Final submission: see [validation and token categories](docs/final-submission.md).

One local compiler workspace: explore compilation in Compiler Town, then run full programs through the native workbench in the same app.

## Start

Requires a C compiler, Make, and Node **22.12+, 24 LTS, or 26+** (Node 25 is unsupported by the installed Vitest version).

```sh
npm ci
npm run dev
```

Startup builds MiniProc automatically. Open the Vite URL, then use **Workbench**. Load `factorial`, `max`, `optimizer`, or `novelty`, edit the source and click **Run compiler**. You can also open a `.mp` file. All phase outputs appear expanded in order, with links to jump between them, execution comparison and statistics.

## Repository layout

```text
native/
  src/           C compiler, optimizer, and virtual machine
  include/       C headers
  grammar/       Flex lexer and Bison grammar
  generated/     Versioned scanner/parser sources for tool-free builds
src/             React interface and browser compiler (tests colocated)
server/          Local Vite API for the native compiler
public/          Static web assets
examples/        MiniProc sample programs
tests/native/   Native regression and differential tests
docs/            Language references, demo guide, and output evidence
tools/legacy/    Historical presentation/report scripts
```

Build products go in `build/` and `dist/`. Local exports and older deliverables
are preserved in `.local-artifacts/`; Git ignores these folders, dependencies,
temporary files, and environment files. Commit `native/generated/` and
`package-lock.json` so fresh clones build consistently.

## The workflow

1. Write a program or load a shared example.
2. Inspect tokens → AST → symbols → activation records → TAC → quadruples → optimized TAC.
3. Compare original and optimized execution, including matching runtime failures.
4. Inspect optimizer statistics and optional call/frame traces.
5. Download the full output as text or export the source and evidence as JSON.

**Explore** contains the optional town, replay and challenge views. Switching views keeps your native source and results in the current page session. The [teacher demo guide](docs/teacher-demo.md) gives a five-minute review route.

For the complete compiler showcase in Terminal, use `./build/mpc --stats examples/novelty.mp`. Omit `--run`, which intentionally shows only execution.

The town map also provides the browser learning engine: per-pass explanations, counterfactual experiments, local ML suggestions, quizzes and replay. Both experiences share one application and startup command. They retain distinct language contracts: MiniProc supports checked integers, functions, branches, loops and recursion; the browser engine supports straight-line int/float expressions using JavaScript numbers. Its ML and pass-omission experiments apply only to browser TAC. Native artifacts are displayed directly from the C compiler rather than converted into incompatible browser IR.

Native requests go only to the local Vite server. Each request uses an isolated temporary source file, subprocess timeout, output cap and 100,000-step VM budget. Native programs run in the C interpreter, not as compiled host machine code. The API is intended for local development; do not expose the development server publicly. Static hosting supports the browser engine only; `npm run preview` also enables the native endpoint locally after `npm run build`.

## Validation and other commands

```sh
npm run check          # browser tests, native tests, production build, lint
npm run native:build   # build C compiler
npm run test:native    # 31 native test methods
make fuzz             # 1,500 native differential cases
```

The dependency lockfile is retained for reproducible installs. No remote AI service or API key is needed.

Detailed engine references: [MiniProc](docs/miniproc.md), [browser learning engine](docs/browser-learning.md). Historical presentation and review scripts remain available; their claims and workstation-specific paths predate this integration.
