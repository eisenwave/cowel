import { computeFoldingRanges, FoldRange } from "../src/folding";

const colors = {
    reset: "\x1b[0m",
    bright: "\x1b[1m",
    red: "\x1b[91m",
    green: "\x1b[92m",
} as const;

interface TestCase {
    name: string;
    input: string;
    expected: FoldRange[];
}

const tests: TestCase[] = [
    {
        name: "single-line directive does not fold",
        input: "\\h4{Lazy processing}",
        expected: [],
    },
    {
        name: "single-line directive with args does not fold",
        input: "\\code(js){f}",
        expected: [],
    },
    {
        name: "multi-line directive folds",
        input: "\\h4{\nSome content\n}",
        expected: [[0, 1]],
    },
    {
        name: "codeblock with inner braces folds to outer closing brace",
        input: [
            "\\codeblock(js){",
            "function f(x) {",
            "    console.log(x);",
            "}",
            "f(2 + 2);",
            "}",
        ].join("\n"),
        expected: [[0, 4]],
    },
    {
        name: "nested directives produce nested folds",
        input: [
            "\\outer{",
            "\\inner{",
            "content",
            "}",
            "}",
        ].join("\n"),
        expected: [[1, 2], [0, 3]],
    },
    {
        name: "line comment braces are ignored",
        input: "\\: { open comment\n\\block{\ncontent\n}",
        expected: [[1, 2]],
    },
    {
        name: "block comment braces are ignored",
        input: "\\* { open comment *\\\n\\block{\ncontent\n}",
        expected: [[1, 2]],
    },
    {
        name: "escaped brace does not open a fold",
        input: "\\{ not a block\n\\block{\ncontent\n}",
        expected: [[1, 2]],
    },
    {
        name: "multi-line argument group folds",
        input: "\\d(\n  x = k{0}\n)",
        expected: [[0, 1]],
    },
    {
        name: "full issue #304 example",
        input: [
            "\\h4{Lazy processing}",
            "",
            "Unlike in typical programming languages,",
            "where the input to a function is processed at the call site and the resulting values",
            "are passed into the function,",
            `COWEL directives have \\em{absolute} control over their inputs and how they are processed.`,
            "",
            "Say you have a JavaScript function:",
            "\\codeblock(js){",
            "function f(x) {",
            "    console.log(x);",
            "}",
            "f(2 + 2);",
            "}",
            "\\code(js){f} has no idea that its input was originally \\code(js){2 + 2}.",
            "It only sees the value \\code(js){4} and prints it, \"oblivious to the outside\".",
        ].join("\n"),
        expected: [[8, 12]],
    },
];

let passed = 0;
let failed = 0;

for (const { name, input, expected } of tests) {
    const actual = computeFoldingRanges(input);
    const ok =
        actual.length === expected.length &&
        actual.every(([s, e], i) => s === expected[i][0] && e === expected[i][1]);

    if (ok) {
        console.log(`${colors.green}${colors.bright}OK:${colors.reset} ${name}`);
        passed++;
    } else {
        console.log(`${colors.red}${colors.bright}FAIL:${colors.reset} ${name}`);
        console.log(`  expected: ${JSON.stringify(expected)}`);
        console.log(`  actual:   ${JSON.stringify(actual)}`);
        failed++;
    }
}

console.log(`\n${passed + failed} tests: ${passed} passed, ${failed} failed`);
process.exit(failed > 0 ? 1 : 0);
