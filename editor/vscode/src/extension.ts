import * as path from 'node:path';
import * as vscode from 'vscode';
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
    TransportKind,
} from 'vscode-languageclient/node';

import { computeFoldingRanges } from './folding';

let client: LanguageClient | undefined;

export function activate(context: vscode.ExtensionContext): void {
    context.subscriptions.push(
        vscode.languages.registerFoldingRangeProvider(
            { language: 'cowel' },
            {
                provideFoldingRanges(document) {
                    return computeFoldingRanges(document.getText()).map(
                        ([start, end]) => new vscode.FoldingRange(start, end)
                    );
                }
            }
        )
    );

    const serverModule = context.asAbsolutePath(path.join('out', 'lsp-server.js'));
    const serverOptions: ServerOptions = {
        run: {
            module: serverModule,
            transport: TransportKind.ipc,
        },
        debug: {
            module: serverModule,
            transport: TransportKind.ipc,
        },
    };
    const clientOptions: LanguageClientOptions = {
        documentSelector: [{ language: 'cowel' }],
    };

    client = new LanguageClient(
        'cowel-language-server',
        'COWEL Language Server',
        serverOptions,
        clientOptions,
    );
    context.subscriptions.push(client.start());
}

export async function deactivate(): Promise<void> {
    await client?.stop();
}
