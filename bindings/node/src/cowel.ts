#!/usr/bin/env node
import path from "path";
import fs from "fs";
import http from "http";
import crypto from "crypto";
import process from "process";
import { fileURLToPath } from "url";
import type { AddressInfo } from "net";
import type { Socket } from "net";

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
Usage: cowel <command> <input> <output> [options]

Commands:
  run   <input> <output>  Processes a COWEL document
  watch <input> <output>  Processes a COWEL document and serves it with live reload

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

async function compile(
    inputPath: string,
    options: RunOptions,
): Promise<string | null> {
    files.length = 0;
    mainDocumentDir = path.dirname(inputPath);

    let source: string;
    try {
        const data = fs.readFileSync(inputPath);
        mainFile = { path: inputPath, data };
        source = data.toString("utf8");
    } catch (_) {
        logError(inputPath, "file.read", "Failed to open main document.");
        return null;
    }

    const result = await wasm!.generateHtml({
        source,
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
        return null;
    }

    return result.output;
}

async function run(
    inputPath: string,
    outputPath: string,
    options: RunOptions,
): Promise<number> {
    const html = await compile(inputPath, options);
    if (html === null) return 1;

    try {
        fs.writeFileSync(outputPath, html, { encoding: "utf8" });
    } catch (_) {
        logError(outputPath, "file.write", "Failed to write generated output.");
        return 1;
    }

    if (options.minSeverity <= cowel.Severity.debug) {
        const absolutePath = path.resolve(outputPath);
        logError(outputPath, "file.write", `Output written to: ${absolutePath}`, cowel.Severity.debug);
    }

    return 0;
}

// Magic GUID required for the WebSocket handshake accept key.
// See: https://datatracker.ietf.org/doc/html/rfc6455#section-1.3
const WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

function upgradeToWs(req: http.IncomingMessage, socket: Socket): void {
    const key = req.headers["sec-websocket-key"] as string;
    const accept = crypto.createHash("sha1").update(key + WS_GUID).digest("base64");
    socket.write("HTTP/1.1 101 Switching Protocols\r\n" +
        "Upgrade: websocket\r\n" +
        "Connection: Upgrade\r\n" +
        `Sec-WebSocket-Accept: ${accept}\r\n\r\n`);
}

function sendWsFrame(socket: Socket, message: string): void {
    const payload = Buffer.from(message, "utf8");
    const frame = Buffer.allocUnsafe(2 + payload.length);
    frame[0] = 0x81; // FIN + text opcode
    frame[1] = payload.length;
    payload.copy(frame, 2);
    socket.write(frame);
}

function injectLiveReloadScript(html: string, port: number): string {
    const script = `<script>(function(){if(window.__cowelWs)window.__cowelWs.close();var ws=window.__cowelWs=new WebSocket("ws://localhost:${port}");ws.onmessage=function(){var x=scrollX,y=scrollY;fetch("/?__cowel_live").then(function(r){return r.text()}).then(function(h){var d=new DOMParser().parseFromString(h,"text/html");document.head.innerHTML=d.head.innerHTML;document.body.innerHTML=d.body.innerHTML;scrollTo(x,y)});};ws.onclose=function(){setTimeout(function(){location.reload();},2000);}}());</script>`;
    // Insert before the last </body> so it runs after page content is parsed.
    const idx = html.lastIndexOf("</body>");
    if (idx === -1) return html + script;
    return html.slice(0, idx) + script + html.slice(idx);
}

async function watchAndServe(
    inputPath: string,
    outputPath: string,
    options: RunOptions,
): Promise<number> {
    let currentHtml = await compile(inputPath, options);
    if (currentHtml === null) return 1;

    try {
        fs.writeFileSync(outputPath, currentHtml, { encoding: "utf8" });
    } catch (_) {
        logError(outputPath, "file.write", "Failed to write generated output.");
    }

    const wsClients: Socket[] = [];

    const server = http.createServer((req, res) => {
        const isLiveFetch = req.url?.includes("__cowel_live") ?? false;
        const body = isLiveFetch ? currentHtml! : injectLiveReloadScript(currentHtml!, port);
        res.writeHead(200, { "Content-Type": "text/html; charset=utf-8" });
        res.end(body);
    });

    server.on("upgrade", (req, socket) => {
        upgradeToWs(req, socket as Socket);
        wsClients.push(socket as Socket);
        socket.on("close", () => {
            const i = wsClients.indexOf(socket as Socket);
            if (i !== -1) wsClients.splice(i, 1);
        });
        // Prevent EPIPE from crashing when the browser closes the connection.
        socket.on("error", () => {
            const i = wsClients.indexOf(socket as Socket);
            if (i !== -1) wsClients.splice(i, 1);
        });
    });

    let port = 0;

    await new Promise<void>((resolve) => server.listen(0, "localhost", resolve));
    port = (server.address() as AddressInfo).port;

    const absInput = path.resolve(inputPath);
    process.stdout.write(`Watching: ${absInput}\n`);
    process.stdout.write(`Serving:  http://localhost:${port}\n`);

    let debounce: ReturnType<typeof setTimeout> | undefined;
    let watchers: fs.FSWatcher[] = [];

    function rewatch(): void {
        for (const w of watchers) w.close();
        const paths = [inputPath, ...files.map(f => f.path)];
        watchers = paths.map(p => fs.watch(p, onChange));
    }

    function onChange(): void {
        clearTimeout(debounce);
        debounce = setTimeout(async () => {
            const html = await compile(inputPath, options);
            if (html !== null) {
                currentHtml = html;
                try {
                    fs.writeFileSync(outputPath, html, { encoding: "utf8" });
                } catch (_) {
                    logError(outputPath, "file.write", "Failed to write generated output.");
                }
                // Included files may have changed.
                // Watchers need to be re-established here.
                rewatch();
            }
            // Notify all connected browsers; remove dead sockets.
            for (let i = wsClients.length - 1; i >= 0; --i) {
                try {
                    sendWsFrame(wsClients[i], "update");
                } catch (_) {
                    wsClients.splice(i, 1);
                }
            }
        }, 50);
    }
    // Keep the process alive indefinitely; users can Ctrl+C to exits.
    rewatch();
    return new Promise<number>(() => { /* intentionally never resolves */ });
}

async function main(): Promise<number> {
    const moduleBytes = readModuleFileSync("cowel.wasm");
    wasm = await cowel.load(moduleBytes);

    // Intercept "watch" before the WASM CLI parser sees it;
    // reuse "run" parsing.
    const isWatch = process.argv[2] === "watch";
    if (isWatch) process.argv[2] = "run";

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
    const runOpts: RunOptions = { minSeverity: opts.minSeverity };
    if (isWatch) return watchAndServe(opts.input, opts.output, runOpts);
    return run(opts.input, opts.output, runOpts);
}

process.exitCode = await main();
