import { readdir, mkdtemp, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { spawnSync } from 'node:child_process';
import { build } from 'esbuild';

const entries = (await readdir(new URL('.', import.meta.url)))
  .filter((name) => name.endsWith('.test.ts'))
  .map((name) => new URL(name, import.meta.url).pathname);
if (entries.length === 0) throw new Error('No UI tests found');

const output = await mkdtemp(join(tmpdir(), 't3k-ui-tests-'));
try {
  await build({ entryPoints: entries, outdir: output, outExtension: { '.js': '.mjs' },
    bundle: true, platform: 'node', format: 'esm', logLevel: 'silent' });
  const files = (await readdir(output)).filter((name) => name.endsWith('.test.mjs'));
  const result = spawnSync(process.execPath, ['--test', ...files.map((name) => join(output, name))],
    { stdio: 'inherit' });
  process.exitCode = result.status ?? 1;
} finally {
  await rm(output, { recursive: true, force: true });
}
