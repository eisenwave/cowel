#!/usr/bin/env node
import path from "path";
import fs from "fs";
import process from "process";
import yargs from "yargs";
import { fileURLToPath } from "url";
import * as helpers from "yargs/helpers";

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

const severityKeys = [
    "min",
    "trace",
    "debug",
    "info",
    "soft_warning",
    "warning",
    "error",
    "fatal",
    "none",
];

function getEnumValue<T extends object>(
    enumObj: T,
    key: string,
): number | undefined {
    if (key in enumObj && typeof enumObj[key as keyof T] === "number") {
        return enumObj[key as keyof T] as unknown as number;
    }
    return undefined;
}

const parser = yargs(helpers.hideBin(process.argv))
    .scriptName("cowel")
    .usage("Usage: $0 <command>")
    .command(
        "run <input> <output>",
        "Processes a COWEL document",
        yargs => {
            return yargs
                .positional("input", {
                    type: "string",
                    description: "Input file",
                })
                .positional("output", {
                    type: "string",
                    description: "Output file",
                })
                .option("severity", {
                    alias: "l",
                    type: "string",
                    choices: severityKeys,
                    default: cowel.Severity[cowel.Severity.info],
                    description: "Minimum (>=) severity for log messages",
                });
        },
    )
    .option("no-color", {
        type: "boolean",
        default: false,
        description: "Disable colored output",
    })
    .version(false)
    .option("version", {
        alias: "v",
        type: "boolean",
        description: "Show version",
    })
    .strict()
    .help("h")
    .alias("h", "help")
    .wrap(Math.min(process.stdout.columns || Infinity, 100));

// eslint-disable-next-line @typescript-eslint/explicit-function-return-type
function parseArgs() {
    return parser.parseSync();
}

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
    const file = diagnostic.fileId < 0 ? null : files[diagnostic.fileId];

    const tag = severityTag(diagnostic.severity);
    const filePath = diagnostic.fileName.length !== 0 ? diagnostic.fileName
        : file ? file.path : mainFile.path;

    const hasLocation =
        diagnostic.begin !== 0 || diagnostic.length !== 0 || diagnostic.line !== 0;
    const location = hasLocation ? `:${diagnostic.line + 1}:${diagnostic.column + 1}` : "";

    const isCitable = diagnostic.length !== 0 && diagnostic.fileName.length === 0;
    const citation = isCitable ? wasm!.generateCodeCitationFor({
        source: file ? file.data : mainFile.data,
        line: diagnostic.line,
        column: diagnostic.column,
        begin: diagnostic.begin,
        length: diagnostic.length,
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
}

function logError(
    fileName: string,
    id: string,
    message: string,
    severity = cowel.Severity.fatal,
): void {
    log({
        severity,
        id,
        message,
        fileName,
        fileId: -1,
        begin: 0,
        length: 0,
        line: 0,
        column: 0,
    });
}

type RunOptions = {
    minSeverity: cowel.Severity;
};

async function run(
    inputPath: string,
    outputPath: string,
    options: RunOptions,
): Promise<number> {
    const moduleBytes = readModuleFileSync("cowel.wasm");

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

    wasm = await cowel.load(moduleBytes);

    const result = await wasm.generateHtml({
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
    const args = parseArgs();

    if (args.version) {
        console.info(getVersion());
        return 0;
    }
    if (args._.length === 0) {
        parser.showHelp();
        return 1;
    }
    if (args["no-color"]) {
        colorsEnabled = false;
    }

    const inputPath = String(args.input);
    const outputPath = String(args.output);

    const minSeverity = getEnumValue(cowel.Severity, args.severity);
    if (minSeverity === undefined) {
        logError(
            "",
            "option.severity",
            `"${args.severity}" is not a valid severity; see --help.`,
        );
        return 1;
    }

    return run(inputPath, outputPath, { minSeverity });
}

process.exitCode = await main();
