import eslint from "@eslint/js";
import tseslint from 'typescript-eslint';
import globals from "globals";
import { defineConfig } from "eslint/config";
import parser from '@typescript-eslint/parser';
import plugin from '@typescript-eslint/eslint-plugin';
import stylistic from '@stylistic/eslint-plugin'

export default defineConfig([
    {
        ignores: [
            "build",
            "ulight",
            "node_modules",
        ]
    },
    ...tseslint.config({
        languageOptions: {
            parser,
            parserOptions: {
                project: './tsconfig.json',
            },
        },
        plugins: {
            '@typescript-eslint': plugin,
            '@stylistic': stylistic
        },
        files: [
            "src/main/ts/*.ts",
        ],
        rules: {
            ...plugin.configs['recommended'].rules,
            '@typescript-eslint/no-unused-vars': [
                'warn',
                {
                    caughtErrors: "none",
                    argsIgnorePattern: '^_',
                    varsIgnorePattern: '^_',
                }
            ],

            '@stylistic/brace-style': ['error', '1tbs'],
            '@stylistic/comma-dangle': ['error', 'always-multiline'],
            '@stylistic/comma-spacing': ['error'],
            '@stylistic/comma-style': ['error'],
            '@stylistic/dot-location': ['error', 'property'],
            '@stylistic/function-call-spacing': ['error'],
            '@stylistic/function-paren-newline': ['error'],
            '@stylistic/indent': ['error'],
            '@stylistic/quotes': ['error', 'double', {
                allowTemplateLiterals: "always"
            }],
            '@stylistic/semi': ['error', 'always'],
            '@stylistic/semi-spacing': ['error'],
            '@stylistic/semi-style': ['error'],
            '@stylistic/space-infix-ops': ['error'],
            '@stylistic/space-unary-ops': ['error'],
            '@stylistic/max-len': ['error', {
                code: 100,
                tabWidth: 4,
                comments: 80,
                ignoreTemplateLiterals: true,
            }],
            '@stylistic/no-tabs': ['error'],
            '@stylistic/no-trailing-spaces': ['error'],

            '@typescript-eslint/no-explicit-any': 'error',
            '@typescript-eslint/explicit-module-boundary-types': 'error',
            '@typescript-eslint/explicit-function-return-type': ['error',
                {
                    allowExpressions: false,
                    allowTypedFunctionExpressions: true,
                    allowHigherOrderFunctions: false,
                },
            ],
        },
    }),
    {
        languageOptions: {
            ecmaVersion: 2022,
            sourceType: 'module',
            globals: {
                ...globals.node
            }
        },
        files: [
            "src/main/js/*.js",
        ],
        plugins: {
            js: eslint
        },
        extends: [
            "js/recommended"
        ],
        rules: {
            "semi": "warn",
            "quotes": ["warn", "double"],
            "prefer-const": "warn",
            "no-unused-vars": "warn"
        },
    },
]);
