import * as fs from 'fs';
import * as path from 'path';
import * as vsctm from 'vscode-textmate';
import * as oniguruma from 'vscode-oniguruma';

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
    }
  };
}

async function dumpScopes(filename: string): Promise<void> {
  const onigLib = await loadOniguruma();
  const registry = new vsctm.Registry({
    onigLib: Promise.resolve(onigLib),
    loadGrammar: async (scopeName: string) => {
      if (scopeName === 'source.cowel') {
        const grammarPath = path.join(__dirname, '../syntaxes/cowel.tmLanguage.json');
        const content = fs.readFileSync(grammarPath, 'utf8');
        return vsctm.parseRawGrammar(content, grammarPath);
      }
      return null;
    }
  });

  const grammar = await registry.loadGrammar('source.cowel');
  if (!grammar) {
    throw new Error('Failed to load grammar');
  }

  const sourceCode = fs.readFileSync(path.join(__dirname, filename), 'utf8');
  const lines = sourceCode.split('\n');
  let ruleStack = vsctm.INITIAL;

  for (const line of lines) {
    const result = grammar.tokenizeLine(line, ruleStack);
    console.log(line);

    for (const token of result.tokens) {
      const text = line.substring(token.startIndex, token.endIndex);
      if (text.trim() === '') continue;

      const marker = ' '.repeat(token.startIndex) + '^'.repeat(token.endIndex - token.startIndex);
      console.log(`  ${marker} ${token.scopes.join(' ')}`);
    }

    console.log();
    ruleStack = result.ruleStack;
  }
}

const filename = process.argv[2] || 'comments.cowel';
dumpScopes(filename).catch(console.error);
