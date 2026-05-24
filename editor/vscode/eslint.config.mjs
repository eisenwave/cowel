import tseslint from 'typescript-eslint';
import { defineConfig } from "eslint/config";
import parser from '@typescript-eslint/parser';
import plugin from '@typescript-eslint/eslint-plugin';
import stylistic from '@stylistic/eslint-plugin';
import { makeTsRules } from '../../tools/eslint-shared-rules.mjs';

export default defineConfig([
    {
        ignores: [
            "out",
            "node_modules",
        ]
    },
    ...tseslint.config({
        languageOptions: {
            parser,
            parserOptions: {
                project: [
                    './tsconfig.json',
                    './tests/tsconfig.json',
                ],
            },
        },
        plugins: {
            '@typescript-eslint': plugin,
            '@stylistic': stylistic,
        },
        files: [
            "src/**/*.ts",
            "tests/**/*.ts",
        ],
        rules: makeTsRules(plugin),
    }),
]);
