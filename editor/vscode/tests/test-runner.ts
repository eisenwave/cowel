#!/usr/bin/env ts-node

import * as fs from 'fs';
import * as path from 'path';
import * as vsctm from 'vscode-textmate';
import * as oniguruma from 'vscode-oniguruma';

const colors = {
  reset: '\x1b[0m',
  bright: '\x1b[1m',
  dim: '\x1b[2m',
  red: '\x1b[91m',
  green: '\x1b[92m',
  yellow: '\x1b[93m',
  cyan: '\x1b[96m',
} as const;

interface TokenInfo {
  startIndex: number;
  endIndex: number;
  scopes: string[];
  text: string;
}

interface TokenizedLine {
  text: string;
  tokens: TokenInfo[];
}

type AssertionType = 'scope-at' | 'top-scope' | 'token-has-scope' | 'scope-chain' | 'parent-scope';

interface BaseAssertion {
  type: AssertionType;
  line: number;
  col: number;
  description?: string;
}

interface ScopeAtAssertion extends BaseAssertion {
  type: 'scope-at';
  expectedScopes: string[];
}

interface TopScopeAssertion extends BaseAssertion {
  type: 'top-scope';
  expected: string;
}

interface TokenHasScopeAssertion extends BaseAssertion {
  type: 'token-has-scope';
  scope: string;
}

interface ScopeChainAssertion extends BaseAssertion {
  type: 'scope-chain';
  expected: string[];
}

interface ParentScopeAssertion extends BaseAssertion {
  type: 'parent-scope';
  expected: string;
}

type Assertion =
  | ScopeAtAssertion
  | TopScopeAssertion
  | TokenHasScopeAssertion
  | ScopeChainAssertion
  | ParentScopeAssertion;

interface TestExpectations {
  description?: string;
  assertions: Assertion[];
}

interface AssertionResult {
  passed: boolean;
  assertion: Assertion;
  message?: string;
  actual?: string[];
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
    }
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
      return null;
    }
  });
}

function tokenizeSource(grammar: vsctm.IGrammar, sourceCode: string): TokenizedLine[] {
  const lines = sourceCode.split('\n');
  const result: TokenizedLine[] = [];
  let ruleStack = vsctm.INITIAL;

  for (const line of lines) {
    const lineResult = grammar.tokenizeLine(line, ruleStack);
    const tokens: TokenInfo[] = [];

    for (const token of lineResult.tokens) {
      tokens.push({
        startIndex: token.startIndex,
        endIndex: token.endIndex,
        scopes: token.scopes,
        text: line.substring(token.startIndex, token.endIndex)
      });
    }

    result.push({
      text: line,
      tokens: tokens
    });
    ruleStack = lineResult.ruleStack;
  }

  return result;
}

function findTokenAt(tokenizedLines: TokenizedLine[], line: number, col: number): TokenInfo | null {
  if (line < 0 || line >= tokenizedLines.length) {
    return null;
  }

  const lineTokens = tokenizedLines[line].tokens;
  for (const token of lineTokens) {
    if (col >= token.startIndex && col < token.endIndex) {
      return token;
    }
  }

  return null;
}

function scopesMatch(actual: string[], expected: string[]): boolean {
  if (actual.length !== expected.length) {
    return false;
  }
  return actual.every((scope, index) => scope === expected[index]);
}

function validateAssertion(assertion: Assertion, tokenizedLines: TokenizedLine[]): AssertionResult {
  const token = findTokenAt(tokenizedLines, assertion.line, assertion.col);

  if (!token) {
    return {
      passed: false,
      assertion,
      message: `No token found at line ${assertion.line}, col ${assertion.col}`
    };
  }

  switch (assertion.type) {
    case 'scope-at': {
      const matches = scopesMatch(token.scopes, assertion.expectedScopes);
      return {
        passed: matches,
        assertion,
        actual: token.scopes,
        message: matches
          ? undefined
          : `Expected scopes ${JSON.stringify(assertion.expectedScopes)}, got ${JSON.stringify(token.scopes)}`
      };
    }

    case 'top-scope': {
      const topScope = token.scopes[token.scopes.length - 1];
      const matches = topScope === assertion.expected;
      return {
        passed: matches,
        assertion,
        actual: token.scopes,
        message: matches
          ? undefined
          : `Expected top scope "${assertion.expected}", got "${topScope}"`
      };
    }

    case 'token-has-scope': {
      const hasScope = token.scopes.includes(assertion.scope);
      return {
        passed: hasScope,
        assertion,
        actual: token.scopes,
        message: hasScope
          ? undefined
          : `Token does not have scope "${assertion.scope}". Actual scopes: ${JSON.stringify(token.scopes)}`
      };
    }

    case 'scope-chain': {
      const matches = scopesMatch(token.scopes, assertion.expected);
      return {
        passed: matches,
        assertion,
        actual: token.scopes,
        message: matches
          ? undefined
          : `Expected scope chain ${JSON.stringify(assertion.expected)}, got ${JSON.stringify(token.scopes)}`
      };
    }

    case 'parent-scope': {
      const parentScope = token.scopes.length >= 2 ? token.scopes[token.scopes.length - 2] : undefined;
      const matches = parentScope === assertion.expected;
      return {
        passed: matches,
        assertion,
        actual: token.scopes,
        message: matches
          ? undefined
          : `Expected parent scope "${assertion.expected}", got "${parentScope || 'none'}"`
      };
    }

    default:
      return {
        passed: false,
        assertion,
        message: `Unknown assertion type: ${(assertion as any).type}`
      };
  }
}

