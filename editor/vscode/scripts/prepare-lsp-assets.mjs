#!/usr/bin/env node

import * as fs from 'node:fs';
import * as path from 'node:path';
import { fileURLToPath } from 'node:url';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const extensionDir = path.resolve(scriptDir, '..');
const repoRoot = path.resolve(extensionDir, '..', '..');
const outLspDir = path.join(extensionDir, 'out', 'lsp');
const buildLspWasmDir = path.join(repoRoot, 'build', 'lsp-wasm');

fs.mkdirSync(outLspDir, { recursive: true });

// Mark the lsp output directory as an ES module package so Node loads
// lsp-wasm-runner.js correctly when spawned by the extension host.
fs.writeFileSync(
    path.join(outLspDir, 'package.json'),
    JSON.stringify({ type: 'module' }),
);

const FILES_TO_BUNDLE = [
    'cowel-wasm.js',
    'lsp-wasm-runner.js',
    'cowel-lsp.wasm',
    // The point of `cowel.wasm` being on the list is actually to remove it
    // in case it ever makes it there.
    // TODO: Investigate why it can make it into `out` in the first place.
    'cowel.wasm',
];

// Copy pre-built ESM assets from build/lsp-wasm/ if present.
for (const name of FILES_TO_BUNDLE) {
    const src = path.join(buildLspWasmDir, name);
    const dst = path.join(outLspDir, name);
    if (fs.existsSync(src)) {
        fs.copyFileSync(src, dst);
    } else if (fs.existsSync(dst)) {
        fs.rmSync(dst);
    }
}
