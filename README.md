# Raden

Raden is a small stack-based scripting language.

It is built around a simple value stack, a small interpreter written in C, and a native module API for extending the language from shared libraries.

## Build

Build the interpreter:

```sh
make
```

Run a script:

```sh
./main MAIN.rdn
```

Run the test suite:

```sh
bash test/run.sh
```

Build a native module manually:

```sh
gcc -Wall -Wextra -Werror -ggdb -std=c11 -fPIC -shared nativelibs/files.c -I. -o nativelibs/files.so
```

## Language Basics

Raden is postfix and stack-based. Values are pushed first, then operations consume them.

Example:

```raden
2 3 + print
```

This pushes `2`, pushes `3`, applies `+`, then prints the result.

### Values

Raden supports:

- integers
- doubles
- strings
- booleans
- lists
- identifiers

Examples:

```raden
10
2.5
"hello"
true
(1 2 "x")
```

### Variables

Use `let` to bind a value:

```raden
10 n let
n print
```

Use `set` to update an existing variable:

```raden
n 1 + n set
```

Use `const` for constants:

```raden
3 limit const
```

### Control Flow

`if` consumes a boolean:

```raden
true if
    "yes" print
else
    "no" print
end
```

`loop` repeats while the condition value at the end of the body stays true:

```raden
0 i let
i 5 < cond let

cond loop
    i print "\n" print
    i 1 + i let
    i 5 < cond set
    cond
end
```

### Functions

Define a function with `defun` and call it with `call`:

```raden
hello defun
    "hello\n" print
end

hello call
```

### Builtins

Common builtins include:

- arithmetic: `+ - * /`
- comparisons: `< > <= >= = !=`
- boolean ops: `! | &`
- bitwise ops: `<< >> ^`
- stack ops: `pop swap dup`
- variables: `let set const`
- conversion: `type to_string`
- containers: `append index remove len`
- loading: `load loadnative`

## Lists And Strings

Lists are written with parentheses:

```raden
(1 2 3)
```

You can index, remove, append, and get length:

```raden
(10 20 30) 1 index print "\n" print
(1 2 3) 1 remove print "\n" print
(1 2) 3 append print "\n" print
(1 2 3) len print
```

The same sequence builtins also work with strings:

```raden
"abcd" 1 index print "\n" print
"abcd" 1 remove print "\n" print
"ab" "cd" append print "\n" print
"hello" len print
```

For string variables, `append` and `remove` mutate the variable in place:

```raden
"hi" text let
text "!" append
text print "\n" print

text 1 remove
text print
```

## argv
```raden

[* access global variable *]

__argv print [* (script.rdn foo bar) *]

```

## enumerations
```raden

enum ONE const      [* ONE == 0 *]
enum TWO const      [* TWO == 1 *]
enum THREE const    [* THREE == 2 *]
reset [* reset the enum to 0 again *]

```

## Example Script

[`MAIN.rdn`](./MAIN.rdn) shows several language features together:

- loops
- lists
- string manipulation
- `loadnative`
- file IO via a native module

Example from it:

```raden
"./nativelibs/files.so" loadnative
"./Makefile" readLines call lines let

lines len print
"\n" print
lines 0 index print
```

The result of `readLines` is a normal Raden list, so you treat it with `len`, `index`, `append`, and `remove` just like any other list.

## Loading Scripts

Use `load` to execute another `.rdn` file:

```raden
"./libs/math.rdn" load
2 6 pow call print
```

Paths are resolved relative to the currently running source file.

## Native Modules

Use `loadnative` to load a shared library:

```raden
"./nativelibs/files.so" loadnative
"./nativelibs/math.so" loadnative
```

Native functions are registered by name, then called with normal `call` syntax:

```raden
"./nativelibs/files.so" loadnative
"./Makefile" readLines call print
```

## Native Module API

The public header is [`include/rdn_native.h`](./include/rdn_native.h).

A module must export:

```c
bool rdn_module_init(RDNModule *module);
```

Inside `rdn_module_init`, register native functions:

```c
if (!module->register_function(module, "myFunc", myFunc)) {
    return false;
}
```

Native functions use this signature:

```c
static bool myFunc(RDNApi *api);
```

### Stack Indexing

The API uses stack indices:

