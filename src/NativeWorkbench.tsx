import { useEffect, useRef, useState } from 'react';
import './NativeWorkbench.css';

type Execution = { stdout: string; stderr: string; success: boolean };
type Evidence = { artifacts: Record<string, Execution>; original: Execution; optimized: Execution; preserved: boolean };
const stages = [
  { id: 'tokens', title: 'Tokens', description: 'The lexer identifies keywords, function names, identifiers, integer literals, operators and punctuation.' },
  { id: 'ast', title: 'Syntax tree', description: 'The parser groups expressions and statements according to the grammar.' },
  { id: 'symbols', title: 'Symbol table', description: 'Declared functions, parameters and variables, with semantic checks.' },
  { id: 'records', title: 'Activation records', description: 'Frame offsets for return information, parameters, locals and temporaries.' },
  { id: 'tac', title: 'Three-address code', description: 'The original intermediate instructions before optimization.' },
  { id: 'quads', title: 'Quadruples', description: 'The same original instructions in operator, argument and result form.' },
  { id: 'optimized', title: 'Optimized TAC', description: 'The result of the eight optimizer transformations.' },
];
const initialSource = 'int main() {\n    int answer;\n    answer = 2 + 3 * 4;\n    return answer;\n}';
function output(run: Execution) { return run.stdout + run.stderr; }
function result(run: Execution) {
  return run.success ? run.stdout.match(/^Result: (.+)$/m)?.[1] ?? 'Completed' : run.stderr.trim().split('\n')[0] || 'Compilation stopped';
}
function saveFile(name: string, content: string, type: string) {
  const url = URL.createObjectURL(new Blob([content], { type }));
  const link = document.createElement('a'); link.href = url; link.download = name; link.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}
