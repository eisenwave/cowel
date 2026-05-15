#!/usr/bin/env node

import * as fs from 'node:fs';
import * as path from 'node:path';
import { spawnSync } from 'node:child_process';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';

const require = createRequire(import.meta.url);
const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const extensionDir = path.resolve(scriptDir, '..');
const repoRoot = path.resolve(extensionDir, '..', '..');
const outDir = path.join(extensionDir, 'out', 'lsp');
const cowelWasmTsPath = path.join(repoRoot, 'bindings', 'node', 'src', 'cowel-wasm.ts');
const repoWasmPath = path.join(repoRoot, 'build', 'npm', 'cowel.wasm');
const packagedWasmPath = path.join(outDir, 'cowel.wasm');

fs.mkdirSync(outDir, { recursive: true });

const tscPath = require.resolve('typescript/bin/tsc');
const compileResult = spawnSync(
    process.execPath,
    [
        tscPath,
        cowelWasmTsPath,
        '--module',
        'commonjs',
        '--target',
        'ES2022',
        '--lib',
        'ES2022,DOM',
        '--moduleResolution',
        'node',
        '--esModuleInterop',
        '--skipLibCheck',
        '--outDir',
        outDir,
    ],
    { stdio: 'inherit' },
);

if (compileResult.status !== 0) {
    process.exit(compileResult.status ?? 1);
}

if (fs.existsSync(repoWasmPath)) {
    fs.copyFileSync(repoWasmPath, packagedWasmPath);
} else if (fs.existsSync(packagedWasmPath)) {
    fs.rmSync(packagedWasmPath);
}
