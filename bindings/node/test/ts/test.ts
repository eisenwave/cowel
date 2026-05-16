import { test, describe } from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import os from "node:os";
import { spawnSync } from "node:child_process";
import { fileURLToPath, pathToFileURL } from "node:url";

import * as cowel from "../../src/cowel-wasm.js";

const __dirname = path.dirname(fileURLToPath(import.meta.url));

function findProjectRoot(startDir: string): string {
    let dir = startDir;
    for (; ;) {
        const markers = [
            path.join(dir, "CMakeLists.txt"),
            path.join(dir, "engine", "test", "files", "semantics"),
            path.join(dir, "docs", "index.cow"),
        ];
        if (markers.every((marker) => fs.existsSync(marker))) {
            return dir;
        }

        const parent = path.dirname(dir);
        if (parent === dir) {
            throw new Error("Unable to find repository root from test location");
        }
        dir = parent;
    }
}

const projectRoot = findProjectRoot(__dirname);

function readModuleFileSync(file: string): NonSharedBuffer {
    const resolved = path.resolve(projectRoot, file);
    return fs.readFileSync(resolved);
}

function findFilesRecursively(dir: string, pattern: RegExp): string[] {
    const results: string[] = [];
    const entries = fs.readdirSync(dir, { withFileTypes: true });
    for (const entry of entries) {
        const fullPath = path.join(dir, entry.name);
        if (entry.isDirectory()) {
            results.push(...findFilesRecursively(fullPath, pattern));
        } else if (pattern.test(entry.name)) {
            results.push(fullPath);
        }
    }
    return results;
}

type LoadedFile = {
    path: string;
    data: Buffer;
};

function frameJsonRpcBody(body: string): Buffer {
    const bodyBytes = Buffer.from(body, "utf8");
    const header = Buffer.from(`Content-Length: ${bodyBytes.length}\r\n\r\n`, "ascii");
    return Buffer.concat([header, bodyBytes]);
}

function parseFramedJsonRpcMessages(bytes: Buffer): unknown[] {
    const messages: unknown[] = [];
    let index = 0;

    while (index < bytes.length) {
        const headerEnd = bytes.indexOf("\r\n\r\n", index, "ascii");
        assert.notStrictEqual(headerEnd, -1, "Incomplete LSP header in stdout");

        const header = bytes.subarray(index, headerEnd).toString("ascii");
        const match = /Content-Length:\s*(\d+)/i.exec(header);
        if (match === null) {
            assert.fail(`Missing Content-Length header: ${header}`);
        }

        const bodyLength = Number.parseInt(match[1], 10);
        const bodyStart = headerEnd + 4;
        const bodyEnd = bodyStart + bodyLength;
        assert.ok(bodyEnd <= bytes.length, "Incomplete LSP body in stdout");

        const body = bytes.subarray(bodyStart, bodyEnd).toString("utf8");
        messages.push(JSON.parse(body) as unknown);
        index = bodyEnd;
    }

    return messages;
}

function mapFixturePlaceholders(value: unknown, fixtureDir: string): unknown {
    const fixturePath = fixtureDir.replaceAll(path.sep, "/");
    const fixtureUri = pathToFileURL(fixtureDir).href;
    if (typeof value === "string") {
        return value
            .replaceAll("{{ROOT_URI}}", fixtureUri)
            .replaceAll("{{ROOT_PATH}}", fixturePath);
    }
    if (Array.isArray(value)) {
        return value.map((entry) => mapFixturePlaceholders(entry, fixtureDir));
    }
    if (value !== null && typeof value === "object") {
        const result: Record<string, unknown> = {};
        for (const [key, entry] of Object.entries(value)) {
            result[key] = mapFixturePlaceholders(entry, fixtureDir);
        }
        return result;
    }
    return value;
}