export function NativeWorkbench() {
  const [examples, setExamples] = useState<{ name: string; source: string }[]>([]);
  const [source, setSource] = useState(initialSource);
  const [example, setExample] = useState('');
  const [evidence, setEvidence] = useState<Evidence>();
  const [compiled, setCompiled] = useState('');
  const [compiledTrace, setCompiledTrace] = useState(false);
  const [trace, setTrace] = useState(false);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState('');
  const [fileError, setFileError] = useState('');
  const fileInput = useRef<HTMLInputElement>(null);
  const demoGuide = useRef<HTMLDetailsElement>(null);
  useEffect(() => {
    const controller = new AbortController();
    fetch('/api/native/examples', { signal: controller.signal }).then(r => {
      if (!r.ok) throw new Error('Examples unavailable');
      return r.json();
    }).then(setExamples).catch(failure => {
      if (failure.name !== 'AbortError') setFileError('Examples could not load. You can still enter code and run it. Start the app with npm run dev.');
    });
    return () => controller.abort();
  }, []);
  async function compile() {
    const submitted = source, submittedTrace = trace;
    setBusy(true); setError(''); setEvidence(undefined);
    try {
      const response = await fetch('/api/native/compile', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ source: submitted, trace: submittedTrace }) });
      if (!response.headers.get('content-type')?.includes('application/json')) throw new Error('The local compiler is unavailable. Start the project with npm run dev.');
      const data = await response.json();
      if (!response.ok) throw new Error(data.error || 'Compilation request failed.');
      setEvidence(data); setCompiled(submitted); setCompiledTrace(submittedTrace);
    } catch (failure) { setError(failure instanceof Error ? failure.message : String(failure)); }
    finally { setBusy(false); }
  }
  async function loadFile(file?: File) {
    if (!file) return;
    setFileError('');
    if (file.size > 80000) { setFileError('Choose a source file smaller than 80 KB (at most 20,000 characters).'); return; }
    try {
      const text = await file.text();
      if (text.length > 20000) throw new Error('Source must contain at most 20,000 characters.');
      setSource(text); setExample('');
    } catch (failure) { setFileError(failure instanceof Error ? failure.message : 'Could not read this file.'); }
  }
  const stale = Boolean(evidence && (source !== compiled || trace !== compiledTrace));
  const failedStage = evidence && stages.find(stage => evidence.artifacts[stage.id] && !evidence.artifacts[stage.id].success);
  const statsMarker = '===== OPTIMIZER STATISTICS =====';
  const statsIndex = evidence?.optimized.stdout.indexOf(statsMarker) ?? -1;
  const executionOutput = evidence ? (statsIndex >= 0 ? evidence.optimized.stdout.slice(0, statsIndex) : evidence.optimized.stdout) + evidence.optimized.stderr : '';
  const fullOutput = evidence ? [
    ...stages.filter(stage => evidence.artifacts[stage.id]).map(stage => output(evidence.artifacts[stage.id])),
    '===== ORIGINAL EXECUTION =====\n' + output(evidence.original),
    '===== OPTIMIZED EXECUTION =====\n' + output(evidence.optimized),
  ].join('\n\n') : '';
  return <section className="workbench" aria-labelledby="workbench-title">
    <div className="workbench-heading"><div><p className="overline">MINIPROC COMPILER</p><h1 id="workbench-title">Code in. Every stage out.</h1><p>Enter a program, run the compiler, then follow its complete output below.</p></div><a className="demo-link" href="#demo-guide" onClick={() => { if (demoGuide.current) demoGuide.current.open = true; }}>Teacher demo guide</a></div>
    <div className="workbench-layout">
      <section className="workbench-editor" aria-labelledby="source-title">
        <h2 id="source-title">1. Enter your program</h2>
        <div className="source-tools"><label htmlFor="native-example">Load an example<select id="native-example" value={example} disabled={busy} onChange={event => { const selected = examples.find(item => item.name === event.target.value); setExample(event.target.value); if (selected) setSource(selected.source); }}><option value="">Your own code</option>{examples.map(item => <option key={item.name} value={item.name}>{item.name}</option>)}</select></label><button type="button" disabled={busy} onClick={() => fileInput.current?.click()}>Open .mp file</button><input ref={fileInput} hidden type="file" accept=".mp,.txt,.c" onChange={event => { void loadFile(event.target.files?.[0]); event.target.value = ''; }} /></div>
        <label className="field-label" htmlFor="native-source">MiniProc source</label><textarea id="native-source" value={source} disabled={busy} onChange={event => { setSource(event.target.value); setExample(''); }} spellCheck={false} />
        <p className="language-help">Supports integer functions, calls, recursion, if/else and while. Declare variables with <code>int x;</code>, then assign with <code>x = 5;</code>.</p>
        <label className="trace-option"><input type="checkbox" disabled={busy} checked={trace} onChange={event => setTrace(event.target.checked)} /> Show procedure call and frame trace</label>
        <button className="run-compiler" disabled={busy || !source.trim() || source.length > 20000} onClick={() => void compile()}>{busy ? 'Running compiler…' : '2. Run compiler'}</button>
        {source.length > 20000 && <p role="alert" className="error">Source must contain at most 20,000 characters.</p>}
        {fileError && <p role="alert" className="error">{fileError}</p>}
        <details className="language-reference"><summary>Language reference and terminal commands</summary><p>This is the MiniProc language, a small integer subset. Use <code>0 - n</code> for negative values. Arrays, floats, strings, printf and #include are outside its scope.</p><p>For the full showcase in Terminal:</p><pre>make{'\n'}./build/mpc examples/novelty.mp{'\n'}./build/mpc --stats examples/novelty.mp</pre><p><code>--run</code> shows only execution. Omit it to print every phase.</p></details>
      </section>
      <section className="workbench-results" aria-labelledby="results-title" aria-busy={busy}>
        <h2 id="results-title">3. Read the complete output</h2>
        {error && <p role="alert" className="error">{error}</p>}
        {busy && <p role="status" className="workbench-empty">Compiling your program and comparing both executions…</p>}
        {!busy && !evidence && !error && <div className="workbench-empty"><h3>Ready when you are</h3><p>Click <strong>Run compiler</strong> to see tokens, syntax tree, symbols, frames, TAC, quadruples, optimized TAC and execution.</p><p>Start with the code on the left. It returns <strong>14</strong>.</p></div>}
        {evidence && <>
          <p role="status" className={`run-verdict ${!evidence.preserved || !evidence.original.success ? 'run-warning' : ''}`}>{stale ? 'Code or trace settings changed. Run again to refresh these outputs. ' : ''}{failedStage ? 'Compilation stopped. Inspect the diagnostics in the available output below.' : evidence.preserved ? evidence.original.success ? 'Completed. Original and optimized results match.' : 'Both executions report the same runtime error.' : 'Execution observations differ. Inspect both outputs.'}</p>
          <div className="result-comparison"><div><span>Original result</span><strong>{result(evidence.original)}</strong></div><div><span>Optimized result</span><strong>{result(evidence.optimized)}</strong></div></div>
          <nav className="output-jumps" aria-label="Jump to compiler output">{stages.map(stage => evidence.artifacts[stage.id] ? <a key={stage.id} href={`#output-${stage.id}`}>{stage.title}</a> : <span key={stage.id}>{stage.title} (not reached)</span>)}<a href="#output-execution">Execution</a>{statsIndex >= 0 && <a href="#output-statistics">Statistics</a>}</nav>
          <div className="output-downloads"><button onClick={() => saveFile('miniproc-full-output.txt', fullOutput, 'text/plain')}>Download full output</button><button onClick={() => saveFile('miniproc-evidence.json', JSON.stringify({ engine: 'MiniProc', semantics: 'checked C int; truncating division', source: compiled, trace: compiledTrace, ...evidence }, null, 2), 'application/json')}>Export evidence JSON</button></div>
          {stages.map((stage, index) => { const run = evidence.artifacts[stage.id]; return run && <section className="compiler-output" id={`output-${stage.id}`} key={stage.id}><h3><span>{index + 1}</span> {stage.title}</h3><p>{stage.description}</p><pre className={!run.success ? 'failed-output' : ''}>{output(run) || 'No output.'}</pre></section>; })}
          <section className="compiler-output" id="output-execution"><h3><span>8</span> Execution comparison</h3><p>The same program runs before and after optimization.</p><h4>Original execution</h4><pre>{output(evidence.original)}</pre><h4>Optimized execution{compiledTrace ? ' with call trace' : ''}</h4><pre>{executionOutput}</pre></section>
          {statsIndex >= 0 && <section className="compiler-output" id="output-statistics"><h3><span>9</span> Optimization statistics</h3><p>Instruction counts and executed VM steps, followed by transformation counters.</p><pre>{evidence.optimized.stdout.slice(statsIndex)}</pre><p>VM steps measure interpreter work, not wall-clock speed.</p></section>}
        </>}
      </section>
    </div>
    <details ref={demoGuide} className="demo-guide" id="demo-guide"><summary>Teacher demonstration: one route, about five minutes</summary><ol><li><strong>Show the pipeline.</strong> Run the initial program. Explain tokens, syntax tree and symbols, then compare original and optimized TAC. Both return 14.</li><li><strong>Show optimization.</strong> Load <code>novelty</code> and run. Both return 100. Statistics show 36 to 19 instructions and 1,915 to 1,210 VM steps.</li><li><strong>Show procedures.</strong> Load <code>factorial</code>, enable the trace and run. Explain the frame layout and recursive calls. Result: 1144.</li><li><strong>Show a live edit.</strong> Enter your own valid MiniProc program and run. For error preservation, try <code>int main() &#123; return 1 / 0; &#125;</code>. Both executions must fail.</li><li><strong>Finish with the learning view.</strong> Open Explore to show the town, replay and challenges. Return to Workbench for the same saved native program.</li></ol></details>
  </section>;
}
