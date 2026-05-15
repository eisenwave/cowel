import * as fs from 'node:fs';
import * as path from 'node:path';

import {
    createConnection,
    Diagnostic,
    InitializeParams,
    InitializeResult,
    PositionEncodingKind,
    ProposedFeatures,
    TextDocumentSyncKind,
    TextDocuments,
} from 'vscode-languageserver/node';
import { TextDocument } from 'vscode-languageserver-textdocument';

import {
    CowelDiagnostic,
    CowelFileResult,
    IoStatusLike,
    collectDiagnosticsForDocument,
    createFailureDiagnostic,
} from './lsp-support';

interface CowelModeLike {
    document: number;
}

interface CowelSeverityLike {
    min: number;
}

interface CowelSyntaxHighlightPolicyLike {
    fall_back: number;
}

interface CowelWasmInstanceLike {
    generateHtml(options: {
        source: string;
        mode: number;
        minSeverity: number;
        highlightPolicy: number;
        enableXHighlighting: boolean;
        loadFile(path: string, baseFileId: number): CowelFileResult;
        log(diagnostic: CowelDiagnostic): void;
    }): Promise<unknown>;
}

interface CowelWasmModuleLike {
    Mode: CowelModeLike;
    Severity: CowelSeverityLike;
    SyntaxHighlightPolicy: CowelSyntaxHighlightPolicyLike;
    IoStatus: IoStatusLike;
    load(module: BufferSource): Promise<CowelWasmInstanceLike>;
}

type LoadedWasm = {
    api: CowelWasmModuleLike;
    wasm: CowelWasmInstanceLike;
};

const connection = createConnection(ProposedFeatures.all);
const documents = new TextDocuments(TextDocument);
const publishedUrisByDocument = new Map<string, Set<string>>();

let negotiatedPositionEncoding = PositionEncodingKind.UTF16;
let wasmPromise: Promise<LoadedWasm> | null = null;

connection.onInitialize((params: InitializeParams): InitializeResult => {
    negotiatedPositionEncoding = choosePositionEncoding(params);

    return {
        capabilities: {
            textDocumentSync: TextDocumentSyncKind.Incremental,
            positionEncoding: negotiatedPositionEncoding,
        },
        serverInfo: {
            name: 'cowel',
        },
    };
});

documents.onDidOpen((event) => {
    void validateDocument(event.document);
});

documents.onDidChangeContent((event) => {
    void validateDocument(event.document);
});

documents.onDidClose((event) => {
    clearPublishedDiagnostics(event.document.uri);
});

documents.listen(connection);
connection.listen();

function choosePositionEncoding(params: InitializeParams): PositionEncodingKind {
    const supported = params.capabilities.general?.positionEncodings;
    return supported?.includes(PositionEncodingKind.UTF8)
        ? PositionEncodingKind.UTF8
        : PositionEncodingKind.UTF16;
}

async function validateDocument(document: TextDocument): Promise<void> {
    if (document.languageId !== 'cowel' || !document.uri.startsWith('file://')) {
        clearPublishedDiagnostics(document.uri);
        return;
    }

    const expectedVersion = document.version;

    try {
        const loadedWasm = await loadWasm();
        if (documents.get(document.uri)?.version !== expectedVersion) {
            return;
        }

        const diagnosticsByUri = await collectDiagnosticsForDocument({
            document,
            ioStatus: loadedWasm.api.IoStatus,
            positionEncoding: negotiatedPositionEncoding,
            async generateDocument({ source, loadFile, log }) {
                await loadedWasm.wasm.generateHtml({
                    source,
                    mode: loadedWasm.api.Mode.document,
                    minSeverity: loadedWasm.api.Severity.min,
                    highlightPolicy: loadedWasm.api.SyntaxHighlightPolicy.fall_back,
                    enableXHighlighting: false,
                    loadFile,
                    log,
                });
            },
        });
        if (documents.get(document.uri)?.version !== expectedVersion) {
            return;
        }

        publishDiagnostics(document.uri, diagnosticsByUri);
    } catch (error) {
        if (documents.get(document.uri)?.version !== expectedVersion) {
            return;
        }

        const message = error instanceof Error ? error.message : String(error);
        publishDiagnostics(document.uri, new Map([
            [document.uri, [createFailureDiagnostic(message)]],
        ]));
        connection.console.error(error instanceof Error ? error.stack ?? error.message : message);
    }
}

async function loadWasm(): Promise<LoadedWasm> {
    if (wasmPromise === null) {
        wasmPromise = (async () => {
            const runtimePaths = resolveRuntimePaths();
            // prepare-lsp-assets.mjs transpiles bindings/node/src/cowel-wasm.ts into CommonJS
            // so the server can reuse the existing WASM wrapper without copying its source.
            const api = require(runtimePaths.modulePath) as CowelWasmModuleLike;
            const moduleBytes = fs.readFileSync(runtimePaths.wasmPath);
            const wasm = await api.load(moduleBytes);
            return { api, wasm };
        })();
    }

    return wasmPromise;
}

function resolveRuntimePaths(): { modulePath: string; wasmPath: string } {
    const packagedModulePath = path.join(__dirname, 'lsp', 'cowel-wasm.js');
    const packagedWasmPath = path.join(__dirname, 'lsp', 'cowel.wasm');
    const repoRoot = path.resolve(__dirname, '../../..');
    const repoModulePath = path.join(repoRoot, 'build', 'npm', 'cowel-wasm.js');
    const repoWasmPath = path.join(repoRoot, 'build', 'npm', 'cowel.wasm');

    const modulePath = fs.existsSync(packagedModulePath) ? packagedModulePath : repoModulePath;
    const wasmPath = fs.existsSync(packagedWasmPath) ? packagedWasmPath : repoWasmPath;

    if (!fs.existsSync(modulePath) || !fs.existsSync(wasmPath)) {
        throw new Error(
            'Missing COWEL WASM assets. Build the WASM package so build/npm/cowel.wasm exists.',
        );
    }

    return { modulePath, wasmPath };
}

function publishDiagnostics(sourceUri: string, diagnosticsByUri: Map<string, Diagnostic[]>): void {
    const previousUris = publishedUrisByDocument.get(sourceUri) ?? new Set<string>();
    const nextUris = new Set(diagnosticsByUri.keys());

    for (const uri of previousUris) {
        if (!nextUris.has(uri)) {
            connection.sendDiagnostics({ uri, diagnostics: [] });
        }
    }

    for (const [uri, diagnostics] of diagnosticsByUri) {
        connection.sendDiagnostics({ uri, diagnostics });
    }

    publishedUrisByDocument.set(sourceUri, nextUris);
}

function clearPublishedDiagnostics(sourceUri: string): void {
    const previousUris = publishedUrisByDocument.get(sourceUri);
    if (previousUris === undefined) {
        return;
    }

    for (const uri of previousUris) {
        connection.sendDiagnostics({ uri, diagnostics: [] });
    }
    publishedUrisByDocument.delete(sourceUri);
}
