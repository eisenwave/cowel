import { test, describe } from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import os from "node:os";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

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
    const testPaths = findFilesRecursively(cliTestDir, /\.cow$/);
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
