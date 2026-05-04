#!/usr/bin/env node
import path from "path";
import fs from "fs";
import process from "process";
import { fileURLToPath } from "url";

import * as cowel from "./cowel-wasm.js";

function readModuleFileSync(file: string): NonSharedBuffer;
function readModuleFileSync(file: string, encoding: BufferEncoding): string;
function readModuleFileSync(file: string, encoding?: BufferEncoding): string | NonSharedBuffer {
    const moduleDir = path.dirname(fileURLToPath(import.meta.url));
    const resolved = path.resolve(moduleDir, file);
    return fs.readFileSync(resolved, {
        encoding,
    });
}

function getVersion(): string {
    const p = JSON.parse(readModuleFileSync("package.json", "utf8"));
    return p.version;
}

enum AnsiCode {
    reset = "\x1b[0m",
    black = "\x1B[30m",
    red = "\x1B[31m",
    green = "\x1B[32m",
    yellow = "\x1B[33m",
    blue = "\x1B[34m",
    magenta = "\x1B[35m",
    cyan = "\x1B[36m",
    white = "\x1B[37m",
    highBlack = "\x1B[0;90m",
    highRed = "\x1B[0;91m",
    highGreen = "\x1B[0;92m",
    highYellow = "\x1B[0;93m",
    highBlue = "\x1B[0;94m",
    highMagenta = "\x1B[0;95m",
    highCyan = "\x1B[0;96m",
    highWhite = "\x1B[0;97m",
}

const helpText = `\
Usage: cowel run <input> <output> [options]

Commands:
  run <input> <output>  Processes a COWEL document

Options:
  -h, --help            Display this help menu
  -v, --version         Show version
  -l, --severity        Minimum (>=) severity for log messages
                        Choices: min, trace, debug, info, soft_warning,
                                 warning, error, fatal, none
                        Default: info
      --no-color        Disable colored output
`;

type LoadedFile = {
    path: string;
    data: Buffer;
};

const files: LoadedFile[] = [];
let mainFile: LoadedFile;

let mainDocumentDir = "";
let colorsEnabled = true;
let wasm: cowel.CowelWasm | undefined;

function loadFile(unresolvedPath: string, baseFileId: number): cowel.FileResult {
    const resolvedPath = ((): string => {
        if (baseFileId < 0) {
            return path.resolve(mainDocumentDir, unresolvedPath);
        }
        const parent = path.dirname(files[baseFileId].path);
        return path.resolve(parent, unresolvedPath);
    })();

    let status = cowel.IoStatus.ok;
    let data: Buffer | undefined;
    let id = 0;
    try {
        data = fs.readFileSync(resolvedPath);
        id = files.length;
        files.push({ path: resolvedPath, data });
    } catch (_) {
        status = cowel.IoStatus.error;
    }
    return { status, data, id };
};


function severityColor(severity: cowel.Severity): string {
    const s = severity;
    return s <= cowel.Severity.trace ? AnsiCode.highBlack
        : s <= cowel.Severity.debug ? AnsiCode.white
            : s <= cowel.Severity.info ? AnsiCode.highBlue
                : s <= cowel.Severity.soft_warning ? AnsiCode.highGreen
                    : s <= cowel.Severity.warning ? AnsiCode.highYellow
                        : s <= cowel.Severity.error ? AnsiCode.highRed
                            : s <= cowel.Severity.fatal ? AnsiCode.red
                                : AnsiCode.magenta;
}

function severityTag(severity: cowel.Severity): string {
    switch (severity) {
        case cowel.Severity.min: return "MIN";
        case cowel.Severity.trace: return "TRACE";
        case cowel.Severity.debug: return "DEBUG";
        case cowel.Severity.info: return "INFO";
        case cowel.Severity.soft_warning: return "SOFTWARN";
        case cowel.Severity.warning: return "WARNING";
        case cowel.Severity.error: return "ERROR";
        case cowel.Severity.fatal: return "FATAL";
        case cowel.Severity.none: break;
    }
    return String(Number(severity));
}

