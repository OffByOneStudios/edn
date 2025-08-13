# Contributing

Thanks for your interest in improving `edn`.

## Workflow
1. Fork the repository
2. Create a feature branch: `git checkout -b feature/short-description`
3. Make changes (add/update tests where possible)
4. Run the test suite (`cmake -S . -B build -DEDN_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build`)
5. Open a Pull Request with a clear description and rationale

## Coding Guidelines
- C++17
- Header-only design (keep heavy implementation details isolated in headers; avoid unnecessary templates)
- Keep patches focused; unrelated refactors should be separate PRs
- Prefer small helpers over large monolithic functions

## Parser / AST Conventions
- Each syntax form is represented as a `node` whose variant holds structural data
- Source position metadata keys: `line`, `col`, `end-line`, `end-col`
- Additional metadata should use lowercase dash-separated keys

## Testing
- Add regression tests for bug fixes
- For new syntactic features add round-trip parse + selected transformation or metadata assertions

## Versioning
- Follow semantic versioning once API stabilizes; increment minor for additive changes and patch for bug fixes

## Style
- Keep consistent indentation (spaces) & brace placement as existing code
- Avoid global `using namespace` in headers

## License
By contributing you agree that your contributions are licensed under the MIT license of the project.