describe("Document Generation", async () => {
    const moduleBytes = readModuleFileSync("build/npm/cowel.wasm");
    const wasm = await cowel.load(moduleBytes);

    const testDir = path.join(projectRoot, "engine/test/files/semantics");
    const testPaths = findFilesRecursively(testDir, /\.cow$/);

    for (const testPath of testPaths) {
        const relativePath = path.relative(testDir, testPath);

        test(relativePath, async () => {
            const files: LoadedFile[] = [];
            const diagnostics: cowel.Diagnostic[] = [];

            const source = fs.readFileSync(testPath, "utf8");
            const mainFile: LoadedFile = { path: testPath, data: fs.readFileSync(testPath) };

            const loadFile = (
                unresolvedPath: string,
                baseFileId: number,
            ): cowel.FileResult => {
                const resolvedPath = ((): string => {
                    if (baseFileId < 0) {
                        return path.resolve(path.dirname(testPath), unresolvedPath);
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
                } catch {
                    status = cowel.IoStatus.not_found;
                }
                return { status, data, id };
            };

            const log = (diagnostic: cowel.Diagnostic): void => {
                diagnostics.push(diagnostic);
            };

            const result = await wasm.generateHtml({
                source,
                mode: cowel.Mode.minimal,
                minSeverity: cowel.Severity.warning,
                preservedVariables: ["__test_input", "__test_output"],
                highlightPolicy: cowel.SyntaxHighlightPolicy.fall_back,
                enableXHighlighting: true,
                loadFile,
                log,
            });

            if (diagnostics.length > 0) {
                let errorMessage = `Test produced diagnostics: ${relativePath}\n`;
                for (const d of diagnostics) {
                    const location = d.stack.length !== 0 ? d.stack[0] : null;
                    const file = location && location.fileId >= 0
                        ? files[location.fileId]
                        : mainFile;
                    const filePath = location === null ? "<unknown>"
                        : location.fileName.length !== 0 ? location.fileName
                            : file ? file.path : mainFile.path;
                    const line = location ? location.line + 1 : 1;
                    const column = location ? location.column + 1 : 1;
                    const locationString = `:${line}:${column}`;
                    errorMessage += `  [${cowel.Severity[d.severity]}] ${filePath}${locationString}: ${d.message} [${d.id}]\n`;
                }
                assert.fail(errorMessage);
            }

            const actual = result.variables["__test_input"] ?? "";
            const expected = result.variables["__test_output"] ?? "";
            assert.strictEqual(actual, expected);
        });
    }
});

describe("CLI Output", () => {
    const cliTestDir = path.join(projectRoot, "bindings", "test");
    const lspFixtureDir = path.join(cliTestDir, "lsp");
    const testPaths = findFilesRecursively(cliTestDir, /\.cow$/)
        .filter((testPath) => !testPath.startsWith(`${lspFixtureDir}${path.sep}`));
    const cliPath = path.join(projectRoot, "build", "npm", "cowel.js");

    for (const testPath of testPaths) {
        const relativePath = path.relative(cliTestDir, testPath);

        test(relativePath, () => {
            const outputDir = fs.mkdtempSync(path.join(os.tmpdir(), "cowel-cli-test-"));
            const outputPath = path.join(outputDir, "output.html");

            try {
                const expectedPath = `${testPath}.txt`;
                assert.ok(fs.existsSync(expectedPath), `Missing expected output fixture: ${expectedPath}`);
                const expected = fs.readFileSync(expectedPath, "utf8");

                const result = spawnSync(
                    process.execPath,
                    [cliPath, "run", relativePath, outputPath, "--no-color"],
                    {
                        cwd: cliTestDir,
                        encoding: "utf8",
                    },
                );

                assert.notStrictEqual(
                    result.status,
                    null,
                    `CLI process timed out or failed to start for ${relativePath}`,
                );

                const actual = `${result.stdout}${result.stderr}`.replaceAll("\r\n", "\n");
                assert.strictEqual(actual, expected);
            } finally {
                fs.rmSync(outputDir, { recursive: true, force: true });
            }
        });
    }

    test("help", () => {
        const expectedPath = path.join(cliTestDir, "help.txt");
        assert.ok(fs.existsSync(expectedPath), `Missing expected output fixture: ${expectedPath}`);

        const expected = fs.readFileSync(expectedPath, "utf8");
        const result = spawnSync(process.execPath, [cliPath, "--help"], {
            cwd: cliTestDir,
            encoding: "utf8",
        });

        assert.strictEqual(result.status, 0);

        const actual = `${result.stdout}${result.stderr}`.replaceAll("\r\n", "\n");
        assert.strictEqual(actual, expected);
    });
});

describe("LSP Integration (WASM)", () => {
    const lspTestDir = path.join(projectRoot, "bindings", "test", "lsp");
    const runnerPath = path.join(projectRoot, "build", "lsp-wasm", "lsp-wasm-runner.js");
    const suiteDirs = fs.readdirSync(lspTestDir, { withFileTypes: true })
        .filter((entry) => entry.isDirectory())
        .map((entry) => path.join(lspTestDir, entry.name))
        .sort();

    assert.ok(suiteDirs.length > 0, "No LSP integration test suites discovered");

    for (const suiteDir of suiteDirs) {
        const suiteName = path.relative(lspTestDir, suiteDir);
        const configPath = path.join(suiteDir, ".cowel_config.json");
        const inputPath = path.join(suiteDir, "input.json");
        const outputPath = path.join(suiteDir, "output.json");
        const sourcePaths = findFilesRecursively(suiteDir, /\.cow$/);

        test(suiteName, () => {
            assert.ok(fs.existsSync(configPath), `Missing fixture config: ${configPath}`);
            assert.ok(fs.existsSync(inputPath), `Missing LSP input fixture: ${inputPath}`);
            assert.ok(fs.existsSync(outputPath), `Missing LSP output fixture: ${outputPath}`);
            assert.ok(sourcePaths.length > 0, `Missing .cow source fixtures in: ${suiteDir}`);

            const inputFixture = JSON.parse(fs.readFileSync(inputPath, "utf8")) as unknown;
            const outputFixture = JSON.parse(fs.readFileSync(outputPath, "utf8")) as unknown;

            assert.ok(Array.isArray(inputFixture), `LSP input fixture must be an array: ${inputPath}`);
            assert.ok(Array.isArray(outputFixture), `LSP output fixture must be an array: ${outputPath}`);

            const inputMessages = mapFixturePlaceholders(inputFixture, suiteDir) as unknown[];
            const expectedMessages = mapFixturePlaceholders(outputFixture, suiteDir) as unknown[];

            const stdinBytes = Buffer.concat(inputMessages.map((entry) => {
                if (
                    entry !== null
                    && typeof entry === "object"
                    && "$raw" in entry
                    && typeof (entry as { $raw: unknown }).$raw === "string"
                ) {
                    return frameJsonRpcBody((entry as { $raw: string }).$raw);
                }
                return frameJsonRpcBody(JSON.stringify(entry));
            }));

            const result = spawnSync(process.execPath, [runnerPath], {
                cwd: suiteDir,
                input: stdinBytes,
            });

            assert.notStrictEqual(result.status, null, `LSP runner failed to start: ${suiteName}`);
            assert.strictEqual(
                result.status,
                0,
                `LSP runner exited with ${result.status} for ${suiteName}: ${result.stderr.toString("utf8")}`,
            );

            const actualMessages = parseFramedJsonRpcMessages(result.stdout);
            assert.deepStrictEqual(actualMessages, expectedMessages);
        });
    }
});
