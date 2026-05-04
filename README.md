[![CMake build status][badge-cmake]][build-cmake]
[![clang-format build status][badge-format]][build-format]
[![codecov][badge-codecov]][codecov]

# Compact Web Language (COWEL)

COWEL is a markup language with TeX-like syntax,
intended to generate HTML documents,
mainly for proposals and technical text.
Many of its features are purpose-built for use in WG21,
such as for writing C++ proposals.

See [GitHub pages for documentation](https://cowel.org).

Get support on [our official Discord server][discord].

## Local LLVM Coverage (VS Code Coverage Gutters)

Use the dedicated coverage script to generate LCOV data
for the native C++ tests.

```bash
bash tools/coverage-llvm.sh
```

The generated report is written to:

```text
build/clang20-coverage/coverage/lcov.info
```

In VS Code with Coverage Gutters installed,
Coverage Gutters will pick this up automatically
when `coverage-gutters.coverageBaseDir` points to that directory.

The CI workflow in [.github/workflows/coverage.yml](.github/workflows/coverage.yml)
also uploads this LCOV report to Codecov,
and stores the LCOV and profdata files as workflow artifacts.

[build-cmake]: https://github.com/eisenwave/cowel/actions/workflows/cmake-multi-platform.yml/
[badge-cmake]: https://github.com/eisenwave/cowel/actions/workflows/cmake-multi-platform.yml/badge.svg
[build-format]: https://github.com/eisenwave/cowel/actions/workflows/clang-format.yml/
[badge-format]: https://github.com/eisenwave/cowel/actions/workflows/clang-format.yml/badge.svg
[codecov]: https://codecov.io/github/eisenwave/cowel
[badge-codecov]: https://codecov.io/github/eisenwave/cowel/graph/badge.svg?token=4TGUMSCXYJ
[discord]: https://discord.gg/fx8r5mP3Y9
