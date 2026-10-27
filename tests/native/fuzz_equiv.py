"""Seeded differential testing of optimized and original MiniProc execution.

Compare the exit status, result output and full runtime diagnostics. Bounded
loops keep instruction-budget differences out of semantic equivalence checks.
A mismatch writes a reproducible source file and exits unsuccessfully.
"""
import argparse
import pathlib
import random
import subprocess
import tempfile


def generate(rng):
    variables = ['a', 'b', 'c', 'd']

    def expression(depth=0):
        if depth >= 3 or rng.random() < .4:
            return rng.choice(variables + ['0', '1', '2', '2147483647', '(0 - 1)'])
        op = rng.choice(['+', '-', '*', '/', '<', '>', '<=', '>=', '==', '!='])
        return f'({expression(depth + 1)} {op} {expression(depth + 1)})'

    lines = ['int helper(int x, int y) { return x + y; }',
             'int main() { int a; int b; int c; int d; int i;']
    for name in variables:
        if rng.random() < .82:
            lines.append(f'{name} = {rng.randint(0, 8)};')
    for _ in range(rng.randint(2, 12)):
        name = rng.choice(variables)
        choice = rng.randrange(5)
        if choice == 0:
            lines.append(f'if ({expression()}) {name} = {expression()}; else {name} = {expression()};')
        elif choice == 1:
            lines.append(f'i = 0; while (i < {rng.randint(0, 5)}) {{ {name} = {expression()}; i = i + 1; }}')
        elif choice == 2:
            lines.append(f'{name} = helper({expression()}, {expression()});')
        else:
            lines.append(f'{name} = {expression()};')
    lines.append(f'return {expression()}; }}')
    return '\n'.join(lines) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', nargs='?', default='./build/mpc')
    parser.add_argument('--count', type=int, default=1500)
    parser.add_argument('--seed', type=int, default=307)
    args = parser.parse_args()
    if args.count <= 0:
        parser.error('--count must be positive')
    binary = str(pathlib.Path(args.binary).resolve())
    rng = random.Random(args.seed)
    errors = 0
    with tempfile.TemporaryDirectory(prefix='miniproc-fuzz-') as directory:
        path = pathlib.Path(directory) / 'case.mp'
        for index in range(args.count):
            source = generate(rng)
            path.write_text(source)
            results = []
            for options in [[], ['--no-opt']]:
                run = subprocess.run([binary, '--run', *options, str(path)],
                                     capture_output=True, text=True, timeout=10)
                results.append((run.returncode, run.stdout, run.stderr))
            if results[0] != results[1] or results[0][0] not in (0, 1) or 'Sanitizer' in results[0][2]:
                failure = pathlib.Path('fuzz-failure.mp').resolve()
                failure.write_text(source)
                print(f'Mismatch at case {index}, seed {args.seed}: {failure}')
                print('optimized:', repr(results[0]))
                print('original: ', repr(results[1]))
                return 1
            errors += results[0][0] != 0
    print(f'{args.count} programs, {errors} runtime errors, 0 mismatches (seed {args.seed})')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
