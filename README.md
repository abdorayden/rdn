# What's Raden?

- Raden is a stack-based scripting language that is used for configuration and other purposes.
- It has a stack-based virtual machine and is easy to embed into other projects.
- You can create native functions in C and call them from within Raden.

```raden
2 2 +
print

true if
    "hello" print
end
```

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

If you load this repo directly as a local plugin, Lazy expects runtime directories at the repo root.

Use a local plugin spec like:

```lua
return {
  {
    dir = "/home/rayden/prog/raden/nvim", -- path to your repo dir
    name = "raden.nvim",
    ft = "raden",
  },
}
```

The repo now exposes these runtime directories at the root for Lazy compatibility:

- `ftdetect/`
- `ftplugin/`
- `indent/`
- `syntax/`

### Treesitter Scaffold

A starter Treesitter grammar scaffold is included under [`treesitter/raden/`](./treesitter/raden).

It is meant as a starting point for a real parser, not a finished grammar. The folder includes:

- `grammar.js`
- `package.json`
- `queries/highlights.scm`