function log(diagnostic: cowel.Diagnostic): void {
    const primaryLocation = diagnostic.stack.length !== 0 ? diagnostic.stack[0] : null;
    const file = primaryLocation && primaryLocation.fileId >= 0
        ? files[primaryLocation.fileId]
        : null;

    const tag = severityTag(diagnostic.severity);
    const filePath = primaryLocation === null ? ""
        : primaryLocation.fileName.length !== 0 ? primaryLocation.fileName
            : file ? file.path : mainFile.path;

    const hasLocation = primaryLocation !== null
        && (
            primaryLocation.begin !== 0
            || primaryLocation.length !== 0
            || primaryLocation.line !== 0
        );
    const location = hasLocation && primaryLocation !== null
        ? `:${primaryLocation.line + 1}:${primaryLocation.column + 1}`
        : "";

    const isCitable = primaryLocation !== null
        && primaryLocation.length !== 0
        && primaryLocation.fileName.length === 0;
    const citation = isCitable ? wasm!.generateCodeCitationFor({
        source: file ? file.data : mainFile.data,
        line: primaryLocation.line,
        column: primaryLocation.column,
        begin: primaryLocation.begin,
        length: primaryLocation.length,
        colors: colorsEnabled,
    }) : null;

    const fullLocation = filePath.length !== 0 || location.length !== 0 ?
        ` ${colorsEnabled ? AnsiCode.highBlack : ""}${filePath}${location}:` : "";

    if (colorsEnabled) {
        const tagColor = severityColor(diagnostic.severity);
        process.stdout.write(`${tagColor}${tag}${fullLocation} ${AnsiCode.reset}${diagnostic.message} ${AnsiCode.highBlack}[${diagnostic.id}]${AnsiCode.reset}\n`);
    } else {
        process.stdout.write(`${tag}${fullLocation} ${diagnostic.message} [${diagnostic.id}]\n`);
    }
    if (citation !== null) {
        process.stdout.write(citation);
    }

    for (let i = 1; i < diagnostic.stack.length; ++i) {
        const stackLocation = diagnostic.stack[i];

        const stackFile = stackLocation.fileId < 0 ? mainFile : files[stackLocation.fileId];
        const stackFilePath = stackLocation.fileName.length !== 0 ? stackLocation.fileName
            : stackFile ? stackFile.path : mainFile.path;
        const hasStackPos =
            stackLocation.begin !== 0 || stackLocation.length !== 0 || stackLocation.line !== 0;
        const stackPos = hasStackPos
            ? `:${stackLocation.line + 1}:${stackLocation.column + 1}`
            : "";

        const stackFullLocation = stackFilePath.length !== 0 || stackPos.length !== 0
            ? ` ${colorsEnabled ? AnsiCode.highBlack : ""}${stackFilePath}${stackPos}:`
            : "";

        if (colorsEnabled) {
            const noteLine = `${AnsiCode.highWhite}NOTE${stackFullLocation} ${AnsiCode.reset}Expanded from here.${AnsiCode.reset}\n`;
            process.stdout.write(noteLine);
        } else {
            process.stdout.write(`NOTE${stackFullLocation} Expanded from here.\n`);
        }

        const stackSource = stackLocation.fileName.length === 0
            ? stackFile ? stackFile.data : mainFile.data
            : null;
        const stackCitation = stackSource !== null && stackLocation.length !== 0
            ? wasm!.generateCodeCitationFor({
                source: stackSource,
                line: stackLocation.line,
                column: stackLocation.column,
                begin: stackLocation.begin,
                length: stackLocation.length,
                colors: colorsEnabled,
            })
            : null;
        if (stackCitation !== null) {
            process.stdout.write(stackCitation);
        }
    }
}

function logError(
    fileName: string,
    id: string,
    message: string,
    severity = cowel.Severity.fatal,
): void {
    const stack = fileName.length === 0 ? [] : [{
        fileName,
        fileId: -1,
        begin: 0,
        length: 0,
        line: 0,
        column: 0,
    }];
    log({ severity, id, message, stack });
}

type RunOptions = {
    minSeverity: cowel.Severity;
};

async function run(
    inputPath: string,
    outputPath: string,
    options: RunOptions,
): Promise<number> {
    const mainFileResult = ((): string => {
        try {
            const data = fs.readFileSync(inputPath);
            mainFile = { path: inputPath, data };
            return data.toString("utf8");
        } catch (_) {
            logError(
                inputPath,
                "file.read",
                "Failed to open main document.",
            );
            process.exit(1);
        }
    })();

    mainDocumentDir = path.dirname(inputPath);

    const result = await wasm!.generateHtml({
        source: mainFileResult,
        mode: cowel.Mode.document,
        minSeverity: options.minSeverity,
        highlightPolicy: cowel.SyntaxHighlightPolicy.fall_back,
        enableXHighlighting: false,
        loadFile,
        log,
    });
    if (result.status !== cowel.ProcessingStatus.ok) {
        const statusString = cowel.ProcessingStatus[result.status];
        logError(
            inputPath,
            `status.${statusString}`,
            `Generation exited with status ${result.status} (${statusString}).`,
        );
        return 1;
    }

    try {
        fs.writeFileSync(outputPath, result.output, { encoding: "utf8" });
    } catch (_) {
        logError(
            outputPath,
            "file.write",
            "Failed to write generated output.",
        );
        return 1;
    }

    if (options.minSeverity <= cowel.Severity.debug) {
        const absolutePath = path.resolve(outputPath);
        logError(
            outputPath,
            "file.write",
            `Output written to: ${absolutePath}`,
            cowel.Severity.debug,
        );
    }

    return 0;
}

async function main(): Promise<number> {
    const moduleBytes = readModuleFileSync("cowel.wasm");
    wasm = await cowel.load(moduleBytes);

    const opts = wasm.parseCliOptions(process.argv.slice(2));

    if (!opts.ok) {
        process.stderr.write(opts.errorMessage + "\n");
        return 1;
    }

    switch (opts.command) {
        case "none":
        case "help":
            process.stdout.write(helpText);
            return opts.command === "help" ? 0 : 1;
        case "version":
            console.info(getVersion());
            return 0;
        case "run":
            break;
    }

    colorsEnabled = !opts.noColor;
    return run(opts.input, opts.output, { minSeverity: opts.minSeverity });
}

process.exitCode = await main();
