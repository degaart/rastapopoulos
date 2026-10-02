# Commit hook

Activate the shared pre-commit hook in each clone:

```sh
git config --local core.hooksPath .githooks
```

The hook runs `clang-format -i` on staged `.c`, `.cpp`, `.h`, and `.hpp`
files, then stages the formatted files. If a file is partially staged, this
also stages its other working-tree edits.
