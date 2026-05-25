import { test, expect } from "@playwright/test";
import type { Page } from "@playwright/test";
import fs from "node:fs";
import path from "node:path";
import { spawn, type ChildProcess } from "node:child_process";
import { fileURLToPath } from "node:url";

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

function waitForServingUrl(processHandle: ChildProcess, timeoutMs: number): Promise<string> {
    const stdout = processHandle.stdout;
    const stderr = processHandle.stderr;
    if (stdout === null || stderr === null) {
        throw new Error("Watch process did not provide stdout/stderr pipes");
    }

    return new Promise((resolve, reject) => {
        let output = "";
        const expression = /Serving:\s+(http:\/\/localhost:\d+)/;

        const timer = setTimeout(() => {
            cleanup();
            reject(new Error(`Timed out waiting for watch server URL.\nOutput:\n${output}`));
        }, timeoutMs);

        const onData = (chunk: string | Buffer): void => {
            output += chunk.toString("utf8");
            const match = expression.exec(output);
            if (match !== null) {
                cleanup();
                resolve(match[1]);
            }
        };

        const onExit = (code: number | null, signal: NodeJS.Signals | null): void => {
            cleanup();
            const message = `Watch process exited before reporting URL (code=${code}, signal=${signal}).\nOutput:\n${output}`;
            reject(new Error(message));
        };

        const cleanup = (): void => {
            clearTimeout(timer);
            stdout.off("data", onData);
            stderr.off("data", onData);
            processHandle.off("exit", onExit);
        };

        stdout.on("data", onData);
        stderr.on("data", onData);
        processHandle.once("exit", onExit);
    });
}

function waitForExit(
    processHandle: ChildProcess,
    timeoutMs: number,
): Promise<void> {
    if (processHandle.exitCode !== null || processHandle.signalCode !== null) {
        return Promise.resolve();
    }

    return new Promise((resolve, reject) => {
        const timer = setTimeout(() => {
            reject(new Error("Timed out waiting for watch process to exit"));
        }, timeoutMs);

        processHandle.once("exit", () => {
            clearTimeout(timer);
            resolve();
        });
    });
}

async function closeWatchProcess(processHandle: ChildProcess): Promise<void> {
    if (processHandle.exitCode !== null || processHandle.signalCode !== null) {
        return;
    }

    processHandle.kill("SIGTERM");
    try {
        await waitForExit(processHandle, 2000);
        return;
    } catch {
        processHandle.kill("SIGKILL");
        await waitForExit(processHandle, 2000);
    }
}

async function waitForText(
    page: Page,
    text: string,
    timeoutMs: number,
): Promise<void> {
    await page.getByText(text).waitFor({ timeout: timeoutMs });
}

const DEFAULT_TIMEOUT = 2_500;