function runTest(
  grammar: vsctm.IGrammar,
  sourceCode: string,
  expectations: TestExpectations
): { passed: boolean; results: AssertionResult[] } {
  const tokenized = tokenizeSource(grammar, sourceCode);
  const results: AssertionResult[] = [];

  for (const assertion of expectations.assertions) {
    const result = validateAssertion(assertion, tokenized);
    results.push(result);
  }

  const allPassed = results.every(r => r.passed);
  return { passed: allPassed, results };
}

function printAssertionFailures(testFile: string, sourceLines: string[], results: AssertionResult[]): void {
  const failures = results.filter(r => !r.passed);

  for (const failure of failures) {
    const assertion = failure.assertion;
    const sourceLine = sourceLines[assertion.line] || '';
    const marker = ' '.repeat(assertion.col) + '^';

    console.log(`  ${colors.red}${colors.bright}Assertion failed:${colors.reset} ${assertion.description || assertion.type}`);
    console.log(`    at line ${assertion.line + 1}, column ${assertion.col + 1}`);
    console.log(`    ${colors.dim}${sourceLine}${colors.reset}`);
    console.log(`    ${colors.cyan}${marker}${colors.reset}`);
    console.log(`    ${colors.red}${failure.message}${colors.reset}`);
    if (failure.actual) {
      console.log(`    ${colors.dim}Actual scope chain: ${failure.actual.join(' → ')}${colors.reset}`);
    }
    console.log();
  }
}

async function runTests(): Promise<void> {
  const registry = await createRegistry();
  const grammar = await registry.loadGrammar('source.cowel');

  if (!grammar) {
    console.error(`${colors.red}${colors.bright}ERROR:${colors.reset} Failed to load COWEL grammar`);
    process.exit(1);
  }

  const fixturesDir = path.join(__dirname, 'fixtures');
  if (!fs.existsSync(fixturesDir)) {
    console.error(`${colors.red}${colors.bright}ERROR:${colors.reset} Fixtures directory not found: ${fixturesDir}`);
    process.exit(1);
  }

  const testFiles = fs.readdirSync(fixturesDir)
    .filter(file => file.endsWith('.cowel'))
    .sort();

  let passCount = 0;
  let failCount = 0;
  let noExpectationsCount = 0;
  for (const testFile of testFiles) {
    const testPath = path.join(fixturesDir, testFile);
    const jsonPath = testPath + '.json';

    const hasJsonExpectations = fs.existsSync(jsonPath);
    const sourceCode = fs.readFileSync(testPath, 'utf8');
    const sourceLines = sourceCode.split('\n');
    if (!hasJsonExpectations) {
      console.log(`${colors.green}${colors.bright}OK:${colors.reset} ${testFile}     ${colors.yellow}[NOTE: no expectations]${colors.reset}`);
      noExpectationsCount++;
      passCount++;
      continue;
    }

    const expectations: TestExpectations = JSON.parse(fs.readFileSync(jsonPath, 'utf8'));
    const { passed, results } = runTest(grammar, sourceCode, expectations);

    if (passed) {
      console.log(`${colors.green}${colors.bright}OK:${colors.reset} ${testFile}`);
      passCount++;
    } else {
      const failedCount = results.filter(r => !r.passed).length;
      const totalCount = results.length;
      console.log(`${colors.red}${colors.bright}FAIL:${colors.reset} ${testFile} ${colors.dim}[${failedCount}/${totalCount} assertions failed]${colors.reset}`);
      printAssertionFailures(testFile, sourceLines, results);
      failCount++;
    }
  }

  console.log(`\n${colors.bright}Test Results:${colors.reset}`);
  console.log(`  ${colors.green}✓ ${passCount} passed${colors.reset}`);
  if (failCount > 0) {
    console.log(`  ${colors.red}✗ ${failCount} failed${colors.reset}`);
  }
  if (noExpectationsCount > 0) {
    // FIXME: Maybe a better unicode?
    console.log(`  ${colors.yellow}⊘ ${noExpectationsCount} without expectations${colors.reset}`);
  }
  console.log(`\nTotal: ${passCount + failCount} tests`);
  process.exit(failCount > 0 ? 1 : 0);
}

runTests().catch(error => {
  console.error(`${colors.red}${colors.bright}ERROR:${colors.reset} ${error.message}`);
  console.error(error.stack);
  process.exit(1);
});
