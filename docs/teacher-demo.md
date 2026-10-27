# MiniProc: five-minute teacher demonstration

## Start before the review

```sh
cd /Users/somyadeeptarat/Desktop/minicomp
npm install
npm run dev
```

Open the local URL printed in Terminal. Start on **Workbench**. Keep Terminal open. Use **Teacher demo guide** in the app for the same route.

## 1. Show the complete pipeline (one minute)

Run the initial program or paste this:

```c
int main() {
    int answer;
    answer = 2 + 3 * 4;
    return answer;
}
```

Click **Run compiler**. Both executions return **14**.

Explain: “One input passes through lexical analysis, parsing, semantic checks, activation-record layout, TAC generation, optimization and VM execution.”

Use the output links in order: Tokens, Syntax tree, Symbol table, Activation records, Three-address code, Quadruples, Optimized TAC. Every section is already expanded.

Point to `3 * 4` in the original TAC and `return 14` in optimized TAC. The initial example reduces 5 static instructions to 1, and 4 executed VM steps to 1.

## 2. Show all optimizer transformations (one minute)

Choose **novelty** in Load an example, then click **Run compiler**.

- Original and optimized results: **100**.
- Static instructions: **36 to 19** (47.2% fewer).
- VM steps: **1,915 to 1,210** (36.8% fewer).
- Fixpoint rounds: **3**.
- All eight transformation counters are nonzero.

Explain: “The optimizer folds constants, propagates copies, simplifies identities, reduces strength, reuses common expressions, folds branches, removes unreachable instructions and removes safe dead stores. It repeats until unchanged or the 20-round cap.”

These are interpreter instruction counts, not a measured wall-clock speedup. The report's 40% VM-step target remains unmet.

## 3. Show procedure calls and frames (one minute)

Choose **factorial**, tick **Show procedure call and frame trace**, and run.

Show Activation records, then Execution. Result: **1144**, which is factorial(5) + power(2, 10), or 120 + 1024.

Explain: “Each call has its own heap frame. The frame stores the return address, caller link, reserved access link, return value, parameters, locals and temporaries. The VM uses an iterative dispatcher, so MiniProc recursion does not use C recursion.”

Untick trace before the next example to keep output short.

## 4. Prove the input is editable (one minute)

Paste this fresh program or open a saved `.mp` file:

```c
int twice(int x) {
    return x * 2;
}
int main() {
    int i;
    int sum;
    i = 0;
    sum = 0;
    while (i < 4) {
        sum = sum + twice(i);
        i = i + 1;
    }
    return sum;
}
```

Run it. Both results should be **12**. Change the loop bound from 4 to 5 and rerun: both results should be **20**.

Then demonstrate error preservation:

```c
int main() {
    return 1 / 0;
}
```

Both executions must report division by zero. Explain: “Optimization must preserve observable failures as well as successful return values.”

MiniProc supports its documented integer language. It does not accept arbitrary full C: `printf`, `#include`, arrays, floats and strings are outside scope. Use separate declaration and assignment statements. Negative values use `0 - n`.

## 5. Show learning interfaces and finish (one minute)

Open **Explore**. Show Town map, Pipeline replay and Challenges. Each remains available behind one secondary navigation. Open a shop if asked about detailed explanations or the local ML scout.

Explain that these optional learning examples use a separate, smaller int/float expression language. Their five browser passes and local ML suggestions do not drive the native eight-transformation optimizer.

Return to **Workbench**. The native source, trace choice and results remain intact while switching views in the current page session. Reloading the page resets them.

Click **Download full output** for the complete compiler text, or **Export evidence JSON** to save the compiled source with its outputs. If the source has changed since the last run, rerun before exporting new evidence. The app flags stale results.

## Full showcase in Terminal

```sh
make
./build/mpc examples/novelty.mp
```

This prints tokens, AST, symbols, activation records, TAC, quadruples, optimized TAC and execution. For the same showcase plus statistics:

```sh
./build/mpc --stats examples/novelty.mp
```

For your own source:

```sh
./build/mpc --stats examples/my-demo.mp
```

`--run` deliberately prints only execution. Leave it out when you want all phases.

Useful focused commands:

```sh
./build/mpc --phase tac --compile-only examples/novelty.mp
./build/mpc --phase optimized --compile-only examples/novelty.mp
./build/mpc --run --trace examples/factorial.mp
npm run check
```
