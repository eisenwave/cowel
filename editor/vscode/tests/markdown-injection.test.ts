import * as fs from 'fs';
import * as path from 'path';
import * as vsctm from 'vscode-textmate';
import * as oniguruma from 'vscode-oniguruma';

const colors = {
    reset: '\x1b[0m',
    bright: '\x1b[1m',
    red: '\x1b[91m',
    green: '\x1b[92m',
} as const;

interface TokenInfo {
    startIndex: number;
    endIndex: number;
    scopes: string[];
}

async function loadOniguruma(): Promise<vsctm.IOnigLib> {
    const wasmPath = path.join(__dirname, '../node_modules/vscode-oniguruma/release/onig.wasm');
    const wasmBin = fs.readFileSync(wasmPath).buffer;
    await oniguruma.loadWASM(wasmBin);

    return {
        createOnigScanner(patterns: string[]) {
            return new oniguruma.OnigScanner(patterns);
        },
        createOnigString(s: string) {
            return new oniguruma.OnigString(s);
        },
    };
}

async function createRegistry(): Promise<vsctm.Registry> {
    const onigLib = await loadOniguruma();

    return new vsctm.Registry({
        onigLib: Promise.resolve(onigLib),
        loadGrammar: async (scopeName: string) => {
            if (scopeName === 'source.cowel') {
                const grammarPath = path.join(__dirname, '../syntaxes/cowel.tmLanguage.json');
                const content = fs.readFileSync(grammarPath, 'utf8');
                return vsctm.parseRawGrammar(content, grammarPath);
            }
            if (scopeName === 'markdown.cowel.codeblock') {
                const grammarPath = path.join(__dirname, '../syntaxes/markdown-cowel-codeblock.tmLanguage.json');
                const content = fs.readFileSync(grammarPath, 'utf8');
                return vsctm.parseRawGrammar(content, grammarPath);
            }
            return null;
        },
    });
}

function tokenizeSource(grammar: vsctm.IGrammar, sourceCode: string): TokenInfo[][] {
    const lines = sourceCode.split('\n');
    const result: TokenInfo[][] = [];
    let ruleStack = vsctm.INITIAL;

    for (const line of lines) {
        const lineResult = grammar.tokenizeLine(line, ruleStack);
        const tokens: TokenInfo[] = [];
        for (const token of lineResult.tokens) {
            tokens.push({
                startIndex: token.startIndex,
                endIndex: token.endIndex,
                scopes: token.scopes,
            });
        }
        result.push(tokens);
        ruleStack = lineResult.ruleStack;
    }

    return result;
}

function findTokenAt(tokenizedLines: TokenInfo[][], line: number, col: number): TokenInfo | null {
    if (line < 0 || line >= tokenizedLines.length) {
        return null;
    }
    for (const token of tokenizedLines[line]) {
        if (col >= token.startIndex && col < token.endIndex) {
            return token;
        }
    }
    return null;
}

function assert(condition: boolean, message: string): void {
    if (!condition) {
        throw new Error(message);
    }
}

async function run(): Promise<void> {
    const registry = await createRegistry();
    const grammar = await registry.loadGrammar('markdown.cowel.codeblock');
    if (!grammar) {
        throw new Error('Failed to load markdown COWEL injection grammar');
    }

    const markdown = [
        '# Example',
        '',
        '```cowel',
        '\\h1{Hello \\em{world}}',
        '```',
        '',
        '```js',
        'const x = 1;',
        '```',
    ].join('\n');

    const tokenized = tokenizeSource(grammar, markdown);

    const directiveToken = findTokenAt(tokenized, 3, 1);
    assert(directiveToken !== null, 'Expected token in COWEL fenced block');
    assert(
        directiveToken!.scopes.includes('support.function.directive.cowel'),
        `Expected COWEL directive scope in fenced block, got ${JSON.stringify(directiveToken!.scopes)}`
    );
    assert(
        directiveToken!.scopes.includes('meta.embedded.block.cowel'),
        `Expected embedded COWEL block scope, got ${JSON.stringify(directiveToken!.scopes)}`
    );

    const jsToken = findTokenAt(tokenized, 7, 0);
    assert(jsToken !== null, 'Expected token in JS fenced block');
    assert(
        !jsToken!.scopes.includes('support.function.directive.cowel'),
        `Did not expect COWEL directive scope in non-COWEL fence, got ${JSON.stringify(jsToken!.scopes)}`
    );

    console.log(`${colors.green}${colors.bright}OK:${colors.reset} markdown COWEL fenced block embeds source.cowel`);
}

run().catch((error) => {
    console.log(`${colors.red}${colors.bright}FAIL:${colors.reset} markdown COWEL fenced block injection`);
    console.error(error.message);
    process.exit(1);
});
