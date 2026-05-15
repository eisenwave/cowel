#!/usr/bin/env node

import * as fs from 'node:fs';
import * as path from 'node:path';
import { fileURLToPath } from 'node:url';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const extensionDir = path.resolve(scriptDir, '..');
const repoRoot = path.resolve(extensionDir, '..', '..');
const outLspDir = path.join(extensionDir, 'out', 'lsp');
const buildNpmDir = path.join(repoRoot, 'build', 'npm');

fs.mkdirSync(outLspDir, { recursive: true });

// Mark the lsp output directory as an ES module package so Node loads
// lsp-wasm-runner.js correctly when spawned by the extension host.
fs.writeFileSync(
    path.join(outLspDir, 'package.json'),
    JSON.stringify({ type: 'module' }),
);

// Copy pre-built ESM assets from build/npm/ if present.
for (const name of ['cowel-wasm.js', 'lsp-wasm-runner.js', 'cowel-lsp.wasm']) {
    const src = path.join(buildNpmDir, name);
    const dst = path.join(outLspDir, name);
    if (fs.existsSync(src)) {
        fs.copyFileSync(src, dst);
    } else if (fs.existsSync(dst)) {
        fs.rmSync(dst);
    }
}
