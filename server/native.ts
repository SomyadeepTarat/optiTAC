import { execFile } from 'node:child_process';
import { mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { resolve, join } from 'node:path';
import type { IncomingMessage, ServerResponse } from 'node:http';
import type { Plugin } from 'vite';

const root = resolve(import.meta.dirname, '..');
const phases = ['tokens', 'ast', 'symbols', 'records', 'tac', 'quads', 'optimized'] as const;
function execute(args: string[]) {
  return new Promise<{ stdout: string; stderr: string; success: boolean }>(done => {
    execFile(join(root, 'build', process.platform === 'win32' ? 'mpc.exe' : 'mpc'), args, { timeout: 5000, maxBuffer: 1024 * 1024 }, (error, stdout, stderr) => {
      done({ stdout, stderr: stderr + (error && error.code !== 1 ? `\nExecution stopped: ${error.message}` : ''), success: !error });
    });
  });
}
async function handle(req: IncomingMessage, res: ServerResponse) {
  res.setHeader('Content-Type', 'application/json');
  res.setHeader('Cache-Control', 'no-store');
  // The local compiler endpoint accepts same-origin requests only.
  if (req.headers.origin && req.headers.origin !== `http://${req.headers.host}` && req.headers.origin !== `https://${req.headers.host}`) {
    res.statusCode = 403; res.end(JSON.stringify({ error: 'Cross-origin requests are not allowed.' })); return;
  }
  try {
    if (req.method === 'GET' && req.url === '/api/native/examples') {
      const examples = await Promise.all(['factorial', 'max', 'optimizer', 'novelty'].map(async name => ({ name, source: await readFile(join(root, 'examples', `${name}.mp`), 'utf8') })));
      res.end(JSON.stringify(examples)); return;
    }
    if (req.method !== 'POST' || req.url !== '/api/native/compile') { res.statusCode = 404; res.end('{}'); return; }
    if (!req.headers['content-type']?.startsWith('application/json')) { res.statusCode = 415; res.end('{}'); return; }
    let body = '';
    for await (const chunk of req) { body += chunk; if (Buffer.byteLength(body) > 80000) throw new Error('Request too large.'); }
    const { source, trace = false } = JSON.parse(body);
    if (typeof source !== 'string' || source.length > 20000) throw new Error('Source must contain at most 20,000 characters.');
    const directory = await mkdtemp(join(tmpdir(), 'optitac-'));
    try {
      const file = join(directory, 'program.mp');
      await writeFile(file, source);
      const artifacts: Record<string, Awaited<ReturnType<typeof execute>>> = {};
      for (const phase of phases) {
        artifacts[phase] = await execute(['--phase', phase, '--compile-only', file]);
        if (!artifacts[phase].success) break;
      }
      const original = await execute(['--run', '--no-opt', '--max-steps', '100000', file]);
      const optimized = await execute(['--run', '--stats', '--max-steps', '100000', ...(trace ? ['--trace'] : []), file]);
      // Compare observations without the optional statistics or trace presentation.
      const observed = await execute(['--run', '--max-steps', '100000', file]);
      res.end(JSON.stringify({ artifacts, original, optimized, preserved: original.success === observed.success && original.stdout === observed.stdout && original.stderr === observed.stderr }));
    } finally { await rm(directory, { recursive: true, force: true }); }
  } catch (error) {
    res.statusCode = 400; res.end(JSON.stringify({ error: error instanceof Error ? error.message : String(error) }));
  }
}
export function nativeCompiler(): Plugin {
  const middleware = (req: IncomingMessage, res: ServerResponse, next: () => void) => {
    if (req.url?.startsWith('/api/native/')) void handle(req, res); else next();
  };
  return { name: 'optitac-native', configureServer(server) { server.middlewares.use(middleware); }, configurePreviewServer(server) { server.middlewares.use(middleware); } };
}
