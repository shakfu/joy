# Modernization Review

This document summarizes opportunities to modernize and restructure the Joy NOBDW interpreter for current best practices.

## Testing & CI Automation

- `test2/CMakeLists.txt` and `test4/CMakeLists.txt` only create ad-hoc custom targets that dump `.out` files beside the sources; nothing asserts on the output, files are generated inside the repo, and false positives are common.
- `CMakeLists.txt` disables `RUN_TESTS` for the default Release build, and `.github/workflows/cmake.yml` comments out the `ctest` step, so CI never executes the interpreter.
- Convert the Joy suites into proper `add_test` rules that compare against golden data (or self-assert within Joy), emit artifacts under the binary tree, and run under `ctest`. Enable `RUN_TESTS` for CI builds, expand the workflow into a matrix (Linux/macOS/Windows), and include sanitizer and coverage jobs for better safety signals.

## Developer Experience & Distribution

- The README requires manual copying of `lib/usrlib.joy` into `$HOME/usrlib` and editing `inilib.joy`. Provide `cmake --install` rules (`install(TARGETS joy)`, `install(DIRECTORY lib/ DESTINATION share/joy)`), document environment variables for locating Joy libraries, and ship binary artifacts from CI.
- Add `CMakePresets.json`, a `justfile` or `Makefile` wrapper for common workflows, and a devcontainer or Dockerfile to lower onboarding friction.

## Quality Gates & Tooling

- Beyond compiler warnings, there is no formatter, linter, or static analysis configuration. Introduce `.clang-format`, `clang-tidy`, or `cppcheck` targets, run ASan/UBSan/Valgrind configurations in CI, and consider libFuzzer harnesses for the parser and garbage collector to catch memory issues early.
- Document coding standards and testing expectations in `AGENTS.md`/`README.md`, and enforce them through pre-commit hooks or CI checks.