test.describe("Watch Mode", () => {
    test("serves a page and live-reloads after source edits", async ({ page }) => {
        test.setTimeout(45_000);

        const buildDir = path.join(projectRoot, "build");
        expect(fs.existsSync(buildDir)).toBeTruthy();

        const tempDir = fs.mkdtempSync(path.join(buildDir, "watch-e2e-"));
        const inputPath = path.join(tempDir, "watch.cow");
        const outputPath = path.join(tempDir, "watch.html");
        const cliPath = path.join(projectRoot, "build", "npm", "cowel.js");

        const initialSource = "WATCH_E2E_INITIAL \\strong{TOKEN_A}\n";
        const updatedSource = "WATCH_E2E_UPDATED \\strong{TOKEN_B}\n";

        fs.writeFileSync(inputPath, initialSource, "utf8");

        const watchProcess = spawn(
            process.execPath,
            [cliPath, "watch", inputPath, outputPath, "--no-color"],
            {
                cwd: projectRoot,
                stdio: ["ignore", "pipe", "pipe"],
            },
        );

        try {
            const url = await waitForServingUrl(watchProcess, DEFAULT_TIMEOUT);
            await page.goto(url, { waitUntil: "domcontentloaded" });

            await waitForText(page, "WATCH_E2E_INITIAL", DEFAULT_TIMEOUT);
            await waitForText(page, "TOKEN_A", DEFAULT_TIMEOUT);

            fs.writeFileSync(inputPath, updatedSource, "utf8");

            await waitForText(page, "WATCH_E2E_UPDATED", DEFAULT_TIMEOUT);
            await waitForText(page, "TOKEN_B", DEFAULT_TIMEOUT);

            expect(fs.existsSync(outputPath)).toBeTruthy();
            const html = fs.readFileSync(outputPath, "utf8");
            expect(html).toContain("WATCH_E2E_UPDATED");
        } finally {
            await closeWatchProcess(watchProcess);
            fs.rmSync(tempDir, { recursive: true, force: true });
        }
    });

    test("issue #382: updates to included files do not trigger live reload", async ({ page }) => {
        test.setTimeout(45_000);
        test.fail(true, "Known bug tracked in #382");

        const buildDir = path.join(projectRoot, "build");
        expect(fs.existsSync(buildDir)).toBeTruthy();

        const tempDir = fs.mkdtempSync(path.join(buildDir, "watch-issue-382-"));
        const inputPath = path.join(tempDir, "main.cow");
        const includedPath = path.join(tempDir, "snippet.cow");
        const outputPath = path.join(tempDir, "watch.html");
        const cliPath = path.join(projectRoot, "build", "npm", "cowel.js");

        const initialIncluded = "ISSUE_382_INITIAL\\n";
        const updatedIncluded = "ISSUE_382_UPDATED\\n";
        const mainSource = "\\cowel_include(path=\"snippet.cow\")\\n";

        fs.writeFileSync(inputPath, mainSource, "utf8");
        fs.writeFileSync(includedPath, initialIncluded, "utf8");

        const watchProcess = spawn(
            process.execPath,
            [cliPath, "watch", inputPath, outputPath, "--no-color"],
            {
                cwd: projectRoot,
                stdio: ["ignore", "pipe", "pipe"],
            },
        );

        try {
            const url = await waitForServingUrl(watchProcess, DEFAULT_TIMEOUT);
            await page.goto(url, { waitUntil: "domcontentloaded" });
            await waitForText(page, "ISSUE_382_INITIAL", DEFAULT_TIMEOUT);

            fs.writeFileSync(includedPath, updatedIncluded, "utf8");

            // Known bug #382: watch listens only to the entry document,
            // so edits to included files do not trigger browser reload.
            await waitForText(page, "ISSUE_382_UPDATED", DEFAULT_TIMEOUT);
        } finally {
            await closeWatchProcess(watchProcess);
            fs.rmSync(tempDir, { recursive: true, force: true });
        }
    });

    test("issue #385: heading counters are not reset between watch rebuilds", async ({ page }) => {
        test.setTimeout(45_000);
        test.fail(true, "Known bug tracked in #385");

        const buildDir = path.join(projectRoot, "build");
        expect(fs.existsSync(buildDir)).toBeTruthy();

        const tempDir = fs.mkdtempSync(path.join(buildDir, "watch-issue-385-"));
        const inputPath = path.join(tempDir, "watch.cow");
        const outputPath = path.join(tempDir, "watch.html");
        const cliPath = path.join(projectRoot, "build", "npm", "cowel.js");

        const makeSource = (tick: number): string => [
            "\\h1{Issue 385 Heading}",
            "\\h2{Subheading}",
            `Tick ${tick}`,
            "",
        ].join("\n");

        fs.writeFileSync(inputPath, makeSource(0), "utf8");

        const watchProcess = spawn(
            process.execPath,
            [cliPath, "watch", inputPath, outputPath, "--no-color"],
            {
                cwd: projectRoot,
                stdio: ["ignore", "pipe", "pipe"],
            },
        );

        try {
            const url = await waitForServingUrl(watchProcess, DEFAULT_TIMEOUT);
            await page.goto(url, { waitUntil: "domcontentloaded" });
            await waitForText(page, "Tick 0", DEFAULT_TIMEOUT);
            await waitForText(page, "1. Subheading", DEFAULT_TIMEOUT);

            fs.writeFileSync(inputPath, makeSource(1), "utf8");
            await waitForText(page, "Tick 1", DEFAULT_TIMEOUT);

            // Known bug #385: heading counters persist between rebuilds,
            // so this heading can become "2. Subheading", "3. ...", etc.
            await waitForText(page, "1. Subheading", DEFAULT_TIMEOUT);

            fs.writeFileSync(inputPath, makeSource(2), "utf8");
            await waitForText(page, "Tick 2", DEFAULT_TIMEOUT);
            await waitForText(page, "1. Subheading", DEFAULT_TIMEOUT);
        } finally {
            await closeWatchProcess(watchProcess);
            fs.rmSync(tempDir, { recursive: true, force: true });
        }
    });

    test("issue #386: back/forward can crash watch process with EPIPE", async ({ page }) => {
        test.setTimeout(45_000);
        test.fail(true, "Known bug tracked in #386");

        const buildDir = path.join(projectRoot, "build");
        expect(fs.existsSync(buildDir)).toBeTruthy();

        const tempDir = fs.mkdtempSync(path.join(buildDir, "watch-issue-386-"));
        const inputPath = path.join(tempDir, "watch.cow");
        const outputPath = path.join(tempDir, "watch.html");
        const cliPath = path.join(projectRoot, "build", "npm", "cowel.js");

        const initialSource = "ISSUE_386_INITIAL\\n";
        const updatedSource = "ISSUE_386_UPDATED\\n";

        fs.writeFileSync(inputPath, initialSource, "utf8");

        const watchProcess = spawn(
            process.execPath,
            [cliPath, "watch", inputPath, outputPath, "--no-color"],
            {
                cwd: projectRoot,
                stdio: ["ignore", "pipe", "pipe"],
            },
        );

        try {
            const url = await waitForServingUrl(watchProcess, DEFAULT_TIMEOUT);
            await page.goto(url, { waitUntil: "domcontentloaded" });
            await waitForText(page, "ISSUE_386_INITIAL", DEFAULT_TIMEOUT);

            await page.goto(`${url}/navigated`, { waitUntil: "domcontentloaded" });
            await page.goBack({ waitUntil: "domcontentloaded" });
            await waitForText(page, "ISSUE_386_INITIAL", DEFAULT_TIMEOUT);

            fs.writeFileSync(inputPath, updatedSource, "utf8");

            await waitForText(page, "ISSUE_386_UPDATED", DEFAULT_TIMEOUT);
            // Known bug #386: after back/forward navigation,
            // websocket writes can hit EPIPE and terminate the watch process.
            await expect
                .poll(() => watchProcess.exitCode, { timeout: DEFAULT_TIMEOUT })
                .toBeNull();
        } finally {
            await closeWatchProcess(watchProcess);
            fs.rmSync(tempDir, { recursive: true, force: true });
        }
    });
});
