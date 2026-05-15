import assert from 'node:assert/strict';
import * as fs from 'node:fs';
import * as os from 'node:os';
import * as path from 'node:path';
import { pathToFileURL } from 'node:url';

import {
    DiagnosticSeverity,
    PositionEncodingKind,
} from 'vscode-languageserver/node';
import { TextDocument } from 'vscode-languageserver-textdocument';

import {
    collectDiagnosticsForDocument,
    rangeForDiagnosticLocation,
    severityToDiagnosticSeverity,
} from '../src/lsp-support';

const colors = {
    reset: '\x1b[0m',
    bright: '\x1b[1m',
    red: '\x1b[91m',
    green: '\x1b[92m',
} as const;

type TestCase = {
    name: string;
    run(): Promise<void> | void;
};

const tests: TestCase[] = [
    {
        name: 'severity ranges match VSCode diagnostic severities',
        run() {
            assert.equal(severityToDiagnosticSeverity(39), undefined);
            assert.equal(severityToDiagnosticSeverity(40), DiagnosticSeverity.Information);
            assert.equal(severityToDiagnosticSeverity(49), DiagnosticSeverity.Information);
            assert.equal(severityToDiagnosticSeverity(50), DiagnosticSeverity.Warning);
            assert.equal(severityToDiagnosticSeverity(69), DiagnosticSeverity.Warning);
            assert.equal(severityToDiagnosticSeverity(70), DiagnosticSeverity.Error);
            assert.equal(severityToDiagnosticSeverity(99), DiagnosticSeverity.Error);
            assert.equal(severityToDiagnosticSeverity(100), undefined);
        },
    },
    {
        name: 'UTF-8 and UTF-16 ranges are computed from UTF-8 byte offsets',
        run() {
            const text = 'prefix\né🙂z';
            const start = Buffer.byteLength('prefix\né', 'utf8');
            const length = Buffer.byteLength('🙂', 'utf8');

            const utf8Range = rangeForDiagnosticLocation(text, {
                fileName: '',
                fileId: -1,
                begin: start,
                length,
                line: 1,
                column: 0,
            }, PositionEncodingKind.UTF8);
            assert.deepEqual(utf8Range, {
                start: { line: 1, character: 2 },
                end: { line: 1, character: 6 },
            });

            const utf16Range = rangeForDiagnosticLocation(text, {
                fileName: '',
                fileId: -1,
                begin: start,
                length,
                line: 1,
                column: 0,
            }, PositionEncodingKind.UTF16);
            assert.deepEqual(utf16Range, {
                start: { line: 1, character: 1 },
                end: { line: 1, character: 3 },
            });
        },
    },
    {
        name: 'include diagnostics are published for the resolved include file',
        async run() {
            const tempDir = fs.mkdtempSync(path.join(os.tmpdir(), 'cowel-lsp-'));
            try {
                const includePath = path.join(tempDir, 'include.cow');
                fs.writeFileSync(includePath, '🙂');

                const mainPath = path.join(tempDir, 'main.cow');
                const mainUri = pathToFileURL(mainPath).toString();
                const document = TextDocument.create(mainUri, 'cowel', 1, '\\include{include.cow}');

                const diagnosticsByUri = await collectDiagnosticsForDocument({
                    document,
                    ioStatus: {
                        ok: 0,
                        error: 1,
                        not_found: 2,
                    },
                    positionEncoding: PositionEncodingKind.UTF8,
                    async generateDocument({ loadFile, log }) {
                        const loaded = loadFile('include.cow', -1);
                        assert.equal(loaded.status, 0);
                        assert.ok(loaded.data);

                        log({
                            severity: 50,
                            id: 'include.warning',
                            message: 'Include warning',
                            stack: [{
                                fileName: '',
                                fileId: loaded.id,
                                begin: 0,
                                length: Buffer.byteLength('🙂', 'utf8'),
                                line: 0,
                                column: 0,
                            }],
                        });

                        log({
                            severity: 30,
                            id: 'ignored.info',
                            message: 'Ignored',
                            stack: [],
                        });
                    },
                },
                );

                assert.deepEqual(
                    [...diagnosticsByUri.keys()],
                    [pathToFileURL(includePath).toString()],
                );
                assert.equal(
                    diagnosticsByUri.get(pathToFileURL(includePath).toString())?.[0].severity,
                    DiagnosticSeverity.Warning,
                );
            } finally {
                fs.rmSync(tempDir, { recursive: true, force: true });
            }
        },
    },
];

let passed = 0;
let failed = 0;

async function main(): Promise<void> {
    for (const test of tests) {
        try {
            await test.run();
            console.log(`${colors.green}${colors.bright}OK:${colors.reset} ${test.name}`);
            passed++;
        } catch (error) {
            console.log(`${colors.red}${colors.bright}FAIL:${colors.reset} ${test.name}`);
            console.error(error);
            failed++;
        }
    }

    console.log(`\n${passed + failed} tests: ${passed} passed, ${failed} failed`);
    process.exit(failed > 0 ? 1 : 0);
}

void main();
