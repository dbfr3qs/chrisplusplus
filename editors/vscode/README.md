# chrisplusplus for VS Code

Language support for the **chrisplusplus** programming language (`.chr` files).

## Features

- **Syntax highlighting** — full TextMate grammar for all chrisplusplus syntax
- **Real-time diagnostics** — errors and warnings from the compiler as you type
- **Completions** — keywords, built-in types, built-in functions, and document symbols
- **Hover** — type information and function signatures on hover
- **Go to Definition** — jump to function, class, and variable definitions
- **Document Symbols** — outline view of functions, classes, interfaces, enums

## Requirements

The extension requires the `chris-lsp` language server binary. Build it from the compiler repo:

```bash
cd /path/to/chrisplusplus
cmake -B build
cmake --build build --target chris-lsp
```

The extension automatically searches for the binary in:
1. The `chrisplusplus.lspPath` setting (if configured)
2. `<workspace>/build/chris-lsp`
3. `<workspace>/cmake-build-debug/chris-lsp`
4. `<workspace>/cmake-build-release/chris-lsp`

## Development

```bash
cd editors/vscode
npm install
npm run compile
```

To test the extension, press **F5** in VS Code to launch an Extension Development Host.

## Extension Settings

| Setting | Default | Description |
|---------|---------|-------------|
| `chrisplusplus.lspPath` | `""` | Path to the `chris-lsp` binary |
| `chrisplusplus.trace.server` | `"off"` | Trace LSP communication for debugging |