- `-1` means top of stack
- `-2` means one below top
- positive indices start at `1`

### Scalar API

Available helpers include:

- `api->stack_size(api)`
- `api->type(api, index)`
- `api->to_integer(api, index, &out)`
- `api->to_number(api, index, &out)`
- `api->to_boolean(api, index, &out)`
- `api->to_string(api, index)`
- `api->pop(api, count)`
- `api->push_integer(api, value)`
- `api->push_number(api, value)`
- `api->push_boolean(api, value)`
- `api->push_string(api, value)`
- `api->raise_error(api, message)`

### List API

The API also supports list creation and mutation:

- `api->push_list(api)`
- `api->list_len(api, index, &len)`
- `api->list_append(api, list_index, value_index)`
- `api->list_index(api, list_index, item_index)`
- `api->list_remove(api, list_index, item_index)`

`list_index` pushes a cloned item onto the stack.

`list_append` clones the source value before inserting it.

## Native Module Example

Minimal example:

```c
#include "../include/rdn_native.h"

static bool native_add(RDNApi *api) {
    long left = 0;
    long right = 0;

    if (api->stack_size(api) < 2) {
        return api->raise_error(api, "native_add requires 2 operands");
    }

    if (!api->to_integer(api, -2, &left) || !api->to_integer(api, -1, &right)) {
        return api->raise_error(api, "native_add requires integers");
    }

    if (!api->pop(api, 2)) {
        return false;
    }

    return api->push_integer(api, left + right);
}

bool rdn_module_init(RDNModule *module) {
    return module->register_function(module, "native_add", native_add);
}
```

There is a fuller example in [`test/native/native_test_module.c`](./test/native/native_test_module.c).

## Implementing `readLines`

`readLines` in [`nativelibs/files.c`](./nativelibs/files.c) is a good reference for returning a list from native code:

1. read the file
2. `push_list`
3. `push_string` for each line
4. `list_append` into the list
5. `pop` the temporary pushed string

That pattern is also what you use for functions like `split`, `glob`, or directory listing APIs.

## Embedding Raden

The current embedding entry point is:

```c
int rdn_main(int argc, char **argv);
```

It is declared in [`include/src.h`](./include/src.h).

At the moment, embedding is very lightweight and file-oriented: you invoke `rdn_main` with a script path just like the `main` executable does.

Example host program:

```c
#include "./include/src.h"

int main(void) {
    char *argv[] = {
        "host",
        "./MAIN.rdn",
    };

    return rdn_main(2, argv);
}
```

Current embedding notes:

- the public embedding surface is still minimal
- `rdn_main` executes a file path
- native modules are loaded with `loadnative`
- source-relative `load` and `loadnative` path resolution already works in the interpreter

If you want a stronger embedding API later, the next logical step is exposing helpers like:

- `rdn_eval_file(...)`
- `rdn_eval_string(...)`
- `rdn_state_new(...)`
- `rdn_state_free(...)`

## Project Layout

- [`main.c`](./main.c): thin entry point
- [`src/src.c`](./src/src.c): interpreter implementation
- [`include/src.h`](./include/src.h): internal interpreter declarations
- [`include/rdn_native.h`](./include/rdn_native.h): native module API
- [`nativelibs/`](./nativelibs): native module examples
- [`libs/`](./libs): Raden library scripts
- [`test/`](./test): regression tests

## Neovim Support

This repo includes a simple Neovim runtime package for `.rdn` files under [`nvim/`](./nvim):

- file detection
- syntax highlighting
- ftplugin defaults
- basic indent rules

To use it locally:

```sh
mkdir -p ~/.config/nvim
cp -r ./nvim/* ~/.config/nvim/
```

### LazyVim / lazy.nvim

Use a local plugin spec like:

```lua
return {
  {
    dir = "/home/rayden/prog/raden/nvim",
    name = "raden.nvim",
    ft = "raden",
  },
}
```

The repo exposes these runtime directories at the root for Lazy compatibility:

- `ftdetect/`
- `ftplugin/`
- `indent/`
- `syntax/`

## Treesitter Scaffold

A starter Treesitter grammar scaffold is included under [`treesitter/raden/`](./treesitter/raden).

It is a starting point, not a finished grammar.
