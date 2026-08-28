# Raden nvim plugin upgrade — plan (pure VimL)

The current `nvim/` runtime dir has working `ftdetect`/`syntax`/`indent`/`ftplugin`, plus a stray Lua placeholder. Extend it into a full plugin: run+quickfix, better syntax, block-aware indent, completion, tags, folding, and help. All Vim script; works in Vim and Neovim.

## File plan

### 1. `nvim/autoload/raden.vim` (new) — core logic
- `raden#exec()` — interpreter path: `g:raden_exec` if set, else `rdn` if `executable('rdn')`, else repo-local `./main`.
- `raden#run(extra)` — `:cgetexpr system(exec . ' ' . shellescape(@%) . ' 2>&1')`, `:copen` on non-empty; returns `v:shell_error`.
- `raden#eval_selection(lines)` — write selection to `tempname()`, run, `:cexpr` output, `delete()` temp.
- `raden#tagfunc(pattern, flags, info)` — scan open `*.rdn` buffers for `name defun`, `Name module`, `@name apply`; enables `:tag`/`CTRL-]`.
- `raden#complete(findstart, base)` — omnifunc: keywords + builtins + `Module::member` names from buffers + buffer-local `defun`/`let` identifiers.
- `raden#fold(lnum)` — fold expr: counts `if loop defun module apply` vs `end` / `else` per line, ignoring comments `[*...*]` and strings.
- Shared helper `s:strip()` — blank out comments/strings for keyword counting.

### 2. `nvim/compiler/raden.vim` (new)
- `CompilerSet makeprg=rdn\ %`
- `CompilerSet errorformat='%f:%l:%c:\ error:\ %m','%f:%l:%c:\ note:\ %m','%-G%.%#'` — matches rdn's real `file:line:col: error/note:` output; catch-all ignores source/`^^^` context lines, so `:make` also works.

### 3. plugin + ftplugin wiring
- `plugin/raden.vim` commands (buffer-local): `:RadenRun [args]`, `:RadenEval` (range = visual selection), `:RadenTags`.
- `ftplugin/raden.vim`: `setlocal foldmethod=expr foldexpr=raden#fold(v:lnum) foldtext=...`, `setlocal tagfunc=raden#tagfunc completefunc=raden#complete`, `g:raden_enable_fold` gate (default on). No mappings unless `g:raden_mappings` opt-in (`<localleader>r`, `<localleader>e`).

### 4. `nvim/syntax/raden.vim` (upgrade — mostly complete already)
- Add `syntax match radenTag /@\w\+/` (`@print`, `@println`, generated `@name apply`).
- Add module path `radenModulePath /\w\+\::\w\+/` (`Base64::b64decode`).
- Add type-name words `Int Str Bool Real List Null` (signed `defun` sigs).
- Existing keywords/builtins/numbers/escapes verified accurate against `src/rdn.c` token list.

### 5. `nvim/indent/raden.vim` (rewrite)
- Block-aware: nets `if/loop/defun/module/apply` openers vs `end` (`else` no change) across preceding lines, stripping `[*...*]` and strings; handles wrapped lines; `else`/`end` dedent one level.

### 6. `nvim/doc/raden.txt` (new)
Install (runtimepath/pack), commands, mappings, options, quickfix/errorformat notes, folding.

### 7. Remove `nvim/lua/plugin/raden.lua`
The `print("hello")` placeholder would fire on every Neovim load.

## Verification
- `nvim -u NONE -i NONE --cmd 'set rtp+=/path/nvim' examples/base64.rdn` + `:RadenRun` — empty quickfix, exit 0.
- Introduce a temp error file — `:RadenRun` populates quickfix with the exact `file:line:col: error:` line; test `:make` path too.
- `:syntax on`, fold test `zc/zo`, `:tag Base64`, completion via `<C-x><C-u>`.
- Confirm Vim 9 compat with `vim -Nu NONE` for the pure VimL files.
