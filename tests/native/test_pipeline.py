"""End-to-end language, optimizer equivalence, VM and diagnostic regressions."""
import pathlib
import subprocess
import sys
import tempfile
import unittest

BINARY = str(pathlib.Path(sys.argv.pop(1) if len(sys.argv) > 1 else './build/mpc').resolve())
ROOT = pathlib.Path(__file__).resolve().parents[2]

class PipelineTests(unittest.TestCase):
    def compile(self, source, *options):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / 'test.mp'
            path.write_text(source)
            return subprocess.run([BINARY, *options, str(path)], text=True,
                                  capture_output=True, timeout=10)

    def result(self, source, expected):
        for options in [('--run',), ('--run', '--no-opt')]:
            with self.subTest(options=options):
                run = self.compile(source, *options)
                self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
                self.assertEqual(run.stdout.strip(), f'Result: {expected}')
                self.assertEqual(run.stderr, '')

    def error(self, source, message, *options):
        run = self.compile(source, '--run', *options)
        self.assertNotEqual(run.returncode, 0, run.stdout)
        self.assertIn(message, run.stdout + run.stderr)
        self.assertNotIn('Result:', run.stdout)
        self.assertNotIn('Sanitizer', run.stderr)
        return run

    def test_supplied_examples(self):
        for name, expected in [('factorial', 1144), ('max', 7)]:
            self.result((ROOT / 'examples' / (name + '.mp')).read_text(), expected)

    def test_precedence_and_parentheses(self):
        self.result('int main() { return (2 + 3) * 4 - 9 / 2; }', 16)

    def test_comparisons(self):
        self.result('int main() { return (1 < 2) + (3 > 2) + (2 <= 2) + (2 >= 2) + (1 == 1) + (1 != 2); }', 6)

    def test_negative_values_and_division(self):
        self.result('int main() { return (0 - 7) / 2; }', -3)

    def test_folding_and_propagation(self):
        source = 'int main() { int x; int y; x = 2 + 3 * 4; y = x + 1; return y; }'
        self.result(source, 15)
        raw = self.compile(source, '--phase', 'tac', '--compile-only')
        opt = self.compile(source, '--phase', 'optimized', '--compile-only')
        self.assertEqual(raw.returncode, 0)
        self.assertEqual(opt.returncode, 0)
        self.assertIn('3 * 4', raw.stdout)
        self.assertIn('return 15', opt.stdout)
        self.assertNotIn('3 * 4', opt.stdout)

    def test_loop_and_branch_join(self):
        self.result('int main() { int i; int sum; i = 0; sum = 0; while (i < 10) { if (i < 5) sum = sum + i; else sum = sum + 2 * i; i = i + 1; } return sum; }', 80)
        for n, expected in [(0, 11), (1, 22)]:
            self.result(f'int main() {{ int x; x = 7; if ({n}) x = 22; else x = 11; return x; }}', expected)

    def test_forward_and_nested_calls(self):
        self.result('int main() { return add(inc(2), add(inc(3), inc(4))); } int inc(int x) { return x + 1; } int add(int a, int b) { return a + b; }', 12)

    def test_mutual_recursion(self):
        self.result('int even(int n) { if (n == 0) return 1; return odd(n - 1); } int odd(int n) { if (n == 0) return 0; return even(n - 1); } int main() { return even(20); }', 1)

    def test_recursive_frames_preserve_locals(self):
        self.result('int sum(int n) { int saved; saved = n; if (n == 0) return 0; return sum(n - 1) + saved; } int main() { return sum(10); }', 55)

    def test_temp_name_collision(self):
        self.result('int main() { int t1; t1 = 10; return t1 + 2 * 3; }', 16)

    def test_many_arguments(self):
        params = ', '.join(f'int x{i}' for i in range(60))
        args = ', '.join(str(i) for i in range(60))
        self.result(f'int f({params}) {{ return x0 + x59; }} int main() {{ return f({args}); }}', 59)

    def test_function_scopes(self):
        self.result('int f(int x) { int y; y = x + 2; return y; } int main() { int x; int y; x = 10; y = f(3); return x + y; }', 15)

    def test_dangling_else(self):
        self.result('int main() { int x; x = 1; if (1) if (0) x = 2; else x = 3; return x; }', 3)

    def test_early_return(self):
        self.result('int main() { return 42; return 1 / 0; }', 42)

    def test_semantic_errors(self):
        for source, message in [
            ('int main() { return x; }', 'undeclared variable'),
            ('int main() { x = 1; return 0; }', 'undeclared variable'),
            ('int main() { int x; int x; return 0; }', 'declared twice'),
            ('int f(int x, int x) { return x; } int main() { return 0; }', 'declared twice'),
            ('int main() { return nope(); }', 'undeclared function'),
            ('int f(int x) { return x; } int main() { return f(); }', 'needs 1 argument'),
            ('int main() { return 0; } int main() { return 1; }', 'defined twice'),
            ('int f() { return 0; }', 'no main'),
            ('int main(int x) { return x; }', 'main must have no parameters'),
            ('int main() { int x; x = 1; }', 'without return'),
            ('int f(int x) { if (x) return 1; } int main() { return 0; }', 'without return'),
        ]:
            with self.subTest(source=source): self.error(source, message)

    def test_lexer_and_parser_errors(self):
        for source, message in [
            ('int main() { return @; }', "line 1: unknown character '@'"),
            ('int main() {\n return 1\n}', 'line 3: syntax error'),
            ('int main() { return 999999999999999999999; }', 'integer literal out of range'),
            ('', 'syntax error'),
        ]:
            with self.subTest(source=source): self.error(source, message)

    def test_runtime_errors_and_equivalence(self):
        for source, message in [
            ('int main() { return 1 / 0; }', 'division by zero'),
            ('int main() { return 2147483647 + 1; }', 'integer overflow'),
            ('int main() { return (0 - 2147483647 - 1) / (0 - 1); }', 'integer overflow'),
            ('int main() { int x; return x; }', 'uninitialized variable'),
            ('int main() { int x; if (0) x = 1; return x; }', 'uninitialized variable'),
        ]:
            for options in [(), ('--no-opt',)]:
                with self.subTest(source=source, options=options):
                    run = self.error(source, message, *options)
                    self.assertIn('MiniProc stack', run.stderr)

    def test_limits(self):
        self.error('int main() { while (1) {} return 0; }', 'instruction limit', '--max-steps', '100')
        run = self.error('int f() { return f(); } int main() { return f(); }', 'call stack limit')
        self.assertIn('depth 1023', run.stderr)

    def test_trace(self):
        run = self.compile((ROOT / 'examples/factorial.mp').read_text(), '--run', '--trace')
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertIn('[trace] call depth=1 fact', run.stdout)
        self.assertIn('control_link=0', run.stdout)
        self.assertIn('[trace] return depth=1 fact => 120', run.stdout)
        self.assertIn('Result: 1144', run.stdout)

    def test_token_categories_and_lines(self):
        source = ('int helper(int main) { return main; }\n'
                  'int main // entry point\n() { int x; x = helper(2 + 3 - 1 * 4 / 2); '
                  'if (x <= 4) x = (x >= 1) + (x == 3) + (x != 0) + (x < 9) + (x > 0); '
                  'else x = 0; while (0) {} return x; }')
        run = self.compile(source, '--phase', 'tokens', '--compile-only')
        self.assertEqual(run.returncode, 0, run.stderr)
        rows = [line.split(maxsplit=3) for line in run.stdout.splitlines() if line.startswith('line ')]
        tokens = [(int(row[1]), row[2], row[3]) for row in rows]
        self.assertIn((2, 'ENTRY_POINT_FUNCTION', 'main'), tokens)
        self.assertIn((1, 'IDENTIFIER', 'main'), tokens)
        self.assertIn((3, 'FUNCTION_IDENTIFIER', 'helper'), tokens)
        for op in ['+', '-', '*', '/']:
            self.assertIn((3, 'ARITHMETIC_OPERATOR', op), tokens)
        for op in ['<=', '>=', '==', '!=', '<', '>']:
            self.assertIn((3, 'RELATIONAL_OPERATOR', op), tokens)
        self.assertIn((3, 'ASSIGNMENT_OPERATOR', '='), tokens)
        self.assertIn((3, 'PUNCTUATION', ';'), tokens)
        self.assertIn((3, 'INTEGER_LITERAL', '2'), tokens)
        self.assertNotIn('SYMBOL', run.stdout)

    def test_phase_selection(self):
        source = 'int main() { return 2 + 3; }'
        for phase in ['tokens', 'ast', 'symbols', 'records', 'tac', 'quads', 'optimized', 'all']:
            with self.subTest(phase=phase):
                run = self.compile(source, '--phase', phase, '--compile-only')
                self.assertEqual(run.returncode, 0, run.stderr)
                self.assertNotIn('Result:', run.stdout)
                self.assertIn('=====', run.stdout)
        run = self.compile(source, '--phase', 'quads', '--compile-only')
        self.assertIn('(+, 2, 3, t1)', run.stdout)
        run = self.compile(source, '--phase', 'records', '--compile-only')
        self.assertIn('fp+4  temp t1', run.stdout)

    def optimized(self, source):
        run = self.compile(source, '--phase', 'optimized', '--compile-only')
        self.assertEqual(run.returncode, 0, run.stderr)
        return run.stdout

    def test_novelty_example(self):
        self.result((ROOT / 'examples/novelty.mp').read_text(), 100)

    def test_common_subexpressions(self):
        for second in ['a + b', 'b + a']:
            source = f'int f(int a, int b) {{ int x; int y; x = a + b; y = {second}; return x + y; }} int main() {{ return f(3, 4); }}'
            self.result(source, 14)
            out = self.optimized(source)
            self.assertEqual(out.count('a@4 + b@5'), 1)
            self.assertNotIn('b@5 + a@4', out)
        self.result('int f(int a) { int x; int y; x = a + 1; a = a + 2; y = a + 1; return x + y; } int main() { return f(3); }', 10)

    def test_strength_and_identities(self):
        source = 'int f(int x) { return (x + 0) + (0 + x) + (x - 0) + (x * 1) + (1 * x) + (x / 1) + x * 2; } int main() { return f(3); }'
        self.result(source, 24)
        out = self.optimized(source)
        self.assertNotIn(' * 2', out)
        self.assertNotIn(' / 1', out)
        self.assertIn('x@4 + x@4', out)

    def test_constant_branches_and_unreachable(self):
        source = 'int main() { if (0) return 1 / 0; else return 42; return 2 / 0; }'
        self.result(source, 42)
        self.assertNotIn(' / ', self.optimized(source))
        self.assertNotIn('ifFalse', self.optimized(source))
        self.result('int main() { int x; x = 0; while (0) x = 1 / 0; return x; }', 0)

    def test_dead_stores(self):
        source = 'int main() { int unused; unused = 2 + 3; return 7; }'
        self.result(source, 7)
        out = self.optimized(source)
        self.assertNotIn('unused@', out)
        self.assertNotIn(' = ', out)

    def test_optimizer_exact_error_preservation(self):
        for source in [
            'int main() { int x; return x * 0; }',
            'int main() { int x; int unused; unused = x; return 0; }',
            'int f(int x) { int unused; unused = x * 2; return 0; } int main() { return f(2147483647); }',
            'int main() { int unused; unused = 2147483647 + 1; return 0; }',
            'int f(int x) { return x * 2; } int main() { return f(0 - 2147483647); }',
            'int f(int x) { int a; a = x + 0; return 1 / 0; } int main() { return f(2); }',
            'int main() { int x; return (x + 0) * 0; }',
            'int f(int x) { return x + x + x; } int main() { return f(2147483647); }',
        ]:
            with self.subTest(source=source):
                raw = self.compile(source, '--run', '--no-opt')
                opt = self.compile(source, '--run')
                self.assertNotEqual(raw.returncode, 0)
                self.assertEqual((opt.returncode, opt.stdout, opt.stderr),
                                 (raw.returncode, raw.stdout, raw.stderr))

    def test_statistics(self):
        source = (ROOT / 'examples/novelty.mp').read_text()
        for options in [('--run',), ('--run', '--no-opt'), ('--compile-only', '--phase', 'optimized')]:
            run = self.compile(source, *options, '--stats')
            self.assertEqual(run.returncode, 0, run.stderr)
            self.assertIn('static instructions:', run.stdout)
            self.assertIn('passes to fixpoint:', run.stdout)
            if '--run' in options:
                self.assertIn('Result: 100', run.stdout)
                self.assertRegex(run.stdout, r'dynamic VM steps: [0-9]+ -> [0-9]+')
            else:
                self.assertNotIn('dynamic VM steps:', run.stdout)

        limited = self.compile(source, '--run', '--stats', '--max-steps', '1300')
        self.assertEqual(limited.returncode, 0, limited.stderr)
        self.assertEqual(limited.stderr, '')
        self.assertIn('Result: 100', limited.stdout)
        self.assertIn('comparison unavailable', limited.stdout)

    def test_loop_carried_liveness(self):
        self.result('int f(int n) { int i; int a; int b; i = 0; a = 0; b = 1; while (i < n) { a = b; b = a + 1; i = i + 1; } return a; } int main() { return f(12); }', 12)
        self.result('int main() { int i; int x; i = 0; x = 1; while (i < 10) { if (i < 5) x = x + 2; else x = x * 2; i = i + 1; } return x; }', 352)

    def test_copy_redefinition(self):
        self.result('int f(int a) { int b; int c; b = a; c = b; a = 9; b = 10; return c + b + a; } int main() { return f(3); }', 22)
        self.result('int f(int a) { int b; b = a; a = a + 1; return b + a; } int main() { return f(3); }', 7)

    def test_cli_errors(self):
        for options, message in [(('--max-steps', '0'), 'invalid instruction limit'),
                                 (('--phase', 'invalid'), 'unknown phase'),
                                 (('--bogus',), 'unknown or incomplete option')]:
            self.error('int main() { return 0; }', message, *options)
        run = subprocess.run([BINARY, '/does/not/exist.mp'], text=True, capture_output=True)
        self.assertNotEqual(run.returncode, 0)
        self.assertIn('cannot open', run.stderr)

if __name__ == '__main__': unittest.main(verbosity=2)
