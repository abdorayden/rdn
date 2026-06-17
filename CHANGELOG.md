# Changelog

## v1.1.0 (2026-06-17)

### Added
- `ret` keyword for early return from functions
- `do_file` / `do_string` for runtime file/string execution
- `error` keyword to raise errors from scripts
- `unlet` keyword to remove unused variables
- `__line_col`, `__file`, `__func_name`, `__stack_size` introspection builtins
- Advanced `enum` with optional starting value
- Macro expansions via `demac` keyword
- `assert` keyword with type checking module (`libs/typecheck.rdn`)
- `Func` and `Any` types in type checker
- `?`, `@`, `!`, `#`, and leading digits allowed in identifiers
- Bootstrap install script (`install`) and `build.sh`
- Emacs highlight mode (`emacs/rdn-mode.el`)
- PDF manual (`doc/tex/manual.tex`)
- Website documentation overhaul

### Fixed
- Functions that consume stack items (via `let`) no longer leave dangling
  `VALUE_AS_VAR` references on return

### Changed
- `libs/core.rdn` expanded with more utilities
- `nativelibs/files.c` extended with more file operations
- `libs/typecheck.rdn` — new built-in library
