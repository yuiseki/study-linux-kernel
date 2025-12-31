# Repository Guidelines

## Project Structure & Module Organization
- `minishell/`: Minimal C shell implementation (pipe/redirect). Sources live in `minishell/src/` and build output is `minishell/minishell`.
- `minihttpd/`: Minimal HTTP server for socket learning. Sources in `minihttpd/src/`, build output is `minihttpd/minihttpd`.
- `scripts/observe/`: Helper scripts to capture and filter `strace` logs for learning (see `scripts/observe/README.md`).
- `artifacts/`: Generated outputs such as `strace` logs (created by scripts).
- `forks/`: Space for external or experimental code (not referenced by the build).

## Build, Test, and Development Commands
- `make` (in `minishell/`): Build the shell binary with debug flags.
- `make` (in `minihttpd/`): Build the HTTP server binary.
- `make clean` / `make fclean`: Remove object files / binary.
- `make re`: Clean + rebuild.
- `./minishell/minishell`: Launch REPL.
- `./minishell/minishell "echo hi | wc -c > /tmp/out"`: One-shot execution.
- `./minihttpd/minihttpd`: Start HTTP server on port 8080.
- `./minihttpd/minihttpd --trace`: Run HTTP server under `strace` and save logs.
- `./scripts/observe/strace_run.sh -- env -i PATH=/usr/bin:/bin ./minishell/minishell "..."`: Capture reproducible `strace` logs.

## Coding Style & Naming Conventions
- Language: C (gcc). Keep code warning-clean with `-Wall -Wextra -Werror`.
- Indentation: tabs are used in `minishell/src/*.c`.
- Naming: functions and variables use `snake_case`; typedefs and structs follow the existing `t_*` / `s_*` pattern.
- Keep helpers `static` where possible and prefer small, focused functions.

## Testing Guidelines
- No automated test framework currently. Validate changes by:
  - Running `./minishell/minishell` interactively.
  - Using `scripts/observe` to compare syscall behavior.
- If you add tests, document the commands and location in this file.

## Commit & Pull Request Guidelines
- Commit messages are short and direct (e.g., “Add README…”). Use a concise, present-tense summary.
- PRs should include: purpose, key changes, and a minimal manual test plan (commands + expected behavior). Link issues if applicable.

## Notes for Contributors
- `strace` output is sensitive to environment. Use `env -i PATH=/usr/bin:/bin` for stable logs.
- Prefer documenting any new scripts or workflows in the relevant `README.md` under `scripts/` or `minishell/`.
