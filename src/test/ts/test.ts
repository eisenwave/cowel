import { test, describe } from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

import * as cowel from "../../main/ts/cowel-wasm.js";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
// When running from build/test/test/ts/, projectRoot is 4 levels up.
const projectRoot = path.resolve(__dirname, "../../../..");

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

    const testDir = path.join(projectRoot, "test/semantics");
    const testPaths = findFilesRecursively(testDir, /\.cow$/);

    for (const testPath of testPaths) {
        const relativePath = path.relative(testDir, testPath);

        test(relativePath, async () => {
            const files: LoadedFile[] = [];
            const diagnostics: cowel.Diagnostic[] = [];

            const source = fs.readFileSync(testPath, "utf8");
            const mainFile: LoadedFile = { path: testPath, data: fs.readFileSync(testPath) };

            const loadFile = (unresolvedPath: string, baseFileId: number): cowel.FileResult => {
                const resolvedPath = (() => {
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
                    const file = d.fileId < 0 ? mainFile : files[d.fileId];
                    const filePath = d.fileName.length !== 0 ? d.fileName
                        : file ? file.path : mainFile.path;
                    const location = `:${d.line + 1}:${d.column + 1}`;
                    errorMessage += `  [${cowel.Severity[d.severity]}] ${filePath}${location}: ${d.message} [${d.id}]\n`;
                }
                assert.fail(errorMessage);
            }

            const actual = result.variables["__test_input"] ?? "";
            const expected = result.variables["__test_output"] ?? "";
            assert.strictEqual(actual, expected);
        });
    }
});
