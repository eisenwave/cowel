import * as vscode from 'vscode';
import { computeFoldingRanges } from './folding';

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
}

export function deactivate(): void {}
