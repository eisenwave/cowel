import * as path from "node:path";
import * as vscode from "vscode";
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
    State,
    TransportKind,
} from "vscode-languageclient/node";

import { computeFoldingRanges } from "./folding";

let client: LanguageClient | undefined;

type CowelStatusKind = "busy" | "idle" | "error";

const LANGUAGE_CLIENT_ID = "cowel";
const LANGUAGE_CLIENT_NAME = "COWEL Language Server";
const LANGUAGE_NAME = "cowel";
const STATUS_INDICATOR_ID = `${LANGUAGE_NAME}.lsp.status`;
const STATUS_INDICATOR_NAME = "COWEL LSP";

export function activate(context: vscode.ExtensionContext): void {
    const serverOutputChannel = vscode.window.createOutputChannel(LANGUAGE_CLIENT_NAME);
    context.subscriptions.push(serverOutputChannel);

    const lspStatus = vscode.languages.createLanguageStatusItem(
        STATUS_INDICATOR_ID,
        { language: LANGUAGE_NAME },
    );
    lspStatus.name = STATUS_INDICATOR_NAME;
    setStatus(lspStatus, "busy", "Starting language server...");
    context.subscriptions.push(lspStatus);

    context.subscriptions.push(vscode.commands.registerCommand(
        `${LANGUAGE_NAME}.restartLanguageServer`,
        async () => {
            await restartLanguageServer(lspStatus);
        },
    ));

    context.subscriptions.push(vscode.languages.registerFoldingRangeProvider(
        { language: LANGUAGE_NAME },
        {
            provideFoldingRanges(document) {
                return computeFoldingRanges(document.getText()).map(([start, end]) =>
                    new vscode.FoldingRange(start, end));
            },
        },
    ));

    const serverScript = context.asAbsolutePath(path.join("out", "lsp", "lsp-wasm-runner.js"));
    const serverOptions: ServerOptions = {
        run: {
            command: process.execPath,
            args: [serverScript],
            transport: TransportKind.stdio,
        },
        debug: {
            command: process.execPath,
            args: [serverScript],
            transport: TransportKind.stdio,
        },
    };
    const clientOptions: LanguageClientOptions = {
        documentSelector: [{ language: LANGUAGE_NAME }],
        outputChannel: serverOutputChannel,
    };

    client = new LanguageClient(
        LANGUAGE_CLIENT_ID,
        LANGUAGE_CLIENT_NAME,
        serverOptions,
        clientOptions,
    );
    context.subscriptions.push(client.onDidChangeState((event) => {
        if (event.newState === State.Running) {
            setStatus(lspStatus, "idle", "Language server ready");
            return;
        }
        if (event.newState === State.Starting) {
            setStatus(lspStatus, "busy", "Starting language server...");
            return;
        }
        setStatus(lspStatus, "error", "Language server stopped");
    }));
    void client.start();
}

export async function deactivate(): Promise<void> {
    await client?.stop();
}

function setStatus(
    statusItem: vscode.LanguageStatusItem,
    kind: CowelStatusKind,
    detail: string,
): void {
    switch (kind) {
        case "busy": {
            statusItem.busy = true;
            statusItem.severity = vscode.LanguageStatusSeverity.Information;
            statusItem.text = "$(sync~spin) COWEL";
            break;
        }
        case "idle": {
            statusItem.busy = false;
            statusItem.severity = vscode.LanguageStatusSeverity.Information;
            statusItem.text = "$(check) COWEL";
            break;
        }
        case "error": {
            statusItem.busy = false;
            statusItem.severity = vscode.LanguageStatusSeverity.Error;
            statusItem.text = "$(error) COWEL";
            break;
        }
    }
    statusItem.detail = detail;
}

/**
 * Restarts the language server and updates the status item accordingly.
 * @param lspStatus The status item to be modified.
 */
async function restartLanguageServer(lspStatus: vscode.LanguageStatusItem): Promise<void> {
    if (client === undefined) {
        setStatus(lspStatus, "error", "Language server not initialized");
        return;
    }

    try {
        setStatus(lspStatus, "busy", "Restarting language server...");
        await client.stop();
        await client.start();
    } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        setStatus(lspStatus, "error", `Failed to restart: ${message}`);
    }
}
