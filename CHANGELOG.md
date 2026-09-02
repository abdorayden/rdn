# Changelog

## v2.0.0 (2026-08-29)

### Added
- `module` keyword to define named (and nested `::`-qualified) modules, with
  duplicate-definition rejection and balanced-body parsing
- `open` keyword to import a full module or a single member into scope
  (`open Module` / `Module::member`)
- `dbg` / `???` operator that dumps the whole runtime stack for debugging
- Type aliases in modules, letting `defun` signatures be self-describing
- `eprint` / `eprintln` / `eprintf` / `eprintfn` stderr output family
  (`libs/io.rdn`) and the `printf` / `printfn` family
- New standard library modules:
  - `libs/base64.rdn` — Base64 encoding/decoding
  - `libs/integer.rdn` — integer parsing, binary conversion, bit helpers
  - `libs/log.rdn` — Go-style logging with prefixes, flags and severity
  - `libs/map.rdn` — hash-map data structure (+ `nativelibs/map.c`)
  - `libs/queue.rdn` — FIFO queue
  - `libs/regex.rdn` — pure-Raden regex-style string matcher
  - `libs/time.rdn` — UTC time helpers (now, hms, ymd, iso, clocks, sleep)
  - `libs/std.rdn` — loads all standard modules at once
- Extensions to existing modules:
  - `math` — wrapped in the `Math` module; 13 numeric constants (`PI`, `E`, ...)
  - `strings` — `concat`, `concat-by`, `contains`, `rev`, `split`,
    `replace-all`, `has-prefix`, `has-suffix`, `trim-space`
  - `lists` — `extends`; functions namespaced under `Lists`
  - `files` — `read-text` / `write-text`, `read-lines` / `write-lines` /
    `append-lines`, and a descriptor-based `File` API (`fopen`, `fread`,
    `fwrite`, `fclose`, `fseek`, std in/out/err pointers, `EOF`)
  - `os` — `system`, `platform`, `get-pid`, `getenv`, `setenv`, `env`,
    `get-cwd`, `cd`, `clock`, `random`
  - `strconv` — `atoi`, `atof`, `itoa`, `parse-bool`, `format-bool` and the
    `Ascii` submodule (`chr`, `ord`)
  - `net` — new cross-platform `Net::Socket` module (TCP/UDP/ICMP/RAW) with
    `socket_new`, `connect`, `bind`, `listen`, `accept`, `send`, `recv`,
    `sendto`, `recvfrom`, `close`, and socket-option setters
- Native libraries: `nativelibs/map.c`

### Changed
- Realm-based (arena / region) memory management (`src/arena.h`) replaces
  per-object `free`; the OS reclaims all memory on exit
- Variable and function bindings moved from linear lists to a hash table
  (`src/ht.h`) with proper lexical scope shadowing/unshadowing
- Independent C embedding API: `include/rdn.h` no longer depends on
  `stack.h` / `ht.h` / `arena.h`; the API is fully `rdn_` / `RDN_` prefixed
  and `main.c` is now a thin driver that includes `src/rdn.c`
- `build.sh` builds `librdn.a` and `librdn.so` from `src/rdn.c`; installs the
  libraries to `$LIBDIR` and the API headers to `$INCLUDEDIR` for global
  linking; adds a dedicated `libs` build target
- Embed examples moved from `embed/` to `examples/embed/`; Emacs and Vim/Neovim
  plugins consolidated under `editor/`
- Test harness rewritten in Python (`test/run.py`)

### Fixed
- Format-string vulnerability when raising user-controlled error messages
- Buffer overflow in the `open` module's selective import
- `net.c` integer truncation in `send` / `sendto` on large buffers
- `bint.c` memory leak when `realloc` failed
- `files.c` null checks, corrected stack indexes in `closefd`, and a
  `size_t` read-count overflow
- `rdn.c` NULL dereference guards and `match_pattern` rewritten iteratively to
  avoid stack overflow on long patterns
- `net.c` Windows `resolve_host` wrong `memcpy` size

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
