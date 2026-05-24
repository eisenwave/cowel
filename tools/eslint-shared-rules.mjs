/**
 * Shared ESLint rules for the COWEL monorepo.
 *
 * This file contains no imports.
 * Rule objects are built by factory functions
 * so that each sub-package resolves ESLint plugin instances
 * from its own node_modules.
 */

/**
 * Build the TypeScript rule set.
 *
 * @param {import('@typescript-eslint/eslint-plugin').default} plugin
 * @returns {Record<string, unknown>}
 */
export function makeTsRules(plugin) {
    return {
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
    };
}

/** @type {Record<string, unknown>} */
export const jsRules = {
    "semi": "warn",
    "quotes": ["warn", "double"],
    "prefer-const": "warn",
    "no-unused-vars": "warn",
};
