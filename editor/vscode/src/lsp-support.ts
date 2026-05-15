import * as fs from 'node:fs';
import * as path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

import {
    Diagnostic,
    DiagnosticSeverity,
    Position,
    PositionEncodingKind,
    Range,
} from 'vscode-languageserver/node';
import { TextDocument } from 'vscode-languageserver-textdocument';

export interface CowelDiagnosticLocation {
    fileName: string;
    fileId: number;
    begin: number;
    length: number;
    line: number;
    column: number;
}

export interface CowelDiagnostic {
    severity: number;
    id: string;
    message: string;
    stack: CowelDiagnosticLocation[];
}

export interface CowelFileResult {
    status: number;
    data?: Buffer;
    id: number;
}

export interface IoStatusLike {
    ok: number;
    error: number;
    not_found: number;
}

export interface GenerateDocumentRequest {
    source: string;
    loadFile(path: string, baseFileId: number): CowelFileResult;
    log(diagnostic: CowelDiagnostic): void;
}

export interface CollectDiagnosticsRequest {
    document: TextDocument;
    ioStatus: IoStatusLike;
    positionEncoding: PositionEncodingKind;
    generateDocument(request: GenerateDocumentRequest): Promise<void>;
}

type LoadedFile = {
    path: string;
    data: Buffer;
    text: string;
};

const COWEL_SEVERITY_SOFT_WARNING = 40;
const COWEL_SEVERITY_WARNING = 50;
const COWEL_SEVERITY_ERROR = 70;
const COWEL_SEVERITY_NONE = 100;

export function severityToDiagnosticSeverity(severity: number): DiagnosticSeverity | undefined {
    if (severity >= COWEL_SEVERITY_ERROR && severity < COWEL_SEVERITY_NONE) {
        return DiagnosticSeverity.Error;
    }
    if (severity >= COWEL_SEVERITY_WARNING && severity < COWEL_SEVERITY_ERROR) {
        return DiagnosticSeverity.Warning;
    }
    if (severity >= COWEL_SEVERITY_SOFT_WARNING && severity < COWEL_SEVERITY_WARNING) {
        return DiagnosticSeverity.Information;
    }
    return undefined;
}

export function createFailureDiagnostic(message: string): Diagnostic {
    const range = Range.create(Position.create(0, 0), Position.create(0, 0));
    return {
        range,
        severity: DiagnosticSeverity.Error,
        source: 'cowel',
        message,
    };
}

export function rangeForDiagnosticLocation(
    text: string,
    location: CowelDiagnosticLocation,
    positionEncoding: PositionEncodingKind,
): Range {
    const start = positionAtUtf8Offset(text, location.begin, positionEncoding);
    const end = positionAtUtf8Offset(text, location.begin + location.length, positionEncoding);
    return Range.create(start, end);
}

export async function collectDiagnosticsForDocument(
    request: CollectDiagnosticsRequest,
): Promise<Map<string, Diagnostic[]>> {
    const documentPath = fileURLToPath(request.document.uri);
    const loadedFiles: LoadedFile[] = [];
    const fileTexts = new Map<string, string>([
        [request.document.uri, request.document.getText()],
    ]);
    const diagnosticsByUri = new Map<string, Diagnostic[]>();

    const loadFile = (unresolvedPath: string, baseFileId: number): CowelFileResult => {
        const basePath = baseFileId < 0
            ? documentPath
            : loadedFiles[baseFileId]?.path ?? documentPath;
        const resolvedPath = path.resolve(path.dirname(basePath), unresolvedPath);

        try {
            const data = fs.readFileSync(resolvedPath);
            const text = data.toString('utf8');
            const id = loadedFiles.length;
            loadedFiles.push({ path: resolvedPath, data, text });
            fileTexts.set(pathToFileURL(resolvedPath).toString(), text);
            return {
                status: request.ioStatus.ok,
                data,
                id,
            };
        } catch (error) {
            const code = (error as NodeJS.ErrnoException).code;
            return {
                status: code === 'ENOENT' ? request.ioStatus.not_found : request.ioStatus.error,
                id: -1,
            };
        }
    };

    const loggedDiagnostics: CowelDiagnostic[] = [];
    await request.generateDocument({
        source: request.document.getText(),
        loadFile,
        log(diagnostic) {
            loggedDiagnostics.push(diagnostic);
        },
    });

    for (const diagnostic of loggedDiagnostics) {
        const severity = severityToDiagnosticSeverity(diagnostic.severity);
        if (severity === undefined) {
            continue;
        }

        const primaryLocation = diagnostic.stack.length === 0 ? undefined : diagnostic.stack[0];
        const uri = resolveDiagnosticUri(
            request.document.uri,
            documentPath,
            primaryLocation,
            loadedFiles,
        );
        const sourceText = fileTexts.get(uri) ?? request.document.getText();
        const range = primaryLocation === undefined
            ? Range.create(Position.create(0, 0), Position.create(0, 0))
            : rangeForDiagnosticLocation(sourceText, primaryLocation, request.positionEncoding);

        const bucket = diagnosticsByUri.get(uri) ?? [];
        bucket.push({
            range,
            severity,
            source: 'cowel',
            code: diagnostic.id,
            message: diagnostic.message,
        });
        diagnosticsByUri.set(uri, bucket);
    }

    return diagnosticsByUri;
}

function resolveDiagnosticUri(
    mainDocumentUri: string,
    mainDocumentPath: string,
    location: CowelDiagnosticLocation | undefined,
    loadedFiles: LoadedFile[],
): string {
    if (location === undefined) {
        return mainDocumentUri;
    }
    if (location.fileId >= 0 && location.fileId < loadedFiles.length) {
        return pathToFileURL(loadedFiles[location.fileId].path).toString();
    }
    if (location.fileName.length === 0) {
        return mainDocumentUri;
    }

    const resolvedPath = path.isAbsolute(location.fileName)
        ? location.fileName
        : path.resolve(path.dirname(mainDocumentPath), location.fileName);
    return pathToFileURL(resolvedPath).toString();
}

function positionAtUtf8Offset(
    text: string,
    utf8Offset: number,
    positionEncoding: PositionEncodingKind,
): Position {
    const clampedOffset = Math.max(0, Math.min(Buffer.byteLength(text, 'utf8'), utf8Offset));
    let consumedUtf8 = 0;
    let line = 0;
    let character = 0;

    for (let i = 0; i < text.length && consumedUtf8 < clampedOffset;) {
        const codePoint = text.codePointAt(i);
        if (codePoint === undefined) {
            break;
        }

        const chunk = String.fromCodePoint(codePoint);
        const utf16Length = chunk.length;
        const utf8Length = Buffer.byteLength(chunk, 'utf8');

        if (consumedUtf8 + utf8Length > clampedOffset) {
            break;
        }

        consumedUtf8 += utf8Length;
        i += utf16Length;

        if (chunk === '\n') {
            line++;
            character = 0;
            continue;
        }

        character += positionEncoding === PositionEncodingKind.UTF8
            ? utf8Length
            : utf16Length;
    }

    return Position.create(line, character);
}
